
#include "AudioInputManager.h"
#include <algorithm>
#include <stdexcept>

AudioInputManager::AudioInputManager() {
    fft = ofxFft::create(
        FFT_SIZE,
        OF_FFT_WINDOW_HAMMING
    );

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
    if (key == OF_KEY_RETURN || key == OF_KEY_ENTER) {
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

    if (!soundStream.setup(settings)) {
        throw std::runtime_error(
            "Could not initialize the audio stream"
        );
    }

    streamRunning = true;
}

void AudioInputManager::audioIn(ofSoundBuffer& input) {
    
void AudioInputManager::audioIn(ofSoundBuffer& input) {
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
    if (streamRunning) {
        soundStream.stop();
        soundStream.close();
        streamRunning = false;
    }

    selectedDeviceIndex = -1;
}
