//
// Created by asimy on 02.11.2026.
//

#include "AudioManager.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
using namespace std;

template <typename Func>
Sound AudioManager::MakeProceduralSound(float durationSecs, float vol, Func sampleAt) {
        constexpr unsigned int sampleRate = 44100;
        const auto sampleCount = static_cast<size_t>(durationSecs * sampleRate);
        vector<int16_t> samples(sampleCount);
        for (size_t index = 0; index < sampleCount; ++index) {
                const float time = static_cast<float>(index) / sampleRate;
                const float sample = clamp(sampleAt(time), -1.0f, 1.0f);
                samples[index] = static_cast<uint16_t>(sample * 32767.0f);
        }
        Wave wave{};
        wave.frameCount = static_cast<unsigned int>(samples.size());
        wave.sampleRate = sampleRate;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = samples.data();
        Sound sound = LoadSoundFromWave(wave);
        if (IsSoundValid(sound)) {
                SetSoundVolume(sound, vol);
        }
        return sound;
}

void AudioManager::init() {
        InitAudioDevice();
        audioDeviceReady = IsAudioDeviceReady();
        if (!audioDeviceReady) {
                TraceLog(LOG_WARNING, "Audio Device is unavailable: Sound effects are disabled");
                return;
        }
        constexpr float twoPi = PI * 2;
        soundEffects[static_cast<size_t>(SoundEffect::DIG)] = MakeProceduralSound(
                0.065f, 0.28f, [](float time) {
                        return 0.72f * sin(2.0f * 3.14159265359f * 1480.0f * time) * exp(-time * 90.0f);
                });

        soundEffects[static_cast<size_t>(SoundEffect::FLAG)] = MakeProceduralSound(
                0.045f, 0.2f, [](float time) {
                        return 0.5f * sin(2.0f * 3.14159265359f * 880.0f * time) * exp(-time * 140.0f);
                });

        soundEffects[static_cast<size_t>(SoundEffect::CHORD)] = MakeProceduralSound(
                0.15f, 0.23f, [twoPi](float time) {
                        if (time < 0.045f) return 0.52f * sin(twoPi * 720.0f * time) * exp(-time * 90.0f);
                        if (time >= 0.085f) {
                                const float secondClickTime = time - 0.085f;
                                return 0.52f * sin(twoPi * 980.0f * secondClickTime) * exp(
                                        -secondClickTime * 90.0f);
                        }
                        return 0.0f;
                });
        //golden ratio
        unsigned int noiseSeed = 0x9E3779B9U;
        soundEffects[static_cast<size_t>(SoundEffect::LOSS)] = MakeProceduralSound(
                0.38f, 0.36f, [&noiseSeed](float time) {
                        noiseSeed = noiseSeed * 1664525U + 1013904223U;
                        const float noise = (static_cast<float>((noiseSeed >> 16U) & 0x7FFFU) / 16383.5f) - 1.0f;
                        const float envelope = exp(-time * 8.5f);
                        const float rumble = sin(2.0f * 3.14159265359f * (58.0f - 18.0f * time) * time);
                        return (0.62f * rumble + 0.38f * noise) * envelope;
                });

        soundEffects[static_cast<size_t>(SoundEffect::WIN)] = MakeProceduralSound(
                0.72f, 0.25f, [twoPi](float time) {
                        constexpr float noteDuration = 0.17f;
                        constexpr float notes[] = {659.25f, 783.99f, 987.77f, 1318.51f};
                        const int note = min(3, static_cast<int>(time / noteDuration));
                        const float noteTime = time - note * noteDuration;
                        const float envelope = exp(-noteTime * 4.0f);
                        return 0.56f * sin(twoPi * notes[note] * noteTime) * envelope;
                });
}

void AudioManager::shutdown() {
        if (!audioDeviceReady) return;
        for (Sound sound : soundEffects) {
                if (IsSoundValid(sound)) UnloadSound(sound);
        }
        CloseAudioDevice();
        audioDeviceReady = false;
        soundEffects.fill(Sound{});
}

void AudioManager::play(SoundEffect effect) {
        if (!soundEnabled || !audioDeviceReady) return;
        const Sound sound = soundEffects[static_cast<size_t>(effect)];
        if (IsSoundValid(sound)) PlaySound(sound);
}
