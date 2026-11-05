//
// Created by asimy on 02.11.2026.
//

#ifndef MINESWEEPERCLASSIC_AUDIOMANAGER_H
#define MINESWEEPERCLASSIC_AUDIOMANAGER_H
#include <array>

#include "raylib.h"

enum class SoundEffect {
        DIG,
        FLAG,
        CHORD,
        LOSS,
        WIN
};

class AudioManager {
        bool audioDeviceReady = false;
        bool soundEnabled = true;
        std::array<Sound, 5> soundEffects{};

        template<typename Func>
        Sound MakeProceduralSound(float durationSecs, float vol, Func sampleAt);
public:
        AudioManager() = default;
        ~AudioManager() = default;

        void init();
        void shutdown();
        void play(SoundEffect effect);

        void setEnabled(bool enabled) {
                soundEnabled = enabled;
        }

        [[nodiscard]] bool isEnbabled() const {
                return soundEnabled;
        }
};

#endif
