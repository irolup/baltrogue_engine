#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include "Core/ThreadManager.h"
#include "Platform.h"
#include <string>
#include <atomic>
#include <vector>
#include <mutex>

#ifdef LINUX_BUILD
#include <AL/al.h>
#include <AL/alc.h>
#elif defined(VITA_BUILD)
#include <psp2/audioout.h>
#endif

namespace GameEngine {

class SoundComponent;

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
    
    bool isInitialized() const { return initialized; }
    float getMasterVolume() const { return masterVolume; }
    
#ifdef VITA_BUILD
    void registerSoundComponent(SoundComponent* component);
    void unregisterSoundComponent(SoundComponent* component);
#endif
    
#ifdef LINUX_BUILD
    ALCdevice* getDevice() const { return alDevice; }
    ALCcontext* getContext() const { return alContext; }
#elif defined(VITA_BUILD)
#endif
    
private:
    AudioManager();
    ~AudioManager();
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    
    bool initialized;
    bool initializationAttempted;
    bool threadingEnabled;
    ThreadHandle audioThread;
    ThreadSafeQueue<AudioCommand> commandQueue;
    std::atomic<bool> audioThreadRunning;
    
    float masterVolume;
    bool paused;
    
#ifdef VITA_BUILD
    std::vector<SoundComponent*> activeSoundComponents;
    std::mutex soundComponentsMutex;
#endif
    
#ifdef LINUX_BUILD
    ALCdevice* alDevice;
    ALCcontext* alContext;
#elif defined(VITA_BUILD)
#endif
    
    void audioThreadFunction();
    void processAudioCommand(const AudioCommand& cmd);
    
    bool initializeAudioSystem();
    void shutdownAudioSystem();
};

} // namespace GameEngine

#endif // AUDIO_MANAGER_H

