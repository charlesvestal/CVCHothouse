#pragma once

#include <cmath>

// Simple State Variable Filter to replace daisysp::Svf
class SVFFilter
{
  public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        Reset();
    }

    void SetFreq(float freq)
    {
        freq_ = freq;
        UpdateCoefficients();
    }

    void SetRes(float res)
    {
        res_ = res;
        UpdateCoefficients();
    }

    void SetDrive(float drive)
    {
        drive_ = drive;
    }

    void Reset()
    {
        low_ = 0.0f;
        band_ = 0.0f;
        high_ = 0.0f;
        notch_ = 0.0f;
    }

    // Process a sample and return lowpass output
    float Process(float input)
    {
        input *= drive_;

        low_ = low_ + f_ * band_;
        high_ = input - low_ - q_ * band_;
        band_ = f_ * high_ + band_;
        notch_ = high_ + low_;

        return low_;
    }

    float Low() const { return low_; }
    float Band() const { return band_; }
    float High() const { return high_; }
    float Notch() const { return notch_; }

  private:
    void UpdateCoefficients()
    {
        f_ = 2.0f * std::sin(3.14159265358979323846f * freq_ / sampleRate_);
        q_ = res_;
    }

    float sampleRate_ = 48000.0f;
    float freq_ = 1000.0f;
    float res_ = 0.707f;
    float drive_ = 1.0f;
    float f_ = 0.0f;
    float q_ = 0.707f;
    float low_ = 0.0f;
    float band_ = 0.0f;
    float high_ = 0.0f;
    float notch_ = 0.0f;
};
