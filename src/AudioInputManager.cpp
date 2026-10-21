#include "AudioInputManager.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <pulse/stream.h>
#include <stdexcept>
#include <thread>

AudioInputManager::AudioInputManager() {
    fft.reset(ofxFft::create(FFT_SIZE, OF_FFT_WINDOW_HAMMING));

    fftInput.assign(FFT_SIZE, 0.0f);

    if (fft) {
        spectrum.assign(fft->getBinSize(), 0.0f);
    }

    status = "Press L to list audio input devices";
}

AudioInputManager::~AudioInputManager() {
    close();
}

void AudioInputManager::listDevices() {
    // Refresh the device list.
    ofSoundStream temporaryStream;
    auto devices = temporaryStream.getDeviceList();

    inputDevices.clear();

    ofLogNotice("AudioInputManager")
        << "Available audio input devices:";

    int displayIndex = 0;

    for (const auto& device : devices) {
        // Ignore output-only devices.
        if (device.inputChannels <= 0) {
            continue;
        }

        inputDevices.push_back(device);

        ofLogNotice("AudioInputManager")
            << "[" << displayIndex << "] "
            << device.name
            << " | ID: " << device.deviceID
            << " | Input channels: " << device.inputChannels;

        displayIndex++;
    }

    if (inputDevices.empty()) {
        status = "No audio input devices found";
        ofLogWarning("AudioInputManager") << status;
        return;
    }

    selectingDevice = true;
    typedNumber.clear();

    status = "Type a device number and press Enter";

    ofLogNotice("AudioInputManager")
        << "Type a device number and press Enter.";
}

void AudioInputManager::handleKey(int key) {
    if (key == 'l' || key == 'L') {
        listDevices();
        return;
    }

    if (!selectingDevice) {
        return;
    }

    // Enter: confirm the typed device index.
    if (key == OF_KEY_RETURN) {
        if (!typedNumber.empty()) {
            int index = ofToInt(typedNumber);
            selectDevice(index);
        }

        selectingDevice = false;
        typedNumber.clear();
        return;
    }

    // Backspace: remove the last digit.
    if (key == OF_KEY_BACKSPACE) {
        if (!typedNumber.empty()) {
            typedNumber.pop_back();
        }
        return;
    }

    // Accept numeric device indices.
    if (key >= '0' && key <= '9') {
        if (typedNumber.size() < 5) {
            typedNumber += static_cast<char>(key);
        }
    }
    status = "Select device number: " + typedNumber;
}

bool AudioInputManager::selectDevice(int index) {
    if (index < 0 ||
        index >= static_cast<int>(inputDevices.size())) {
        status = "Invalid device number. Press L to list devices.";

        ofLogWarning("AudioInputManager") << status;
        return false;
    }

    const auto device = inputDevices[index];

    close();

    try {
        startStream(device);

        selectedDeviceIndex = index;
        status = "Input: " + device.name;

        ofLogNotice("AudioInputManager")
            << "Selected input device [" << index << "]: "
            << device.name;

        return true;
    }
    catch (const std::exception& e) {
        status = "Failed to open device: " + device.name;

        ofLogError("AudioInputManager")
            << status << " (" << e.what() << ")";

        return false;
    }
}

