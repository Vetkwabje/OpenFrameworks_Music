#pragma once

#include "ofMain.h"

class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();

    ofSoundPlayer music;

    // Audio analysis
    vector<float> spectrumSmoothed;

    float bass;
    float mids;
    float highs;
    float energy;
    float previousBass;
    float kickPulse;

    // Particle system
    vector<ofVec2f> positions;
    vector<float> angles;
    vector<float> speeds;
    vector<float> distances;
    vector<float> particleSizes;
    vector<float> phases;

    vector<int> particleBands;

    // Audio helper
    float getBandEnergy(int firstBin, int lastBin);

    // Visual helper
    void drawGlowCircle(float x, float y, float radius,
                        ofColor color, float alpha);
};