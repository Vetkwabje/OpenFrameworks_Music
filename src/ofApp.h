
#pragma once

#include "ofMain.h"
#include <vector>

class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void keyPressed(int key);

    ofSoundPlayer music;

    // Audio analysis
    std::vector<float> spectrumSmoothed;
    std::vector<float> circleSpectrum;

    float bass = 0.0f;
    float mids = 0.0f;
    float highs = 0.0f;
    float energy = 0.0f;
    float previousBass = 0.0f;
    float kickPulse = 0.0f;

    float getBandEnergy(int firstBin, int lastBin);

    // CircleWave settings
    static constexpr int BANDS = 32;
    static constexpr int DEPTH = 120;

    float minDepth = -8350.0f;
    float maxDepth = 700.0f;
    float dz = 30.0f;

    int shapeMode = 1;
    bool backgroundEnabled = true;

    struct CircleLayer {
        float z = 0.0f;
        float rotation = 0.0f;
        float rotationSpeed = 0.0f;
        float initialAlpha = 150.0f;
        float lineWeight = 2.0f;

        std::vector<ofVec3f> vertices;

        void initialize(float initialZ);
    };

    std::vector<CircleLayer> layers;

    // CircleWave helpers
    void createLayers();
    void updateLayers();
    void drawLayers();
    void drawLayer(CircleLayer& layer);
    void drawCircleWave(CircleLayer& layer);
    void drawCircleSegments(CircleLayer& layer);
    void drawCircleLine(CircleLayer& layer);

    float getSpectrumValue(int band) const;
    ofColor getBandColor(int band, float alpha) const;
    float getLayerAlpha(const CircleLayer& layer) const;

    ofEasyCam camera;
};
