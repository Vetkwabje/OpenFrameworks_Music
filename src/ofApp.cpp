
#include "ofApp.h"

void ofApp::setup() {
    ofBackground(0);
    ofSetFrameRate(60);
    ofSetCircleResolution(48);

    music.load("You Rock My World.mp3");
    music.setLoop(true);
    music.play();

    // ofSoundGetSpectrum provides the spectrum directly,
    // so a second FFT is not necessary here.
    int numBins = 512;
    spectrumSmoothed.assign(numBins, 0.0f);

    bass = 0;
    mids = 0;
    highs = 0;
    energy = 0;
    previousBass = 0;
    kickPulse = 0;

    int count = 100;

    positions.resize(count);
    angles.resize(count);
    speeds.resize(count);
    distances.resize(count);
    particleSizes.resize(count);
    phases.resize(count);
    particleBands.resize(count);

    for (int i = 0; i < count; i++) {
        angles[i] = ofRandom(TWO_PI);

        // Individual orbital speeds and directions
        speeds[i] = ofRandom(0.002f, 0.009f)
                  * (ofRandom(1.0f) < 0.5f ? -1.0f : 1.0f);

        distances[i] = ofRandom(90, 390);
        particleSizes[i] = ofRandom(1.5f, 4.0f);
        phases[i] = ofRandom(TWO_PI);

        // Different particles respond to different frequencies
        particleBands[i] = i % 3;
    }
}

float ofApp::getBandEnergy(int firstBin, int lastBin) {
    float sum = 0;
    int count = 0;

    firstBin = ofClamp(firstBin, 0, (int)spectrumSmoothed.size() - 1);
    lastBin = ofClamp(lastBin, firstBin + 1,
                      (int)spectrumSmoothed.size());

    for (int i = firstBin; i < lastBin; i++) {
        sum += spectrumSmoothed[i];
        count++;
    }

    return count > 0 ? sum / count : 0;
}

void ofApp::update() {
    // Read the actual audio spectrum
    float* spectrum = ofSoundGetSpectrum(
        (int)spectrumSmoothed.size()
    );

    // Smooth the spectrum to avoid jittery particles
    for (int i = 0; i < spectrumSmoothed.size(); i++) {
        float input = spectrum[i];

        spectrumSmoothed[i] =
            spectrumSmoothed[i] * 0.78f
            + input * 0.22f;
    }

    // Separate the spectrum into frequency bands.
    // These are spectrum-bin ranges, not fixed Hz boundaries.
    float rawBass  = getBandEnergy(1, 8);
    float rawMids  = getBandEnergy(8, 45);
    float rawHighs = getBandEnergy(45, 160);

    // Boost small spectrum values into a more visible range.
    rawBass  = ofClamp(rawBass * 18.0f, 0.0f, 1.0f);
    rawMids  = ofClamp(rawMids * 22.0f, 0.0f, 1.0f);
    rawHighs = ofClamp(rawHighs * 30.0f, 0.0f, 1.0f);

    // Smooth the bands independently
    bass  = bass  * 0.82f + rawBass  * 0.18f;
    mids  = mids  * 0.85f + rawMids  * 0.15f;
    highs = highs * 0.75f + rawHighs * 0.25f;

    energy = (bass + mids + highs) / 3.0f;

    // Detect sudden bass increases for a punchy kick response
    float bassRise = bass - previousBass;
    if (bassRise > 0.015f) {
        kickPulse = ofClamp(kickPulse + bassRise * 5.0f,
                            0.0f, 1.0f);
    }

    kickPulse *= 0.90f;
    previousBass = bass;

    float cx = ofGetWidth() * 0.5f;
    float cy = ofGetHeight() * 0.5f;

    float time = ofGetElapsedTimef();

    for (int i = 0; i < positions.size(); i++) {
        float bandResponse = 0;

        if (particleBands[i] == 0) {
            bandResponse = bass;
        } else if (particleBands[i] == 1) {
            bandResponse = mids;
        } else {
            bandResponse = highs;
        }

        // Mid frequencies accelerate orbital motion
        float speedMultiplier = 1.0f + mids * 2.0f;
        angles[i] += speeds[i] * speedMultiplier;

        // Bass expands the orbit, while mids add a second motion
        float wobble = sin(time * 1.8f + phases[i]) * (8.0f + highs * 14.0f);

        float radius = distances[i]
                     + bass * 130.0f
                     + mids * 45.0f
                     + bandResponse * 45.0f
                     + kickPulse * 35.0f
                     + wobble;

        positions[i].x = cx + cos(angles[i]) * radius;
        positions[i].y = cy + sin(angles[i]) * radius;
    }
}

