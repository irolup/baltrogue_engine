#include "Audio/AudioManager.h"
#include "Core/ThreadManager.h"
#include <iostream>
#include <cstring>

namespace GameEngine {

AudioManager& AudioManager::getInstance() {
    static AudioManager instance;
    return instance;
}

AudioManager::AudioManager()
    : initialized(false)
    , initializationAttempted(false)
    , threadingEnabled(false)
    , audioThreadRunning(false)
    , masterVolume(1.0f)
    , paused(false)
#ifdef LINUX_BUILD
    , alDevice(nullptr)
    , alContext(nullptr)
#endif
{
}

AudioManager::~AudioManager() {
    shutdown();
}

bool AudioManager::initialize() {
    if (initialized) {
        return true;
    }
    
    if (initializationAttempted && !initialized) {
        return false;
    }
    
    initializationAttempted = true;
    
    if (!initializeAudioSystem()) {
        std::cerr << "AudioManager: Failed to initialize audio system" << std::endl;
        return false;
    }
    
    initialized = true;
    return true;
}

void AudioManager::shutdown() {
    if (threadingEnabled && audioThreadRunning) {
        AudioCommand shutdownCmd;
        shutdownCmd.type = AudioCommandType::SHUTDOWN;
        commandQueue.push(shutdownCmd);
        
        ThreadManager::getInstance().joinThread(audioThread);
        audioThreadRunning = false;
        commandQueue.reset();
    }
    
    shutdownAudioSystem();
    initialized = false;
}

void AudioManager::enableThreading(bool enable) {
    if (threadingEnabled == enable) {
        return;
    }
    
    threadingEnabled = enable;
    
    if (enable && !audioThreadRunning) {
        audioThread = ThreadManager::getInstance().createThread(
            "AudioThread",
            [this]() { this->audioThreadFunction(); }
        );
        
        if (!ThreadManager::getInstance().isValid(audioThread)) {
            std::cerr << "AudioManager: Failed to create audio thread!" << std::endl;
            threadingEnabled = false;
        }
    } else if (!enable && audioThreadRunning) {
        AudioCommand shutdownCmd;
        shutdownCmd.type = AudioCommandType::SHUTDOWN;
        commandQueue.push(shutdownCmd);
        
        ThreadManager::getInstance().joinThread(audioThread);
        audioThreadRunning = false;
        commandQueue.reset();
    }
}

void AudioManager::playSound(const std::string& path, float volume, bool loop) {
    if (!threadingEnabled) {
        // Single-threaded mode
        return;
    }
    
    AudioCommand cmd;
    cmd.type = AudioCommandType::PLAY_SOUND;
    cmd.soundPath = path;
    cmd.volume = volume;
    cmd.loop = loop;
    cmd.data = nullptr;
    commandQueue.push(cmd);
}

void AudioManager::stopSound(const std::string& path) {
    if (!threadingEnabled) {
        return;
    }
    
    AudioCommand cmd;
    cmd.type = AudioCommandType::STOP_SOUND;
    cmd.soundPath = path;
    cmd.data = nullptr;
    commandQueue.push(cmd);
}

void AudioManager::setVolume(float volume) {
    if (!threadingEnabled) {
        masterVolume = volume;
        return;
    }
    
    AudioCommand cmd;
    cmd.type = AudioCommandType::SET_VOLUME;
    cmd.volume = volume;
    cmd.data = nullptr;
    commandQueue.push(cmd);
}

void AudioManager::pause() {
    if (!threadingEnabled) {
        paused = true;
        return;
    }
    
    AudioCommand cmd;
    cmd.type = AudioCommandType::PAUSE;
    cmd.data = nullptr;
    commandQueue.push(cmd);
}

void AudioManager::resume() {
    if (!threadingEnabled) {
        paused = false;
        return;
    }
    
    AudioCommand cmd;
    cmd.type = AudioCommandType::RESUME;
    cmd.data = nullptr;
    commandQueue.push(cmd);
}

void AudioManager::audioThreadFunction() {
    audioThreadRunning = true;
    
    while (audioThreadRunning) {
        AudioCommand cmd;
        if (commandQueue.pop(cmd)) {
            processAudioCommand(cmd);
        } else {
            ThreadManager::getInstance().sleep(1);
        }
    }
}

void AudioManager::processAudioCommand(const AudioCommand& cmd) {
    switch (cmd.type) {
        case AudioCommandType::PLAY_SOUND:
            break;
            
        case AudioCommandType::STOP_SOUND:
            break;
            
        case AudioCommandType::SET_VOLUME:
            masterVolume = cmd.volume;
            break;
            
        case AudioCommandType::PAUSE:
            paused = true;
            break;
            
        case AudioCommandType::RESUME:
            paused = false;
            break;
            
        case AudioCommandType::SHUTDOWN:
            audioThreadRunning = false;
            break;
    }
}

bool AudioManager::initializeAudioSystem() {
#ifdef LINUX_BUILD
    const ALCchar* deviceList = nullptr;
    if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT")) {
        deviceList = alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);
    } else {
        deviceList = alcGetString(nullptr, ALC_DEVICE_SPECIFIER);
    }
    
    alDevice = alcOpenDevice(nullptr);
    
    if (!alDevice && deviceList && deviceList[0] != '\0') {
        const ALCchar* device = deviceList;
        while (*device != '\0' && !alDevice) {
            alDevice = alcOpenDevice(device);
            if (alDevice) {
                break;
            }
            device += strlen(device) + 1;
        }
    }
    
    if (!alDevice) {
        const char* alsaDevices[] = {
            "alsa",
            "ALSA",
            "sysdefault",
            "plughw:0,0",
            "hw:0,0"
        };
        
        for (const char* deviceName : alsaDevices) {
            alDevice = alcOpenDevice(deviceName);
            if (alDevice) {
                break;
            }
        }
    }
    
    if (!alDevice) {
        std::cerr << "AudioManager: Failed to open OpenAL device (audio will be disabled)" << std::endl;
        std::cerr << "AudioManager: This may indicate that OpenAL Soft was not built with ALSA/PulseAudio support" << std::endl;
        std::cerr << "AudioManager: Try installing PulseAudio: sudo apt install pulseaudio" << std::endl;
        return false;
    }
    
    alContext = alcCreateContext(alDevice, nullptr);
    if (!alContext || alcMakeContextCurrent(alContext) == ALC_FALSE) {
        std::cerr << "AudioManager: Failed to create OpenAL context (audio will be disabled)" << std::endl;
        if (alContext) {
            alcDestroyContext(alContext);
            alContext = nullptr;
        }
        alcCloseDevice(alDevice);
        alDevice = nullptr;
        return false;
    }
    
    return true;
    
#elif defined(VITA_BUILD)
    return true;
#else
    return false;
#endif
}

void AudioManager::shutdownAudioSystem() {
#ifdef LINUX_BUILD
    if (alContext) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(alContext);
        alContext = nullptr;
    }
    if (alDevice) {
        alcCloseDevice(alDevice);
        alDevice = nullptr;
    }
#elif defined(VITA_BUILD)
#endif
}

} // namespace GameEngine

