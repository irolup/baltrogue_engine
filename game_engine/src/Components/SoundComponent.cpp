#include "Components/SoundComponent.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <cmath>

#ifdef EDITOR_BUILD
#include <imgui.h>
#endif

#include "Audio/AudioManager.h"
#include "Core/MenuManager.h"

namespace GameEngine {

struct WAVHeader {
    char riff[4]; 
    uint32_t chunkSize;
    char wave[4];
    char fmt[4]; 
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4];
    uint32_t dataSize;
};

SoundComponent::SoundComponent()
    : soundFilePath("")
    , volume(1.0f)
    , pitch(1.0f)
    , looping(false)
    , loaded(false)
    , wasPlayingBeforePause(false)
#ifdef LINUX_BUILD
    , source(0)
    , buffer(0)
    , state(AL_INITIAL)
#elif defined(VITA_BUILD)
    , audioPort(-1)
    , audioBuffer(nullptr)
    , audioBufferSize(0)
    , audioDataSize(0)
    , originalAudioBuffer(nullptr)
    , originalAudioDataSize(0)
    , sampleRate(44100)
    , channels(1)
    , isStreaming(false)
    , isPausedState(false)
    , currentStreamPos(0)
#endif
{
}

SoundComponent::~SoundComponent() {
    destroy();
}

void SoundComponent::start() {
    if (!soundFilePath.empty()) {
        loadSound();
    }
}

void SoundComponent::update(float deltaTime) {
    bool gamePaused = MenuManager::getInstance().isGamePaused();
    
    if (gamePaused) {
        if (isPlaying()) {
            wasPlayingBeforePause = true;
            pause();
        }
        return;
    }
    
    if (!gamePaused && wasPlayingBeforePause) {
        if (isPaused()) {
            resume();
        }
        wasPlayingBeforePause = false;
    }
    
    // Update playback if game is not paused
    updatePlayback();
}

void SoundComponent::destroy() {
    stop();
    
#ifdef VITA_BUILD
    auto& audioManager = AudioManager::getInstance();
    if (audioManager.isThreadingEnabled()) {
        audioManager.unregisterSoundComponent(this);
    }
#endif
    
    unloadSound();
}

void SoundComponent::setSoundFile(const std::string& filePath) {
    if (soundFilePath != filePath) {
        unloadSound();
        soundFilePath = filePath;
        if (!soundFilePath.empty() && owner) {
            loadSound();
        }
    }
}

bool SoundComponent::loadSound() {
    if (soundFilePath.empty()) {
        std::cerr << "SoundComponent: No sound file path specified" << std::endl;
        return false;
    }
    
    if (loaded) {
        return true;
    }
    
#ifdef LINUX_BUILD
    if (!initializeOpenAL()) {
        std::cerr << "SoundComponent: Failed to initialize OpenAL" << std::endl;
        return false;
    }
    
    if (!loadWAVFile(soundFilePath, buffer)) {
        std::cerr << "SoundComponent: Failed to load WAV file: " << soundFilePath << std::endl;
        cleanupOpenAL();
        return false;
    }
    
    alGenSources(1, &source);
    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        std::cerr << "SoundComponent: Failed to create OpenAL source, error: " << error << std::endl;
        cleanupOpenAL();
        return false;
    }
    
    alSourcei(source, AL_BUFFER, buffer);
    alSourcef(source, AL_GAIN, volume);
    alSourcef(source, AL_PITCH, pitch);
    alSourcei(source, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
    
    error = alGetError();
    if (error != AL_NO_ERROR) {
        std::cerr << "SoundComponent: Error configuring source, error: " << error << std::endl;
        cleanupOpenAL();
        return false;
    }
    
    loaded = true;
    return true;
    
#elif defined(VITA_BUILD)
    if (!initializeVitaAudio()) {
        return false;
    }
    
    if (!loadWAVFile(soundFilePath)) {
        cleanupVitaAudio();
        return false;
    }
    
    loaded = true;
    return true;
#else
    return false;
#endif
}

void SoundComponent::unloadSound() {
    if (!loaded) {
        return;
    }
    
    stop();
    
#ifdef LINUX_BUILD
    cleanupOpenAL();
#elif defined(VITA_BUILD)
    cleanupVitaAudio();
#endif
    
    loaded = false;
}

