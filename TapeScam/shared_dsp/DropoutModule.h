#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>

struct DropoutParams
{
    float rateHz      = 0.0f;  // average events per second
    float depthMinDb  = 0.0f;
    float depthMaxDb  = 0.0f;
    float durMinMs    = 0.0f;
    float durMaxMs    = 0.0f;
    float clusterProb = 0.0f;  // probability of quick follow-up event
    float monoLink    = 1.0f;  // 1 = both channels linked
    float minRestSec  = 0.0f;  // enforced downtime after each event
};

class DropoutModule
{
  public:
    void Init(float sampleRate);
    void Reset();
    void SetParams(const DropoutParams& params);
    void Process(float** buffers, size_t numSamples, size_t numChannels);

  private:
    struct DropoutState
    {
        bool  active        = false;
        int   stage         = 0;     // 0=attack,1=hold,2=release
        int   stageSamples  = 0;     // progress within stage
        int   attackSamples = 1;
        int   holdSamples   = 1;
        int   releaseSamples= 1;
        float minGain       = 1.0f;
        float env           = 1.0f;
        bool  justEnded     = false;
    };

    void TriggerEvent(int channel, float depthDb, float durationMs);
    float AdvanceState(DropoutState& state);
    bool  AnyActive() const;
    float NextRandom();

    float sampleRate_ = 48000.0f;
    DropoutParams params_{};
    DropoutState  states_[2];

    float restSamplesLeft_       = 0.0f;
    float clusterWindowSamples_  = 0.0f;
    uint32_t randState_ = 0x1234567u;
};
