// PsxReverb.h - PSX SPU Reverb Core DSP
// Implements authentic PSX reverb algorithm at 24kHz internal rate
// Signal flow: Reflections (same/diff) → Comb → APF1 → APF2
#pragma once

#include "WorkArea.h"
#include "PsxPreset.h"
#include "Halfband39.h"
#include <cstdint>
#include <algorithm>

class PsxReverb {
public:
    PsxReverb() : work_buf_(nullptr), initialized_(false) {}

    // Initialize with target I/O sample rate and preset
    void Init(float fs_io, const PsxPreset& preset) {
        io_rate_ = fs_io;

        // PSX runs at 44.1kHz, we run at 48kHz - scale delays accordingly
        scale_factor_ = fs_io / 44100.0f;

        // Scale all delay values
        ScalePreset(preset);

        // Single shared work area: first half = L, second half = R
        // PSX: "L,L,L,L,... R,R,R,R..." in one buffer
        uint32_t work_size_samples = NextPowerOf2(
            static_cast<uint32_t>(preset.work_size * scale_factor_ / sizeof(int16_t))
        );

        // Allocate single shared buffer in SRAM
        if (!work_buf_) {
            work_buf_ = new int16_t[work_size_samples];
        }

        work_.Init(work_buf_, work_size_samples);

        // Initialize resamplers
        down_L_.Init();
        down_R_.Init();
        up_L_.Init();
        up_R_.Init();

        initialized_ = true;
    }

    // Process block at I/O sample rate (48kHz, 16 samples)
    // PSX: Resample 44.1k→22.05k, process BOTH L+R every tick, upsample back
    // We: Resample 48k→24k, process BOTH L+R every tick, upsample back
    void ProcessBlock(const float* inL, const float* inR,
                     float* outL, float* outR, int nFrames) {
        if (!initialized_) {
            for (int i = 0; i < nFrames; ++i) {
                outL[i] = 0.0f;
                outR[i] = 0.0f;
            }
            return;
        }

        // Process pairs (2:1 decimation to 24kHz)
        for (int i = 0; i < nFrames; i += 2) {
            if (i + 1 >= nFrames) break;

            // Decimate both channels 48k→24k
            float Lin = down_L_.Decimate(inL[i], inL[i+1]) * p_.vLIN_f;
            float Rin = down_R_.Decimate(inR[i], inR[i+1]) * p_.vRIN_f;

            // Process BOTH channels every 24kHz tick (matching PSX behavior)
            // Same-side reflection
            float lsame_fb = work_.ReadRelative(p_.dLSAME);
            float lsame_iir = work_.ReadRelative(p_.mLSAME - 1);
            float lsame_out = (Lin + lsame_fb * p_.vWALL_f - lsame_iir) * p_.vIIR_f + lsame_iir;
            work_.WriteRelative(p_.mLSAME, lsame_out);

            float rsame_fb = work_.ReadRelative(p_.dRSAME);
            float rsame_iir = work_.ReadRelative(p_.mRSAME - 1);
            float rsame_out = (Rin + rsame_fb * p_.vWALL_f - rsame_iir) * p_.vIIR_f + rsame_iir;
            work_.WriteRelative(p_.mRSAME, rsame_out);

            // Different-side reflection
            float ldiff_fb = work_.ReadRelative(p_.dRDIFF);
            float ldiff_iir = work_.ReadRelative(p_.mLDIFF - 1);
            float ldiff_out = (Lin + ldiff_fb * p_.vWALL_f - ldiff_iir) * p_.vIIR_f + ldiff_iir;
            work_.WriteRelative(p_.mLDIFF, ldiff_out);

            float rdiff_fb = work_.ReadRelative(p_.dLDIFF);
            float rdiff_iir = work_.ReadRelative(p_.mRDIFF - 1);
            float rdiff_out = (Rin + rdiff_fb * p_.vWALL_f - rdiff_iir) * p_.vIIR_f + rdiff_iir;
            work_.WriteRelative(p_.mRDIFF, rdiff_out);

            // Early echo (comb filter)
            float Lout = p_.vCOMB1_f * work_.ReadRelative(p_.mLCOMB1) +
                        p_.vCOMB2_f * work_.ReadRelative(p_.mLCOMB2) +
                        p_.vCOMB3_f * work_.ReadRelative(p_.mLCOMB3) +
                        p_.vCOMB4_f * work_.ReadRelative(p_.mLCOMB4);

            float Rout = p_.vCOMB1_f * work_.ReadRelative(p_.mRCOMB1) +
                        p_.vCOMB2_f * work_.ReadRelative(p_.mRCOMB2) +
                        p_.vCOMB3_f * work_.ReadRelative(p_.mRCOMB3) +
                        p_.vCOMB4_f * work_.ReadRelative(p_.mRCOMB4);

            // Late reverb APF1
            float lapf1_del = work_.ReadRelative(p_.mLAPF1 - p_.dAPF1);
            Lout -= p_.vAPF1_f * lapf1_del;
            work_.WriteRelative(p_.mLAPF1, Lout);
            Lout = Lout * p_.vAPF1_f + lapf1_del;

            float rapf1_del = work_.ReadRelative(p_.mRAPF1 - p_.dAPF1);
            Rout -= p_.vAPF1_f * rapf1_del;
            work_.WriteRelative(p_.mRAPF1, Rout);
            Rout = Rout * p_.vAPF1_f + rapf1_del;

            // Late reverb APF2
            float lapf2_del = work_.ReadRelative(p_.mLAPF2 - p_.dAPF2);
            Lout -= p_.vAPF2_f * lapf2_del;
            work_.WriteRelative(p_.mLAPF2, Lout);
            Lout = Lout * p_.vAPF2_f + lapf2_del;

            float rapf2_del = work_.ReadRelative(p_.mRAPF2 - p_.dAPF2);
            Rout -= p_.vAPF2_f * rapf2_del;
            work_.WriteRelative(p_.mRAPF2, Rout);
            Rout = Rout * p_.vAPF2_f + rapf2_del;

            // Advance buffer
            work_.Advance(1);

            // Upsample back to 48k and apply output volume
            up_L_.Interpolate(Lout * p_.vLOUT_f, &outL[i], &outL[i+1]);
            up_R_.Interpolate(Rout * p_.vROUT_f, &outR[i], &outR[i+1]);
        }
    }

