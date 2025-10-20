#pragma once

// Lo-Fi Upward Compressor - mimics cheap cassette deck AGC
// Boosts quiet signals, making background hiss "breathe" and pump
class LoFiCompressor
{
  public:
    void Init(float sampleRate);
    void SetMode(int mode);  // 0=Off, 1=Light, 2=Heavy
    void Process(float* left, float* right, size_t size);

  private:
    float sampleRate_ = 48000.0f;
    int mode_ = 0;

    // Envelope follower state
    float envelopeL_ = 0.0f;
    float envelopeR_ = 0.0f;

    // AGC parameters (attack/release for envelope)
    float attackCoeff_ = 0.0f;
    float releaseCoeff_ = 0.0f;

    // Compression parameters
    float threshold_ = 0.0f;  // Below this, boost kicks in
    float ratio_ = 1.0f;      // Upward compression ratio
    float makeupGain_ = 1.0f;

    void UpdateParameters();
    float ProcessGain(float envelope, float inputLevel);
};