void ofApp::drawGlowCircle(float x, float y, float radius,
                           ofColor color, float alpha) {
    // Three soft layers give a simple glow-like effect
    ofEnableBlendMode(OF_BLENDMODE_ADD);

    ofSetColor(color, alpha * 0.12f);
    ofDrawCircle(x, y, radius * 1.8f);

    ofSetColor(color, alpha * 0.22f);
    ofDrawCircle(x, y, radius * 1.3f);

    ofSetColor(color, alpha);
    ofDrawCircle(x, y, radius);

    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
}

void ofApp::draw() {
    // Fade the previous frame instead of clearing it.
    // This leaves smooth trails behind moving particles.
    ofSetColor(0, 0, 0, 32);
    ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

    float cx = ofGetWidth() * 0.5f;
    float cy = ofGetHeight() * 0.5f;

    float time = ofGetElapsedTimef();

    // Center responds primarily to bass and detected transients
    float coreRadius = 24.0f
                     + bass * 55.0f
                     + kickPulse * 28.0f;

    // Outer ring breathes more gently with the mids
    float ringRadius = coreRadius + 22.0f + mids * 30.0f;

    // Subtle outer halo
    drawGlowCircle(cx, cy, coreRadius,
                   ofColor(35, 115, 255),
                   90 + bass * 100.0f);

    // Bright inner core
    drawGlowCircle(cx, cy, coreRadius * 0.38f,
                   ofColor(100, 205, 255),
                   140 + kickPulse * 100.0f);

    // Rotating, audio-reactive orbital ring
    ofPushMatrix();
    ofTranslate(cx, cy);
    ofRotateDeg(time * 9.0f + mids * 25.0f);

    ofNoFill();
    ofSetLineWidth(1.2f + bass * 2.0f);
    ofSetColor(60, 150, 255, 45 + mids * 90.0f);
    ofDrawCircle(0, 0, ringRadius);

    // A second broken-looking ring made from short arcs
    ofSetColor(60, 210, 255, 70 + highs * 120.0f);
    ofSetLineWidth(1.0f);

    float arcRadius = ringRadius + 12.0f + highs * 10.0f;

    for (int i = 0; i < 4; i++) {
        float start = i * HALF_PI + 0.15f;
        float end = start + 0.48f + highs * 0.3f;

        ofPolyline arc;

        const int segments = 24;

        for (int j = 0; j <= segments; j++) {
            float t = (float)j / segments;
            float a = ofLerp(start, end, t);

            arc.addVertex(
                cos(a) * arcRadius,
                sin(a) * arcRadius,
                0
            );
        }

        arc.draw();
    }

    ofPopMatrix();
    ofFill();

    // Particles: each group reacts differently to the spectrum
    for (int i = 0; i < positions.size(); i++) {
        float bandResponse = 0;

        ofColor color;

        if (particleBands[i] == 0) {
            bandResponse = bass;
            color = ofColor(60, 140, 255);       // Bass: blue
        } else if (particleBands[i] == 1) {
            bandResponse = mids;
            color = ofColor(40, 220, 255);       // Mids: cyan
        } else {
            bandResponse = highs;
            color = ofColor(180, 230, 255);      // Highs: pale blue
        }

        float size = particleSizes[i]
                   + bandResponse * 4.0f
                   + kickPulse * 2.0f;

        float alpha = 130.0f + bandResponse * 125.0f;

        // A few brighter particles make the image pop
        if (i % 9 == 0) {
            size *= 1.5f;
            alpha = 255;
        }

        drawGlowCircle(positions[i].x, positions[i].y,
                       size, color, alpha);

        // Fine radial connections, more visible on stronger beats
        if (i % 5 == 0) {
            ofSetColor(color, 12 + energy * 45.0f);
            ofSetLineWidth(1.0f);
            ofDrawLine(cx, cy, positions[i].x, positions[i].y);
        }
    }

    // Tiny treble sparkles around the center
    ofEnableBlendMode(OF_BLENDMODE_ADD);

    for (int i = 0; i < 12; i++) {
        float a = time * (0.25f + i * 0.015f)
                + i * TWO_PI / 12.0f;

        float r = ringRadius + 20.0f
                + sin(time * 3.0f + i) * 8.0f
                + highs * 25.0f;

        float x = cx + cos(a) * r;
        float y = cy + sin(a) * r;

        float sparkleSize = 1.0f + highs * 3.0f;

        ofSetColor(130, 225, 255, 80 + highs * 170.0f);
        ofDrawCircle(x, y, sparkleSize);
    }

    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
}