    // Reset state
    void Reset() {
        if (work_buf_) work_.Init(work_buf_, work_.Size());
    }

    // Runtime parameter controls (0.0 to 1.0 range)
    void SetInputGain(float gain) {
        // Scale vLIN/vRIN (input volume)
        // PSX default is 0x8000 = 1.0, we allow 0.0 to 2.0
        float scaled = gain * 2.0f;
        p_.vLIN_f = scaled;
        p_.vRIN_f = scaled;
    }

    void SetPreDelay(float amount) {
        // Scale reflection delay offsets (dLSAME, dRSAME, dLDIFF, dRDIFF)
        // Amount 0.5 = original, 0.0 = minimum (half), 1.0 = maximum (double)
        float scale = 0.5f + amount;  // 0.5x to 1.5x
        p_.dLSAME = static_cast<uint16_t>(preset_base_.dLSAME * scale);
        p_.dRSAME = static_cast<uint16_t>(preset_base_.dRSAME * scale);
        p_.dLDIFF = static_cast<uint16_t>(preset_base_.dLDIFF * scale);
        p_.dRDIFF = static_cast<uint16_t>(preset_base_.dRDIFF * scale);
    }

    void SetDecayTime(float decay) {
        // Scale vWALL (reflection feedback)
        // Higher values = longer decay
        // PSX typical: vWALL around 0xB000 to 0xC000 (-0.31 to -0.25)
        // Range: 0.5x (decay=0) -> 1.0x (decay=0.5) -> 3.0x (decay=1.0)
        float wall_scale;
        if (decay < 0.5f) {
            wall_scale = 0.5f + decay;  // 0.5x to 1.0x
        } else {
            wall_scale = 1.0f + (decay - 0.5f) * 4.0f;  // 1.0x to 3.0x
        }
        p_.vWALL_f = preset_base_.vWALL_f * wall_scale;
    }

