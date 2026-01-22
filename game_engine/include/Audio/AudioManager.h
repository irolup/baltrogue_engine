#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include "Core/ThreadManager.h"
#include <string>
#include <atomic>

namespace GameEngine {

enum class AudioCommandType {
    PLAY_SOUND,
    STOP_SOUND,
    SET_VOLUME,
    PAUSE,
    RESUME,
    SHUTDOWN
};

struct AudioCommand {
    AudioCommandType type;
    std::string soundPath;
    float volume;
    bool loop;
    void* data;
};

class AudioManager {
public:
    static AudioManager& getInstance();
    
    bool initialize();
    void shutdown();
    
    void enableThreading(bool enable);
    bool isThreadingEnabled() const { return threadingEnabled; }
    
    void playSound(const std::string& path, float volume = 1.0f, bool loop = false);
    void stopSound(const std::string& path);
    void setVolume(float volume);
    void pause();
    void resume();
    
private:
    AudioManager();
    ~AudioManager();
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    
    bool threadingEnabled;
    ThreadHandle audioThread;
    ThreadSafeQueue<AudioCommand> commandQueue;
    std::atomic<bool> audioThreadRunning;
    
    float masterVolume;
    bool paused;
    
    void audioThreadFunction();
    void processAudioCommand(const AudioCommand& cmd);
};

} // namespace GameEngine

#endif // AUDIO_MANAGER_H

