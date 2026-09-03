#include "Envelope.h"
#include <algorithm>
#include <cmath>

namespace mv {

void Envelope::reset() {
    stage = Stage::Idle;
    currentLevel = 0.0f;
    stageStartLevel = stageTargetLevel = 0.0f;
    stageSamplesRemaining = 0;
    stageTotalSamples = 1;
}

void Envelope::enterStage(Stage s, float targetLevel, float ms) {
    stage = s;
    stageStartLevel = currentLevel;
    stageTargetLevel = targetLevel;
    stageTotalSamples = std::max(1, (int) std::lround(ms * 0.001 * sampleRate));
    stageSamplesRemaining = stageTotalSamples;
}

void Envelope::noteOn() {
    // stageStartLevel = currentLevel, not 0, so a retrigger before the
    // previous note fully released doesn't click.
    enterStage(Stage::Attack, params.attackLevel, params.attackMs);
}

void Envelope::noteOff() {
    if (stage == Stage::Idle) return;
    enterStage(Stage::Release, params.releaseLevel, params.releaseMs);
}

float Envelope::getNextSample() {
    switch (stage) {
        case Stage::Idle:
            return 0.0f;

        case Stage::Sustain:
            currentLevel = params.sustainLevel;
            return currentLevel;

        case Stage::Attack:
        case Stage::Decay:
        case Stage::Release: {
            const float t = 1.0f - (float) stageSamplesRemaining / (float) stageTotalSamples;
            currentLevel = stageStartLevel + (stageTargetLevel - stageStartLevel) * t;
            if (--stageSamplesRemaining <= 0) {
                currentLevel = stageTargetLevel;
                if (stage == Stage::Attack)       enterStage(Stage::Decay, params.sustainLevel, params.decayMs);
                else if (stage == Stage::Decay)   stage = Stage::Sustain;
                else                              stage = Stage::Idle;
            }
            return currentLevel;
        }
    }
    return 0.0f;
}

} // namespace mv
