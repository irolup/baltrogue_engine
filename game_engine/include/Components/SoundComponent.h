#ifndef SOUND_COMPONENT_H
#define SOUND_COMPONENT_H

#include "Components/Component.h"
#include "../../include/Platform.h"
#include <string>
#include <memory>

#ifdef LINUX_BUILD
#include <AL/al.h>
#include <AL/alc.h>
#elif defined(VITA_BUILD)
#include <psp2/audioout.h>
#include <stdint.h>
#endif

namespace GameEngine {

class SoundComponent : public Component {
public:
    SoundComponent();
    virtual ~SoundComponent();
    
    COMPONENT_TYPE(SoundComponent)
    
    virtual void start() override;
    virtual void update(float deltaTime) override;
    virtual void destroy() override;
    
    // Sound file management
    void setSoundFile(const std::string& filePath);
    const std::string& getSoundFile() const { return soundFilePath; }
    bool loadSound();
    void unloadSound();
    
    // Playback control
    void play();
    void pause();
    void resume();
    void stop();
    
    // Properties
    void setVolume(float volume); // 0.0 to 1.0
    float getVolume() const { return volume; }
    
    void setPitch(float pitch); // 0.5 to 2.0 (1.0 = normal speed)
    float getPitch() const;
    
    void setLoop(bool loop);
    bool isLooping() const { return looping; }
    
    bool isPlaying() const;
    bool isPaused() const;
    
    virtual void drawInspector() override;
    
private:
    std::string soundFilePath;
    float volume;
    float pitch;  // Playback pitch/speed (0.5 to 2.0, 1.0 = normal)
    bool looping;
    bool loaded;
    bool wasPlayingBeforePause;  // Track if sound was playing before game pause
    
#ifdef LINUX_BUILD
    // OpenAL resources
    ALuint source;
    ALuint buffer;
    ALenum state;
    
    bool initializeOpenAL();
    void cleanupOpenAL();
    bool loadWAVFile(const std::string& filePath, ALuint& buffer);
    
#elif defined(VITA_BUILD)
    // PSVita audio resources
    int audioPort;
    int16_t* audioBuffer;  // Current buffer (may be resampled)
    size_t audioBufferSize;
    size_t audioDataSize;
    int16_t* originalAudioBuffer;  // Original buffer (never modified, used for resampling)
    size_t originalAudioDataSize;  // Original buffer size
    int sampleRate;
    int channels;
    bool isStreaming;
    bool isPausedState;  // Track pause state for Vita
    size_t currentStreamPos;  // Current position in audio stream
    
    bool initializeVitaAudio();
    void cleanupVitaAudio();
    bool loadWAVFile(const std::string& filePath);
    void streamAudio();
    void resampleAudioBuffer();  // Resample audio buffer based on pitch
    
#endif
    
    void updatePlayback();
};

} // namespace GameEngine

#endif // SOUND_COMPONENT_H

