#include "DattorroReverb.h"
#include <algorithm>
#include <cmath>

// Define static arrays
constexpr int DattorroReverb::kInputAPDelays[4];

void DattorroReverb::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    feedback_ = 0.5f;  // Start much lower
    damping_ = 0.5f;

    // Pre-delay
    preDelayBuf_.reset(new float[kPreDelaySize]);
    std::fill(preDelayBuf_.get(), preDelayBuf_.get() + kPreDelaySize, 0.0f);
    preDelayWrite_ = 0;

    // Input diffusion allpass filters
    for(int i = 0; i < 4; i++) {
        inputAP_[i].size = kInputAPDelays[i];
        inputAP_[i].buffer.reset(new float[inputAP_[i].size]);
        std::fill(inputAP_[i].buffer.get(),
                  inputAP_[i].buffer.get() + inputAP_[i].size, 0.0f);
        inputAP_[i].writePos = 0;
    }

    // Tank 1 (Left channel)
    tank1Delay1Buf_.reset(new float[kTank1Delay1]);
    tank1AP1Buf_.reset(new float[kTank1AP1]);
    tank1Delay2Buf_.reset(new float[kTank1Delay2]);
    tank1AP2Buf_.reset(new float[kTank1AP2]);

    std::fill(tank1Delay1Buf_.get(), tank1Delay1Buf_.get() + kTank1Delay1, 0.0f);
    std::fill(tank1AP1Buf_.get(), tank1AP1Buf_.get() + kTank1AP1, 0.0f);
    std::fill(tank1Delay2Buf_.get(), tank1Delay2Buf_.get() + kTank1Delay2, 0.0f);
    std::fill(tank1AP2Buf_.get(), tank1AP2Buf_.get() + kTank1AP2, 0.0f);

    // Tank 2 (Right channel)
    tank2Delay1Buf_.reset(new float[kTank2Delay1]);
    tank2AP1Buf_.reset(new float[kTank2AP1]);
    tank2Delay2Buf_.reset(new float[kTank2Delay2]);
    tank2AP2Buf_.reset(new float[kTank2AP2]);

    std::fill(tank2Delay1Buf_.get(), tank2Delay1Buf_.get() + kTank2Delay1, 0.0f);
    std::fill(tank2AP1Buf_.get(), tank2AP1Buf_.get() + kTank2AP1, 0.0f);
    std::fill(tank2Delay2Buf_.get(), tank2Delay2Buf_.get() + kTank2Delay2, 0.0f);
    std::fill(tank2AP2Buf_.get(), tank2AP2Buf_.get() + kTank2AP2, 0.0f);

    tank1Delay1Write_ = 0;
    tank1AP1Write_ = 0;
    tank1Delay2Write_ = 0;
    tank1AP2Write_ = 0;

    tank2Delay1Write_ = 0;
    tank2AP1Write_ = 0;
    tank2Delay2Write_ = 0;
    tank2AP2Write_ = 0;

    tank1Damp_ = 0.0f;
    tank2Damp_ = 0.0f;
}

void DattorroReverb::SetFeedback(float fb)
{
    feedback_ = fb < 0.0f ? 0.0f : (fb > 0.95f ? 0.95f : fb);
}

void DattorroReverb::SetDamping(float damp)
{
    damping_ = damp < 0.0f ? 0.0f : (damp > 1.0f ? 1.0f : damp);
}

float DattorroReverb::ProcessAllpass(float input, float* buffer, int size, int& writePos, float gain)
{
    float delayed = buffer[writePos];
    float output = -input + delayed;
    buffer[writePos] = input + delayed * gain;

    writePos++;
    if(writePos >= size) writePos = 0;

    return output;
}

void DattorroReverb::Process(float inL, float inR, float* outL, float* outR)
{
    // Mix input to mono for reverb input
    float input = (inL + inR) * 0.5f;

    // Pre-delay
    float preDelayed = preDelayBuf_[preDelayWrite_];
    preDelayBuf_[preDelayWrite_] = input;
    preDelayWrite_++;
    if(preDelayWrite_ >= kPreDelaySize) preDelayWrite_ = 0;

    // Input diffusion (4 cascaded allpass filters)
    const float inputAPGain = 0.75f;
    float diffused = preDelayed;
    for(int i = 0; i < 4; i++) {
        diffused = ProcessAllpass(diffused,
                                   inputAP_[i].buffer.get(),
                                   inputAP_[i].size,
                                   inputAP_[i].writePos,
                                   inputAPGain);
    }

    // Split to two tank inputs
    float tank1In = diffused;
    float tank2In = diffused;

    // Tank 1 (Left)
    // Read from delay1
    float tank1Out1 = tank1Delay1Buf_[tank1Delay1Write_];

    // One-pole lowpass for damping
    const float dampCoeff = 1.0f - damping_;
    tank1Damp_ = tank1Out1 * dampCoeff + tank1Damp_ * damping_;

    // Allpass 1
    float tank1AP1Out = ProcessAllpass(tank1Damp_,
                                       tank1AP1Buf_.get(),
                                       kTank1AP1,
                                       tank1AP1Write_,
                                       -0.5f);

    // Delay 2
    float tank1Out2 = tank1Delay2Buf_[tank1Delay2Write_];
    tank1Delay2Buf_[tank1Delay2Write_] = tank1AP1Out;
    tank1Delay2Write_++;
    if(tank1Delay2Write_ >= kTank1Delay2) tank1Delay2Write_ = 0;

    // Allpass 2
    float tank1AP2Out = ProcessAllpass(tank1Out2,
                                       tank1AP2Buf_.get(),
                                       kTank1AP2,
                                       tank1AP2Write_,
                                       -0.5f);

    // Tank 2 (Right)
    float tank2Out1 = tank2Delay1Buf_[tank2Delay1Write_];

    tank2Damp_ = tank2Out1 * dampCoeff + tank2Damp_ * damping_;

    float tank2AP1Out = ProcessAllpass(tank2Damp_,
                                       tank2AP1Buf_.get(),
                                       kTank2AP1,
                                       tank2AP1Write_,
                                       -0.5f);

    float tank2Out2 = tank2Delay2Buf_[tank2Delay2Write_];
    tank2Delay2Buf_[tank2Delay2Write_] = tank2AP1Out;
    tank2Delay2Write_++;
    if(tank2Delay2Write_ >= kTank2Delay2) tank2Delay2Write_ = 0;

    float tank2AP2Out = ProcessAllpass(tank2Out2,
                                       tank2AP2Buf_.get(),
                                       kTank2AP2,
                                       tank2AP2Write_,
                                       -0.5f);

    // Cross-feedback between tanks
    float tank1Input = tank1In + tank2AP2Out * feedback_;
    float tank2Input = tank2In + tank1AP2Out * feedback_;

    // Write to delay1 buffers
    tank1Delay1Buf_[tank1Delay1Write_] = tank1Input;
    tank1Delay1Write_++;
    if(tank1Delay1Write_ >= kTank1Delay1) tank1Delay1Write_ = 0;

    tank2Delay1Buf_[tank2Delay1Write_] = tank2Input;
    tank2Delay1Write_++;
    if(tank2Delay1Write_ >= kTank2Delay1) tank2Delay1Write_ = 0;

    // Output - tap from multiple points in each tank for stereo width
    *outL = (tank1Out1 + tank1AP1Out + tank1Out2 + tank1AP2Out) * 0.25f;
    *outR = (tank2Out1 + tank2AP1Out + tank2Out2 + tank2AP2Out) * 0.25f;
}