    void SetDamping(float damp) {
        // Scale vIIR (IIR filter coefficient for HF damping)
        // Higher vIIR = less damping (more HF), lower = more damping (less HF)
        // PSX typical: 0x6000 to 0x7E00 (0.75 to 0.98)
        // Inverse: damp=0 means preserve HF, damp=1 means cut HF
        float iir_scale = 1.0f - (damp * 0.5f);  // 1.0x to 0.5x
        p_.vIIR_f = preset_base_.vIIR_f * iir_scale;
    }

private:
    // Store original preset for runtime scaling
    struct ScaledPreset {
        uint16_t dAPF1, dAPF2;
        uint16_t dLSAME, dRSAME, dLDIFF, dRDIFF;
        uint16_t mLSAME, mRSAME, mLDIFF, mRDIFF;
        uint16_t mLCOMB1, mRCOMB1, mLCOMB2, mRCOMB2;
        uint16_t mLCOMB3, mRCOMB3, mLCOMB4, mRCOMB4;
        uint16_t mLAPF1, mRAPF1, mLAPF2, mRAPF2;
        float vIIR_f, vCOMB1_f, vCOMB2_f, vCOMB3_f, vCOMB4_f;
        float vWALL_f, vAPF1_f, vAPF2_f, vLIN_f, vRIN_f;
        float vLOUT_f, vROUT_f;
    } p_;

    // Base preset values for runtime scaling
    ScaledPreset preset_base_;

    // Scale preset from 44.1kHz to 48kHz
    void ScalePreset(const PsxPreset& src) {
        // Scale delay offsets
        p_.dAPF1 = ScaleDelay(src.dAPF1, io_rate_);
        p_.dAPF2 = ScaleDelay(src.dAPF2, io_rate_);
        p_.dLSAME = ScaleDelay(src.dLSAME, io_rate_);
        p_.dRSAME = ScaleDelay(src.dRSAME, io_rate_);
        p_.dLDIFF = ScaleDelay(src.dLDIFF, io_rate_);
        p_.dRDIFF = ScaleDelay(src.dRDIFF, io_rate_);

        // Scale memory addresses
        p_.mLSAME = ScaleDelay(src.mLSAME, io_rate_);
        p_.mRSAME = ScaleDelay(src.mRSAME, io_rate_);
        p_.mLDIFF = ScaleDelay(src.mLDIFF, io_rate_);
        p_.mRDIFF = ScaleDelay(src.mRDIFF, io_rate_);
        p_.mLCOMB1 = ScaleDelay(src.mLCOMB1, io_rate_);
        p_.mRCOMB1 = ScaleDelay(src.mRCOMB1, io_rate_);
        p_.mLCOMB2 = ScaleDelay(src.mLCOMB2, io_rate_);
        p_.mRCOMB2 = ScaleDelay(src.mRCOMB2, io_rate_);
        p_.mLCOMB3 = ScaleDelay(src.mLCOMB3, io_rate_);
        p_.mRCOMB3 = ScaleDelay(src.mRCOMB3, io_rate_);
        p_.mLCOMB4 = ScaleDelay(src.mLCOMB4, io_rate_);
        p_.mRCOMB4 = ScaleDelay(src.mRCOMB4, io_rate_);
        p_.mLAPF1 = ScaleDelay(src.mLAPF1, io_rate_);
        p_.mRAPF1 = ScaleDelay(src.mRAPF1, io_rate_);
        p_.mLAPF2 = ScaleDelay(src.mLAPF2, io_rate_);
        p_.mRAPF2 = ScaleDelay(src.mRAPF2, io_rate_);

        // Convert coefficients to float
        p_.vIIR_f = CoeffToFloat(src.vIIR);
        p_.vCOMB1_f = CoeffToFloat(src.vCOMB1);
        p_.vCOMB2_f = CoeffToFloat(src.vCOMB2);
        p_.vCOMB3_f = CoeffToFloat(src.vCOMB3);
        p_.vCOMB4_f = CoeffToFloat(src.vCOMB4);
        p_.vWALL_f = CoeffToFloat(src.vWALL);
        p_.vAPF1_f = CoeffToFloat(src.vAPF1);
        p_.vAPF2_f = CoeffToFloat(src.vAPF2);
        p_.vLIN_f = CoeffToFloat(src.vLIN);
        p_.vRIN_f = CoeffToFloat(src.vRIN);
        p_.vLOUT_f = CoeffToFloat(src.vLOUT);
        p_.vROUT_f = CoeffToFloat(src.vROUT);

        // Store base values for runtime scaling
        preset_base_ = p_;
    }

    static uint32_t NextPowerOf2(uint32_t v) {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;
        return v;
    }

    // Work area (single shared buffer: L_half | R_half)
    WorkArea work_;
    int16_t* work_buf_;

    // Resamplers
    Halfband39 down_L_, down_R_;
    Halfband39 up_L_, up_R_;

    // State
    float io_rate_;
    float scale_factor_;
    bool initialized_;
};
