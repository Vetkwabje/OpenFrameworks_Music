
#pragma once

#include "ofMain.h"
#include "ofxFft.h"

#include <mutex>
#include <memory>
#include <vector>

class AudioInputManager : public ofBaseSoundInput {
public:
    AudioInputManager();
    ~AudioInputManager();

    // List devices that support audio input.
    void listDevices();

    // Select a device by its displayed list number.
    bool selectDevice(int index);

    // Receive keyboard input for device selection.
    void handleKey(int key);

    // Return a thread-safe copy of the most recent FFT spectrum.
    std::vector<float> getSpectrum();

    // Status for the on-screen interface.
    std::string getStatus() const;
    bool isSelectingDevice() const;

    void close();

    // Called by openFrameworks on the audio thread.
    void audioIn(ofSoundBuffer& input) override;

private:
    ofSoundStream soundStream;
    std::vector<ofSoundDevice> inputDevices;

    std::shared_ptr<ofxFft> fft;

    std::vector<float> fftInput;
    std::vector<float> spectrum;

    std::mutex audioMutex;

    static constexpr int FFT_SIZE = 1024;
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int BUFFER_SIZE = 512;

    std::string typedNumber;
    std::string status;

    bool selectingDevice = false;
    bool streamRunning = false;

    int selectedDeviceIndex = -1;
    int writeIndex = 0;

    void startStream(const ofSoundDevice& device);
};
