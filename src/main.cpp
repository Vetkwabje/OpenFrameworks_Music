#include "ofMain.h"
#include "ofApp.h"

int main() {

    ofGLWindowSettings settings;

    settings.windowMode = OF_FULLSCREEN;

    auto window =
        ofCreateWindow(settings);

    ofRunApp(
        window,
        std::make_shared<ofApp>()
    );

    ofRunMainLoop();
}
