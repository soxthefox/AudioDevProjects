#pragma once

namespace mv {

// Free-form 3-breakpoint envelope: attack ramps to attackLevel, decay ramps
// to sustainLevel and holds, release ramps to releaseLevel then goes idle.
// Replaces juce::ADSR, which can only ever ramp 0 -> 1 -> sustain -> 0 —
// here the attack and release targets are themselves parameters, so a
// breakpoint can be dragged to any level, not just the endpoints.
class Envelope {
public:
    struct Parameters {
        float attackMs = 20.0f, attackLevel = 1.0f;
        float decayMs = 80.0f, sustainLevel = 0.8f;
        float releaseMs = 300.0f, releaseLevel = 0.0f;
    };

    void setSampleRate(double sr) { sampleRate = sr; }
    void setParameters(const Parameters& p) { params = p; }
    void reset();
    void noteOn();
    void noteOff();
    bool isActive() const { return stage != Stage::Idle; }
    float getNextSample();

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void enterStage(Stage s, float targetLevel, float ms);

    Parameters params;
    double sampleRate = 44100.0;
    Stage stage = Stage::Idle;
    float currentLevel = 0.0f;
    float stageStartLevel = 0.0f, stageTargetLevel = 0.0f;
    int stageSamplesRemaining = 0, stageTotalSamples = 1;
};

} // namespace mv
