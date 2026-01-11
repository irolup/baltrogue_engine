#include "Core/Time.h"
#include "Platform.h"

namespace GameEngine {

Time::Time()
    : deltaTime(0.0f)
    , totalTime(0.0f)
    , scaledTotalTime(0.0f)
    , timeScale(1.0f)
    , fps(0.0f)
    , frameCount(0)
    , targetFrameRate(0)
    , lastFrameTime(0.0f)
    , frameStartTime(0.0f)
    , fpsUpdateTimer(0.0f)
    , fpsFrameCount(0)
{
}

Time::~Time() {
}

bool Time::initialize() {
    lastFrameTime = getCurrentTime();
    frameStartTime = lastFrameTime;
    return true;
}

void Time::update() {
    float currentTime = getCurrentTime();
    deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;
    
    totalTime = currentTime;
    scaledTotalTime += deltaTime * timeScale;
    frameCount++;
    
    fpsUpdateTimer += deltaTime;
    fpsFrameCount++;
    
    if (fpsUpdateTimer >= 1.0f) {
        fps = fpsFrameCount / fpsUpdateTimer;
        fpsUpdateTimer = 0.0f;
        fpsFrameCount = 0;
    }
}

void Time::setTargetFrameRate(int targetFps) {
    targetFrameRate = targetFps;
}

void Time::beginFrame() {
    frameStartTime = getCurrentTime();
}

void Time::endFrame() {
    if (targetFrameRate > 0) {
        limitFrameRate();
    }
}

float Time::getCurrentTime() const {
    return platformGetTime();
}

void Time::limitFrameRate() {
    if (targetFrameRate <= 0) return;
    
    float targetFrameTime = 1.0f / targetFrameRate;
    float currentFrameTime = getCurrentTime() - frameStartTime;
    
    if (currentFrameTime < targetFrameTime) {
        float sleepTime = targetFrameTime - currentFrameTime;
        platformSleep((int)(sleepTime * 1000000)); // Convert to microseconds
    }
}

} // namespace GameEngine
