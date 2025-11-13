#include "DropoutModule.h"

#include <algorithm>

namespace
{
constexpr float kMinGain = 0.1f;
constexpr float kClusterWindowSec = 0.15f;
}

void DropoutModule::Init(float sampleRate)
{
    sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    randState_ ^= static_cast<uint32_t>(sampleRate_);
    Reset();
}

void DropoutModule::Reset()
{
    for(auto& st : states_)
    {
        st = {};
        st.env = 1.0f;
    }
    restSamplesLeft_ = 0.0f;
    clusterWindowSamples_ = 0.0f;
}

void DropoutModule::SetParams(const DropoutParams& params)
{
    params_ = params;
    params_.rateHz      = std::max(0.0f, params_.rateHz);
    params_.monoLink    = std::clamp(params_.monoLink, 0.0f, 1.0f);
    params_.clusterProb = std::clamp(params_.clusterProb, 0.0f, 1.0f);
    params_.minRestSec  = std::max(0.0f, params_.minRestSec);
}

void DropoutModule::Process(float** buffers, size_t numSamples, size_t numChannels)
{
    if(!buffers || numChannels < 2 || numSamples == 0)
        return;

    if(params_.rateHz <= 0.0001f)
    {
        for(auto& st : states_)
        {
            st.active = false;
            st.env = 1.0f;
        }
        return;
    }

    // Update rest/cluster timers
    if(restSamplesLeft_ > 0.0f)
        restSamplesLeft_ = std::max(0.0f, restSamplesLeft_ - static_cast<float>(numSamples));
    if(clusterWindowSamples_ > 0.0f)
        clusterWindowSamples_ = std::max(0.0f, clusterWindowSamples_ - static_cast<float>(numSamples));

    const float blockProb = params_.rateHz * static_cast<float>(numSamples) / sampleRate_;
    const bool readyForNewEvent = restSamplesLeft_ <= 0.0f;
    bool trigger = (!AnyActive()) && readyForNewEvent && (NextRandom() < blockProb);

    if(!trigger && !AnyActive() && clusterWindowSamples_ > 0.0f && params_.clusterProb > 0.0f)
    {
        if(NextRandom() < params_.clusterProb)
        {
            trigger = true;
            clusterWindowSamples_ = 0.0f;
        }
    }

    if(trigger)
    {
        const float depthDb = std::min(params_.depthMinDb, params_.depthMaxDb) +
                              NextRandom() * (std::abs(params_.depthMaxDb - params_.depthMinDb));
        const float durMs = params_.durMinMs + NextRandom() * (std::max(params_.durMaxMs, params_.durMinMs) - params_.durMinMs);

        const float monoRand = NextRandom();
        if(monoRand < params_.monoLink)
        {
            TriggerEvent(0, depthDb, durMs);
            TriggerEvent(1, depthDb, durMs);
        }
        else
        {
            const int channel = NextRandom() < 0.5f ? 0 : 1;
            TriggerEvent(channel, depthDb, durMs);
        }
    }

    for(size_t i = 0; i < numSamples; ++i)
    {
        const float gL = AdvanceState(states_[0]);
        const float gR = AdvanceState(states_[1]);
        buffers[0][i] *= gL;
        buffers[1][i] *= gR;
    }
}

void DropoutModule::TriggerEvent(int channel, float depthDb, float durationMs)
{
    if(channel < 0 || channel > 1)
        return;

    DropoutState& state = states_[channel];
    if(state.active)
        return;

    const float totalSamples = std::max(1.0f, durationMs * 0.001f * sampleRate_);
    const int attackSamples  = std::max(1, static_cast<int>(totalSamples * 0.25f));
    const int holdSamples    = std::max(1, static_cast<int>(totalSamples * 0.25f));
    const int releaseSamples = std::max(1, static_cast<int>(totalSamples) - attackSamples - holdSamples);

    const float minGain = std::clamp(std::pow(10.0f, depthDb / 20.0f), kMinGain, 1.0f);

    state.active         = true;
    state.stage          = 0;
    state.stageSamples   = 0;
    state.attackSamples  = attackSamples;
    state.holdSamples    = holdSamples;
    state.releaseSamples = releaseSamples;
    state.minGain        = minGain;
    state.env            = 1.0f;
    state.justEnded      = false;
}

float DropoutModule::AdvanceState(DropoutState& state)
{
    if(!state.active)
        return 1.0f;

    switch(state.stage)
    {
        case 0: // Attack
        {
            ++state.stageSamples;
            const float t = static_cast<float>(state.stageSamples) / std::max(1, state.attackSamples);
            state.env = 1.0f + (state.minGain - 1.0f) * std::clamp(t, 0.0f, 1.0f);
            if(state.stageSamples >= state.attackSamples)
            {
                state.stage = 1;
                state.stageSamples = 0;
            }
            break;
        }
        case 1: // Hold
        {
            state.env = state.minGain;
            ++state.stageSamples;
            if(state.stageSamples >= state.holdSamples)
            {
                state.stage = 2;
                state.stageSamples = 0;
            }
            break;
        }
        case 2: // Release
        {
            ++state.stageSamples;
            const float t = static_cast<float>(state.stageSamples) / std::max(1, state.releaseSamples);
            state.env = state.minGain + (1.0f - state.minGain) * std::clamp(t, 0.0f, 1.0f);
            if(state.stageSamples >= state.releaseSamples)
            {
                state.active = false;
                state.env = 1.0f;
                state.justEnded = true;
                restSamplesLeft_ = std::max(restSamplesLeft_, params_.minRestSec * sampleRate_);
                clusterWindowSamples_ = kClusterWindowSec * sampleRate_;
            }
            break;
        }
        default:
            state.active = false;
            state.env = 1.0f;
            break;
    }

    return state.env;
}

bool DropoutModule::AnyActive() const
{
    return states_[0].active || states_[1].active;
}

float DropoutModule::NextRandom()
{
    randState_ = randState_ * 1664525u + 1013904223u;
    return static_cast<float>(randState_) * 2.3283064365386963e-10f;
}