bool AudioInputManager::startOutputMonitor() {
    close();

    pa_sample_spec sampleSpec;
    sampleSpec.format = PA_SAMPLE_FLOAT32LE;
    sampleSpec.rate = SAMPLE_RATE;
    sampleSpec.channels = 2;

    pa_buffer_attr bufferAttributes{};
    bufferAttributes.maxlength = static_cast<uint32_t>(-1);
    bufferAttributes.tlength = static_cast<uint32_t>(-1);
    bufferAttributes.prebuf = static_cast<uint32_t>(-1);
    bufferAttributes.minreq = static_cast<uint32_t>(-1);
    bufferAttributes.fragsize = BUFFER_SIZE * 2 * sizeof(float);

    int error = 0;
    auto* monitor = pa_simple_new(
        nullptr,
        "musicFFT",
        PA_STREAM_RECORD,
        "@DEFAULT_MONITOR@",
        "System output monitor",
        &sampleSpec,
        nullptr,
        &bufferAttributes,
        &error
    );

    if (!monitor) {
        status = "Failed to open system output monitor";
        ofLogError("AudioInputManager")
            << status << ": " << pa_strerror(error);
        return false;
    }

    outputMonitor = monitor;
    outputMonitorRunning = true;
    outputMonitorThread = std::thread([this, monitor]() {
        constexpr int frames = BUFFER_SIZE;
        std::vector<float> samples(frames * 2);
        int error = 0;

        while (outputMonitorRunning) {
            if (pa_simple_read(
                    static_cast<pa_simple*>(monitor),
                    samples.data(),
                    samples.size() * sizeof(float),
                    &error
                ) < 0) {
                ofLogError("AudioInputManager")
                    << "System output monitor read failed: "
                    << pa_strerror(error);
                break;
            }

            ofSoundBuffer buffer;
            buffer.allocate(samples.size(), 2);
            buffer.setSampleRate(SAMPLE_RATE);

            for (std::size_t i = 0; i < samples.size(); ++i) {
                buffer[i] = samples[i];
            }

            audioIn(buffer);
        }
    });

    status = "System output monitor";
    ofLogNotice("AudioInputManager")
        << "Selected system output monitor";
    return true;
}

void AudioInputManager::startStream(
    const ofSoundDevice& device
) {
    ofSoundStreamSettings settings;

    settings.setInDevice(device);
    settings.setInListener(this);

    settings.sampleRate = SAMPLE_RATE;
    settings.numInputChannels =
        std::min<unsigned int>(2, device.inputChannels);
    settings.numOutputChannels = 0;
    settings.bufferSize = BUFFER_SIZE;
    settings.numBuffers = 4;

    {
        std::lock_guard<std::mutex> lock(audioMutex);
        writeIndex = 0;
        std::fill(fftInput.begin(), fftInput.end(), 0.0f);
        std::fill(spectrum.begin(), spectrum.end(), 0.0f);
    }
    receivedAudio = false;

    if (!soundStream.setup(settings)) {
        throw std::runtime_error(
            "Could not initialize the audio stream"
        );
    }

    streamRunning = true;

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (!receivedAudio && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!receivedAudio) {
        close();
        throw std::runtime_error(
            "Audio stream opened but produced no input buffers"
        );
    }
}

void AudioInputManager::audioIn(ofSoundBuffer& input) {
    receivedAudio = true;

    if (!fft) {
        return;
    }

    const int channels = input.getNumChannels();
    const int frames = input.getNumFrames();

    if (channels <= 0 || frames <= 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(audioMutex);

    // Convert incoming audio to mono and write into a ring buffer.
    for (int i = 0; i < frames; i++) {
        float monoSample = 0.0f;

        for (int ch = 0; ch < channels; ch++) {
            monoSample += input[i * channels + ch];
        }

        monoSample /= channels;

        fftInput[writeIndex] = monoSample;
        writeIndex = (writeIndex + 1) % FFT_SIZE;
    }

    // Reorder the circular buffer into chronological order.
    std::vector<float> orderedInput(FFT_SIZE);

    for (int i = 0; i < FFT_SIZE; i++) {
        orderedInput[i] =
            fftInput[(writeIndex + i) % FFT_SIZE];
    }

    fft->setSignal(orderedInput.data());

    float* amplitudes = fft->getAmplitude();
    const int binCount = fft->getBinSize();

    if (amplitudes && binCount > 0) {
        spectrum.assign(amplitudes, amplitudes + binCount);
    }
}

std::vector<float> AudioInputManager::getSpectrum() {
    std::lock_guard<std::mutex> lock(audioMutex);
    return spectrum;
}

std::string AudioInputManager::getStatus() const {
    return status;
}

bool AudioInputManager::isSelectingDevice() const {
    return selectingDevice;
}

void AudioInputManager::close() {
    stopOutputMonitor();

    if (streamRunning) {
        soundStream.stop();
        soundStream.close();
        streamRunning = false;
    }

    selectedDeviceIndex = -1;
}

void AudioInputManager::stopOutputMonitor() {
    outputMonitorRunning = false;

    if (outputMonitorThread.joinable()) {
        outputMonitorThread.join();
    }

    if (outputMonitor) {
        pa_simple_free(static_cast<pa_simple*>(outputMonitor));
        outputMonitor = nullptr;
    }
}