void SoundComponent::play() {
    if (!loaded) {
        if (!loadSound()) {
            return;
        }
    }
    
#ifdef LINUX_BUILD
    alSourcePlay(source);
#elif defined(VITA_BUILD)
    if (audioPort >= 0 && audioBuffer) {
        currentStreamPos.store(0);
        isPausedState.store(false);
        isStreaming.store(true);
        auto& audioManager = AudioManager::getInstance();
        if (audioManager.isThreadingEnabled()) {
            audioManager.registerSoundComponent(this);
        } else {
            streamAudio();
        }
    }
#endif
}

void SoundComponent::pause() {
    if (!loaded) {
        return;
    }
    
#ifdef LINUX_BUILD
    alSourcePause(source);
#elif defined(VITA_BUILD)
    if (audioPort >= 0 && isStreaming.load()) {
        isStreaming.store(false);
        isPausedState.store(true);
    }
#endif
}

void SoundComponent::resume() {
    if (!loaded) {
        return;
    }
    
#ifdef LINUX_BUILD
    alSourcePlay(source);
#elif defined(VITA_BUILD)
    if (audioPort >= 0 && audioBuffer && isPausedState.load()) {
        isPausedState.store(false);
        isStreaming.store(true);
    }
#endif
}

void SoundComponent::stop() {
    if (!loaded) {
        return;
    }
    
    wasPlayingBeforePause = false;
    
#ifdef LINUX_BUILD
    alSourceStop(source);
#elif defined(VITA_BUILD)
    if (audioPort >= 0) {
        isStreaming.store(false);
        isPausedState.store(false);
        currentStreamPos.store(0);
        auto& audioManager = AudioManager::getInstance();
        if (audioManager.isThreadingEnabled()) {
            audioManager.unregisterSoundComponent(this);
        }
    }
#endif
}

void SoundComponent::setVolume(float vol) {
    volume = vol;
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    
    if (loaded) {
        auto& audioManager = AudioManager::getInstance();
        float finalVolume = volume * audioManager.getMasterVolume();
        
#ifdef LINUX_BUILD
        alSourcef(source, AL_GAIN, finalVolume);
#elif defined(VITA_BUILD)
        if (audioPort >= 0) {
            int vitaVol = (int)(finalVolume * SCE_AUDIO_VOLUME_0DB);
            int volArray[2] = {vitaVol, vitaVol};
            sceAudioOutSetVolume(audioPort, 
                (SceAudioOutChannelFlag)(SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), 
                volArray);
        }
#endif
    }
}

void SoundComponent::setPitch(float p) {
    pitch = p;
    if (pitch < 0.5f) pitch = 0.5f;
    if (pitch > 2.0f) pitch = 2.0f;
    
    if (loaded) {
#ifdef LINUX_BUILD
        alSourcef(source, AL_PITCH, pitch);
#elif defined(VITA_BUILD)
        if (audioBuffer && audioDataSize > 0) {
            resampleAudioBuffer();
        }
#endif
    }
}

float SoundComponent::getPitch() const {
    return pitch;
}

