#ifndef TIME_H
#define TIME_H

namespace GameEngine {

class Time {
public:
    Time();
    ~Time();
    
    // Time system lifecycle
    bool initialize();
    void update();
    
    // Time queries
    float getDeltaTime() const { return deltaTime; }
    float getTotalTime() const { return totalTime; }
    float getFPS() const { return fps; }
    int getFrameCount() const { return frameCount; }
    
    // Time scale (for slow motion, etc.)
    float getTimeScale() const { return timeScale; }
    void setTimeScale(float scale) { timeScale = scale; }
    
    // Scaled time (affected by time scale)
    float getScaledDeltaTime() const { return deltaTime * timeScale; }
    float getScaledTotalTime() const { return scaledTotalTime; }
    
    // Frame rate limiting
    void setTargetFrameRate(int fps);
    int getTargetFrameRate() const { return targetFrameRate; }
    bool isFrameRateLimited() const { return targetFrameRate > 0; }
    
    // Performance tracking
    void beginFrame();
    void endFrame();
    
private:
    float deltaTime;
    float totalTime;
    float scaledTotalTime;
    float timeScale;
    float fps;
    int frameCount;
    int targetFrameRate;
    
    // Internal timing
    float lastFrameTime;
    float frameStartTime;
    float fpsUpdateTimer;
    int fpsFrameCount;
    
    // Platform-specific time functions
    float getCurrentTime() const;
    void limitFrameRate();
};

} // namespace GameEngine

#endif // TIME_H
