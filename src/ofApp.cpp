
#include "ofApp.h"

void ofApp::setup() {
    ofSetFrameRate(60);
    ofSetCircleResolution(48);
    ofSetVerticalSync(true);
    ofSetSmoothLighting(false);
    ofEnableAlphaBlending();

    ofBackground(0);

    music.load("You Rock My World.mp3");
    music.setLoop(true);
    music.play();

    spectrumSmoothed.assign(512, 0.0f);
    circleSpectrum.assign(BANDS, 0.0f);

    bass = mids = highs = energy = 0.0f;
    previousBass = kickPulse = 0.0f;

    createLayers();

    // Camera looks through the tunnel along the Z axis.
    camera.setPosition(0, 0, 5000);
    camera.lookAt(ofVec3f(0, 0, -3500));
    camera.setNearClip(1.0f);
    camera.setFarClip(30000.0f);
    camera.setFov(60.0f);
}

void ofApp::CircleLayer::initialize(float initialZ) {
    z = initialZ;
    rotation = ofRandom(TWO_PI);
    rotationSpeed = ofRandom(-0.001f, 0.001f);
    initialAlpha = ofRandom(100.0f, 200.0f);
    lineWeight = ofRandom(1.0f, 2.0f);

    vertices.resize(BANDS);

    for (int j = 0; j < BANDS; ++j) {
        vertices[j].set(0, 0, 0);
    }
}

void ofApp::createLayers() {
    layers.clear();
    layers.resize(DEPTH);

    for (int i = 0; i < DEPTH; ++i) {
        float z = ofMap(
            i, 0, DEPTH,
            minDepth, maxDepth
        );

        layers[i].initialize(z);
    }
}

float ofApp::getBandEnergy(int firstBin, int lastBin) {
    if (spectrumSmoothed.empty()) {
        return 0.0f;
    }

    firstBin = ofClamp(
        firstBin, 0,
        static_cast<int>(spectrumSmoothed.size()) - 1
    );

    lastBin = ofClamp(
        lastBin, firstBin + 1,
        static_cast<int>(spectrumSmoothed.size())
    );

    float sum = 0.0f;
    int count = 0;

    for (int i = firstBin; i < lastBin; ++i) {
        sum += spectrumSmoothed[i];
        ++count;
    }

    return count > 0 ? sum / count : 0.0f;
}

float ofApp::getSpectrumValue(int band) const {
    if (band < 0 || band >= static_cast<int>(circleSpectrum.size())) {
        return 0.0f;
    }

    return circleSpectrum[band];
}

void ofApp::update() {
    // Read the spectrum from the currently playing audio.
    float* spectrum = ofSoundGetSpectrum(
        static_cast<int>(spectrumSmoothed.size())
    );

    for (int i = 0; i < static_cast<int>(spectrumSmoothed.size()); ++i) {
        spectrumSmoothed[i] =
            spectrumSmoothed[i] * 0.78f
            + spectrum[i] * 0.22f;
    }

    // Preserve the existing audio-reactive frequency bands.
    float rawBass  = getBandEnergy(1, 8);
    float rawMids  = getBandEnergy(8, 45);
    float rawHighs = getBandEnergy(45, 160);

    rawBass  = ofClamp(rawBass * 18.0f, 0.0f, 1.0f);
    rawMids  = ofClamp(rawMids * 22.0f, 0.0f, 1.0f);
    rawHighs = ofClamp(rawHighs * 30.0f, 0.0f, 1.0f);

    bass  = bass  * 0.82f + rawBass  * 0.18f;
    mids  = mids  * 0.85f + rawMids  * 0.15f;
    highs = highs * 0.75f + rawHighs * 0.25f;

    energy = (bass + mids + highs) / 3.0f;

    float bassRise = bass - previousBass;

    if (bassRise > 0.015f) {
        kickPulse = ofClamp(
            kickPulse + bassRise * 5.0f,
            0.0f, 1.0f
        );
    }

    kickPulse *= 0.90f;
    previousBass = bass;

    // Sample 32 values across the usable spectrum.
    for (int band = 0; band < BANDS; ++band) {
        int bin = ofMap(band, 0, BANDS - 1, 1, 180);
        float value = spectrumSmoothed[bin];

        circleSpectrum[band] =
            circleSpectrum[band] * 0.65f
            + value * 0.35f;

        circleSpectrum[band] =
            ofClamp(circleSpectrum[band] * 12.0f, 0.0f, 1.0f);
    }

    dz = ofMap(energy, 0.0f, 1.0f, 30.0f, 75.0f);

    updateLayers();
}

void ofApp::updateLayers() {
    for (auto& layer : layers) {
        layer.z += dz;
        layer.rotation += layer.rotationSpeed;

        // Respawn at the back, preserving any movement beyond maxDepth.
        if (layer.z > maxDepth) {
            float overshoot = layer.z - maxDepth;
            layer.initialize(minDepth + overshoot);
        }
    }
}

ofColor ofApp::getBandColor(int band, float alpha) const {
    // Each part of the spectrum contributes to a different color channel.
    float red   = ofMap(bass,  0.0f, 1.0f, 40.0f, 200.0f);
    float green = ofMap(mids,  0.0f, 1.0f, 40.0f, 200.0f);
    float blue  = ofMap(highs, 0.0f, 1.0f, 40.0f, 200.0f);

    // Slight variation along the ring adds a subtle color gradient.
    float phase = TWO_PI * band / BANDS;

    red   += 20.0f * sin(phase);
    green += 20.0f * sin(phase + TWO_PI / 3.0f);
    blue  += 20.0f * sin(phase + 2.0f * TWO_PI / 3.0f);

    return ofColor(
        ofClamp(red, 0.0f, 255.0f),
        ofClamp(green, 0.0f, 255.0f),
        ofClamp(blue, 0.0f, 255.0f),
        ofClamp(alpha, 0.0f, 255.0f)
    );
}