void SoundComponent::setLoop(bool loop) {
    looping = loop;
    
    if (loaded) {
#ifdef LINUX_BUILD
        alSourcei(source, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
#elif defined(VITA_BUILD)
#endif
    }
}

bool SoundComponent::isPlaying() const {
    if (!loaded) {
        return false;
    }
    
#ifdef LINUX_BUILD
    ALint currentState;
    alGetSourcei(source, AL_SOURCE_STATE, &currentState);
    return (currentState == AL_PLAYING);
#elif defined(VITA_BUILD)
    return isStreaming.load() && !isPausedState.load();
#else
    return false;
#endif
}

bool SoundComponent::isPaused() const {
    if (!loaded) {
        return false;
    }
    
#ifdef LINUX_BUILD
    ALint currentState;
    alGetSourcei(source, AL_SOURCE_STATE, &currentState);
    return (currentState == AL_PAUSED);
#elif defined(VITA_BUILD)
    return isPausedState.load();
#else
    return false;
#endif
}

void SoundComponent::updatePlayback() {
    if (!loaded) {
        return;
    }
    
#ifdef LINUX_BUILD
#elif defined(VITA_BUILD)
    // Audio streaming is now handled by the audio thread
    // This function is kept for compatibility but does nothing on Vita when threading is enabled
    auto& audioManager = AudioManager::getInstance();
    if (!audioManager.isThreadingEnabled()) {
        if (isStreaming.load() && !isPausedState.load() && audioPort >= 0 && audioBuffer) {
            streamAudio();
        }
    }
#endif
}

#ifdef LINUX_BUILD

bool SoundComponent::initializeOpenAL() {
    auto& audioManager = AudioManager::getInstance();
    if (!audioManager.isInitialized()) {
        if (!audioManager.initialize()) {
            return false;
        }
    }
    
    ALCcontext* context = audioManager.getContext();
    if (!context) {
        return false;
    }
    alcMakeContextCurrent(context);
    
    return true;
}

void SoundComponent::cleanupOpenAL() {
    if (source != 0) {
        alDeleteSources(1, &source);
        source = 0;
    }
    if (buffer != 0) {
        alDeleteBuffers(1, &buffer);
        buffer = 0;
    }
}

bool SoundComponent::loadWAVFile(const std::string& filePath, ALuint& outBuffer) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "SoundComponent: Failed to open file: " << filePath << std::endl;
        return false;
    }
    
    char riff[4];
    uint32_t chunkSize;
    char wave[4];
    file.read(riff, 4);
    file.read(reinterpret_cast<char*>(&chunkSize), 4);
    file.read(wave, 4);
    
    if (strncmp(riff, "RIFF", 4) != 0 || strncmp(wave, "WAVE", 4) != 0) {
        std::cerr << "SoundComponent: Invalid WAV file (not RIFF WAVE): " << filePath << std::endl;
        file.close();
        return false;
    }
    
    char chunkId[4];
    uint32_t chunkSizeVal;
    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint32_t byteRate = 0;
    uint16_t blockAlign = 0;
    uint16_t bitsPerSample = 0;
    bool foundFmt = false;
    
    while (file.read(chunkId, 4)) {
        file.read(reinterpret_cast<char*>(&chunkSizeVal), 4);
        
        if (strncmp(chunkId, "fmt ", 4) == 0) {
            file.read(reinterpret_cast<char*>(&audioFormat), 2);
            file.read(reinterpret_cast<char*>(&numChannels), 2);
            file.read(reinterpret_cast<char*>(&sampleRate), 4);
            file.read(reinterpret_cast<char*>(&byteRate), 4);
            file.read(reinterpret_cast<char*>(&blockAlign), 2);
            file.read(reinterpret_cast<char*>(&bitsPerSample), 2);
            
            // Skip any extra fmt data
            if (chunkSizeVal > 16) {
                file.seekg(chunkSizeVal - 16, std::ios::cur);
            }
            foundFmt = true;
        } else if (strncmp(chunkId, "data", 4) == 0) {
            if (!foundFmt) {
                std::cerr << "SoundComponent: Found data chunk before fmt chunk: " << filePath << std::endl;
                file.close();
                return false;
            }
            
            if (audioFormat != 1) {
                std::cerr << "SoundComponent: Only PCM format supported (format: " << audioFormat << ")" << std::endl;
                file.close();
                return false;
            }
            
            ALenum format = AL_NONE;
            if (numChannels == 1) {
                format = (bitsPerSample == 8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
            } else if (numChannels == 2) {
                format = (bitsPerSample == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;
            } else {
                std::cerr << "SoundComponent: Unsupported channel count: " << numChannels << std::endl;
                file.close();
                return false;
            }
            
            std::vector<char> audioData(chunkSizeVal);
            file.read(audioData.data(), chunkSizeVal);
            
            alGenBuffers(1, &outBuffer);
            alBufferData(outBuffer, format, audioData.data(), chunkSizeVal, sampleRate);
            
            ALenum error = alGetError();
            if (error != AL_NO_ERROR) {
                std::cerr << "SoundComponent: OpenAL error loading buffer: " << error << std::endl;
                alDeleteBuffers(1, &outBuffer);
                outBuffer = 0;
                file.close();
                return false;
            }
            
            file.close();
            return true;
        } else {
            file.seekg(chunkSizeVal, std::ios::cur);
        }
        
        if (chunkSizeVal % 2 != 0) {
            file.seekg(1, std::ios::cur);
        }
    }
    
    std::cerr << "SoundComponent: Could not find data chunk in WAV file: " << filePath << std::endl;
    file.close();
    return false;
}

#elif defined(VITA_BUILD)

bool SoundComponent::initializeVitaAudio() {
    if (audioPort >= 0) {
        return true;
    }
    return true;
}

void SoundComponent::cleanupVitaAudio() {
    if (audioPort >= 0) {
        sceAudioOutReleasePort(audioPort);
        audioPort = -1;
    }
    
    if (audioBuffer && audioBuffer != originalAudioBuffer) {
        delete[] audioBuffer;
        audioBuffer = nullptr;
    }
    
    if (originalAudioBuffer) {
        delete[] originalAudioBuffer;
        originalAudioBuffer = nullptr;
    }
    
    audioBuffer = nullptr;
    
    audioBufferSize = 0;
    audioDataSize = 0;
    originalAudioDataSize = 0;
}

void SoundComponent::resampleAudioBuffer() {
    if (!originalAudioBuffer || originalAudioDataSize == 0) {
        std::cerr << "SoundComponent::resampleAudioBuffer() - No original buffer available" << std::endl;
        return;
    }
    
    if (pitch == 1.0f) {
        if (audioBuffer && audioBuffer != originalAudioBuffer) {
            delete[] audioBuffer;
        }
        audioBuffer = originalAudioBuffer;
        audioDataSize = originalAudioDataSize;
        audioBufferSize = originalAudioDataSize;
        currentStreamPos.store(0);
        return;
    }
    
    size_t originalTotalSamples = originalAudioDataSize / sizeof(int16_t);
    size_t originalSamplesPerChannel = originalTotalSamples / channels;
    size_t newSamplesPerChannel = (size_t)(originalSamplesPerChannel / pitch);
    
    if (newSamplesPerChannel == 0) {
        newSamplesPerChannel = 1;
    }
    
    size_t newTotalSamples = newSamplesPerChannel * channels;
    int16_t* resampledBuffer = new int16_t[newTotalSamples];
    
    if (channels == 1) {
        for (size_t i = 0; i < newSamplesPerChannel; i++) {
            float sourcePos = i * pitch;
            size_t sourceIndex = (size_t)sourcePos;
            float fraction = sourcePos - sourceIndex;
            
            if (sourceIndex >= originalSamplesPerChannel - 1) {
                resampledBuffer[i] = originalAudioBuffer[originalSamplesPerChannel - 1];
            } else {
                int16_t sample1 = originalAudioBuffer[sourceIndex];
                int16_t sample2 = originalAudioBuffer[sourceIndex + 1];
                resampledBuffer[i] = (int16_t)(sample1 + (sample2 - sample1) * fraction);
            }
        }
    } else if (channels == 2) {
        for (size_t i = 0; i < newSamplesPerChannel; i++) {
            float sourcePos = i * pitch;
            size_t sourceIndex = (size_t)sourcePos;
            float fraction = sourcePos - sourceIndex;
            
            if (sourceIndex >= originalSamplesPerChannel - 1) {
                size_t lastIdx = (originalSamplesPerChannel - 1) * 2;
                resampledBuffer[i * 2] = originalAudioBuffer[lastIdx];
                resampledBuffer[i * 2 + 1] = originalAudioBuffer[lastIdx + 1];
            } else {
                size_t idx1 = sourceIndex * 2;
                size_t idx2 = (sourceIndex + 1) * 2;
                
                int16_t left1 = originalAudioBuffer[idx1];
                int16_t right1 = originalAudioBuffer[idx1 + 1];
                int16_t left2 = originalAudioBuffer[idx2];
                int16_t right2 = originalAudioBuffer[idx2 + 1];
                
                resampledBuffer[i * 2] = (int16_t)(left1 + (left2 - left1) * fraction);
                resampledBuffer[i * 2 + 1] = (int16_t)(right1 + (right2 - right1) * fraction);
            }
        }
    }
    
    if (audioBuffer && audioBuffer != originalAudioBuffer) {
        delete[] audioBuffer;
    }
    audioBuffer = resampledBuffer;
    audioDataSize = newTotalSamples * sizeof(int16_t);
    audioBufferSize = audioDataSize;
    currentStreamPos = 0;
}

bool SoundComponent::loadWAVFile(const std::string& filePath) {
    std::string vitaPath = filePath;
    if (vitaPath.find("assets/") == 0) {
        vitaPath = "app0:/" + vitaPath;
    }
    
    std::ifstream file(vitaPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "SoundComponent: Failed to open file: " << vitaPath << std::endl;
        return false;
    }
    
    char riff[4];
    uint32_t chunkSize;
    char wave[4];
    file.read(riff, 4);
    file.read(reinterpret_cast<char*>(&chunkSize), 4);
    file.read(wave, 4);
    
    if (strncmp(riff, "RIFF", 4) != 0 || strncmp(wave, "WAVE", 4) != 0) {
        std::cerr << "SoundComponent: Invalid WAV file (not RIFF WAVE): " << vitaPath << std::endl;
        file.close();
        return false;
    }
    
    char chunkId[4];
    uint32_t chunkSizeVal;
    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRateVal = 0;
    uint32_t byteRate = 0;
    uint16_t blockAlign = 0;
    uint16_t bitsPerSample = 0;
    bool foundFmt = false;
    uint32_t dataSize = 0;
    
    while (file.read(chunkId, 4)) {
        file.read(reinterpret_cast<char*>(&chunkSizeVal), 4);
        
        if (strncmp(chunkId, "fmt ", 4) == 0) {
            file.read(reinterpret_cast<char*>(&audioFormat), 2);
            file.read(reinterpret_cast<char*>(&numChannels), 2);
            file.read(reinterpret_cast<char*>(&sampleRateVal), 4);
            file.read(reinterpret_cast<char*>(&byteRate), 4);
            file.read(reinterpret_cast<char*>(&blockAlign), 2);
            file.read(reinterpret_cast<char*>(&bitsPerSample), 2);
            
            // Skip any extra fmt data
            if (chunkSizeVal > 16) {
                file.seekg(chunkSizeVal - 16, std::ios::cur);
            }
            foundFmt = true;
        } else if (strncmp(chunkId, "data", 4) == 0) {
            if (!foundFmt) {
                std::cerr << "SoundComponent: Found data chunk before fmt chunk: " << vitaPath << std::endl;
                file.close();
                return false;
            }
            
            if (audioFormat != 1) {
                std::cerr << "SoundComponent: Only PCM format supported (format: " << audioFormat << ")" << std::endl;
                file.close();
                return false;
            }
            
            dataSize = chunkSizeVal;
            
            sampleRate = sampleRateVal;
            channels = numChannels;
            audioDataSize = dataSize;
            
            audioBufferSize = dataSize;
            int16_t* tempBuffer = new int16_t[audioDataSize / sizeof(int16_t)];
            file.read(reinterpret_cast<char*>(tempBuffer), audioDataSize);
            
            if (bitsPerSample != 16) {
                std::cerr << "SoundComponent: Warning - file is " << bitsPerSample << "-bit, expected 16-bit" << std::endl;
            }
            
            originalAudioBuffer = tempBuffer;
            originalAudioDataSize = audioDataSize;
            audioBuffer = originalAudioBuffer;
            audioBufferSize = audioDataSize;
            
            
            file.close();
            break;
        } else {
            file.seekg(chunkSizeVal, std::ios::cur);
        }
        
        if (chunkSizeVal % 2 != 0) {
            file.seekg(1, std::ios::cur);
        }
    }
    
    if (!foundFmt || dataSize == 0) {
        std::cerr << "SoundComponent: Could not find fmt or data chunk in WAV file: " << vitaPath << std::endl;
        file.close();
        return false;
    }
    
    if (channels != 1 && channels != 2) {
        std::cerr << "SoundComponent: Unsupported channel count: " << channels << " (only mono/stereo supported)" << std::endl;
        delete[] originalAudioBuffer;
        originalAudioBuffer = nullptr;
        audioBuffer = nullptr;
        originalAudioDataSize = 0;
        return false;
    }
    
    if (audioPort < 0) {
        int bufferSize = 256;
        SceAudioOutMode mode = (channels == 1) ? SCE_AUDIO_OUT_MODE_MONO : SCE_AUDIO_OUT_MODE_STEREO;
        
        audioPort = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, bufferSize, sampleRate, mode);
        if (audioPort < 0) {
            std::cerr << "SoundComponent: Failed to open Vita audio port: " << audioPort << std::endl;
            delete[] originalAudioBuffer;
            originalAudioBuffer = nullptr;
            audioBuffer = nullptr;
            originalAudioDataSize = 0;
            return false;
        }
        
        int vol = (int)(volume * SCE_AUDIO_VOLUME_0DB * 0.8f);
        int volArray[2] = {vol, vol};
        sceAudioOutSetVolume(audioPort, 
            (SceAudioOutChannelFlag)(SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), 
            volArray);
    } else {
        int bufferSize = 256;
        SceAudioOutMode mode = (channels == 1) ? SCE_AUDIO_OUT_MODE_MONO : SCE_AUDIO_OUT_MODE_STEREO;
        sceAudioOutSetConfig(audioPort, bufferSize, sampleRate, mode);
    }
    
    currentStreamPos.store(0);
    
    if (pitch != 1.0f) {
        resampleAudioBuffer();
    }
    
    return true;
}