float ofApp::getLayerAlpha(const CircleLayer& layer) const {
    float depthFade = ofMap(
        layer.z, minDepth, maxDepth,
        1.0f, 0.4f, true
    );

    float alpha = layer.initialAlpha * depthFade;

    // The original sketch used overall energy to alter opacity.
    return alpha * ofMap(energy, 0.0f, 1.0f, 0.65f, 1.0f);
}

void ofApp::draw() {
    if (backgroundEnabled) {
        ofBackground(0);
    } else {
        // A low-alpha overlay creates trails.
        ofSetColor(0, 0, 0, 32);
        ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
    }

    camera.begin();
    drawLayers();
    camera.end();
}

void ofApp::drawLayers() {
    // Draw the distant layers first.
    for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
        drawLayer(layers[i]);
    }
}

void ofApp::drawLayer(CircleLayer& layer) {
    ofPushMatrix();

    ofTranslate(0, 0, layer.z);
    ofRotateZRad(layer.rotation);

    drawCircleWave(layer);

    ofPopMatrix();
}

void ofApp::drawCircleWave(CircleLayer& layer) {
    switch (shapeMode) {
        case 1:
            drawCircleLine(layer);
            break;

        case 2:
            drawCircleSegments(layer);
            break;

        case 3: {
            // Square-like radial waveform.
            ofNoFill();
            ofSetLineWidth(layer.lineWeight);

            for (int j = 0; j < BANDS; ++j) {
                float angle = TWO_PI * j / BANDS;
                float spectrumValue = getSpectrumValue(j);

                float side = 1500.0f
                    * ofMap(spectrumValue, 0.0f, 1.0f, 2.0f, 1.2f);

                float maxComponent = std::max(
                    std::abs(cos(angle)),
                    std::abs(sin(angle))
                );

                float radius = side / std::max(maxComponent, 0.001f);

                layer.vertices[j].set(
                    radius * cos(angle),
                    radius * sin(angle),
                    0
                );
            }

            ofBeginShape();
            for (int j = 0; j < BANDS; ++j) {
                ofSetColor(getBandColor(j, getLayerAlpha(layer)));
                ofVertex(layer.vertices[j]);
            }
            for (int j = 0; j < 3; ++j) {
                ofSetColor(getBandColor(j, getLayerAlpha(layer)));
                ofVertex(layer.vertices[j]);
            }
            ofEndShape(false);
            ofFill();
            break;
        }

        case 4: {
            // Two half-circles, producing the mirrored waveform.
            ofNoFill();
            ofSetLineWidth(layer.lineWeight);

            for (int half = 0; half < 2; ++half) {
                ofBeginShape();

                for (int j = 0; j <= BANDS / 2; ++j) {
                    int band = j % BANDS;
                    float angle = PI * j / (BANDS / 2) + half * PI;

                    float radius = ofMap(
                        getSpectrumValue(band),
                        0.0f, 1.0f,
                        3000.0f, 1400.0f
                    );

                    ofSetColor(getBandColor(band, getLayerAlpha(layer)));
                    ofVertex(
                        radius * cos(angle),
                        radius * sin(angle)
                    );
                }

                ofEndShape(false);
            }

            ofFill();
            break;
        }
    }
}

void ofApp::drawCircleLine(CircleLayer& layer) {
    ofNoFill();
    ofSetLineWidth(layer.lineWeight + energy * 2.0f);

    // Mode 1: circular outline, with stronger frequencies pulling
    // the points toward the center.
    for (int j = 0; j < BANDS; ++j) {
        float angle = TWO_PI * j / BANDS;
        float value = getSpectrumValue(j);

        float radius = 4000.0f;

        if (value > 0.01f) {
            radius = ofMap(value, 0.01f, 1.0f, 3600.0f, 2000.0f, true);
        }

        layer.vertices[j].set(
            radius * cos(angle),
            radius * sin(angle),
            0
        );
    }

    float alpha = getLayerAlpha(layer);

    ofBeginShape();
    for (int j = 0; j < BANDS; ++j) {
        ofSetColor(getBandColor(j, alpha));
        ofVertex(layer.vertices[j]);
    }

    // Close the curve.
    for (int j = 0; j < 3; ++j) {
        ofSetColor(getBandColor(j, alpha));
        ofVertex(layer.vertices[j]);
    }
    ofEndShape(false);

    ofFill();
}

void ofApp::drawCircleSegments(CircleLayer& layer) {
    // Mode 2: radial segments rather than a connected outline.
    float alpha = getLayerAlpha(layer);

    ofSetLineWidth(layer.lineWeight);

    for (int j = 0; j < BANDS; ++j) {
        float angle = TWO_PI * j / BANDS;
        float radius = ofMap(
            getSpectrumValue(j), 0.0f, 1.0f,
            2000.0f, 4000.0f
        );

        ofSetColor(getBandColor(j, alpha));
        ofDrawLine(
            radius * cos(angle),
            radius * sin(angle),
            (radius + 100.0f) * cos(angle),
            (radius + 100.0f) * sin(angle)
        );
    }
}

void ofApp::keyPressed(int key) {
    switch (key) {
        case '1':
        case '2':
        case '3':
        case '4':
            shapeMode = key - '0';
            break;

        case 'b':
        case 'B':
            backgroundEnabled = !backgroundEnabled;
            break;

        case ' ':
            if (music.isPlaying()) {
                music.stop();
            } else {
                music.play();
            }
            break;

        case 'r':
        case 'R':
            createLayers();
            break;
    }
}