void SoundComponent::streamAudio() {
    if (!audioBuffer || audioPort < 0 || !isStreaming.load()) {
        return;
    }
    
    
    const size_t SAMPLES_PER_CHANNEL = 256;
    const size_t SAMPLES_PER_CHUNK = SAMPLES_PER_CHANNEL * channels;
    
    // Output one chunk per call - sceAudioOutOutput() is blocking and naturally throttles
    static int16_t outputBuffer[512];
    size_t totalSamples = audioDataSize / sizeof(int16_t);
    
    size_t currentPos = currentStreamPos.load();
    size_t currentSample = currentPos / sizeof(int16_t);
    
    if (currentSample >= totalSamples) {
        if (looping) {
            currentStreamPos.store(0);
            currentSample = 0;
        } else {
            isStreaming.store(false);
            return;
        }
    }
    
    size_t remainingSamples = totalSamples - currentSample;
    
    if (SAMPLES_PER_CHUNK > 512) {
        std::cerr << "SoundComponent: Buffer too small for channel count" << std::endl;
        isStreaming.store(false);
        return;
    }
    
    if (remainingSamples >= SAMPLES_PER_CHUNK) {
        memcpy(outputBuffer, audioBuffer + currentSample, SAMPLES_PER_CHUNK * sizeof(int16_t));
        currentStreamPos.store(currentPos + SAMPLES_PER_CHUNK * sizeof(int16_t));
    } else {
        if (looping) {
            size_t samplesFromEnd = remainingSamples;
            size_t samplesFromStart = SAMPLES_PER_CHUNK - samplesFromEnd;
            
            memcpy(outputBuffer, audioBuffer + currentSample, samplesFromEnd * sizeof(int16_t));
            memcpy(outputBuffer + samplesFromEnd, audioBuffer, samplesFromStart * sizeof(int16_t));
            
            currentStreamPos.store(samplesFromStart * sizeof(int16_t));
        } else {
            memcpy(outputBuffer, audioBuffer + currentSample, remainingSamples * sizeof(int16_t));
            memset(outputBuffer + remainingSamples, 0, (SAMPLES_PER_CHUNK - remainingSamples) * sizeof(int16_t));
            currentStreamPos.store(totalSamples * sizeof(int16_t));
            isStreaming.store(false);
        }
    }
    
    int result = sceAudioOutOutput(audioPort, outputBuffer);
    if (result < 0) {
        std::cerr << "SoundComponent: Audio output error: " << result << std::endl;
        isStreaming.store(false);
        return;
    }
    
    if (currentStreamPos.load() >= audioDataSize && !looping) {
        isStreaming.store(false);
    }
}

#endif

void SoundComponent::drawInspector() {
#ifdef EDITOR_BUILD
    static char filePathBuffer[256] = {0};
    if (filePathBuffer[0] == '\0') {
        strncpy(filePathBuffer, soundFilePath.c_str(), sizeof(filePathBuffer) - 1);
    }
    
    if (ImGui::InputText("Sound File", filePathBuffer, sizeof(filePathBuffer))) {
        setSoundFile(std::string(filePathBuffer));
    }
    
    float vol = volume;
    if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f)) {
        setVolume(vol);
    }
    
    bool loop = looping;
    if (ImGui::Checkbox("Loop", &loop)) {
        setLoop(loop);
    }
    
    ImGui::Separator();
    if (ImGui::Button("Play")) {
        play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause")) {
        pause();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        stop();
    }
    
    ImGui::Text("Status: %s", loaded ? (isPlaying() ? "Playing" : (isPaused() ? "Paused" : "Stopped")) : "Not Loaded");
#endif
}

} // namespace GameEngine

