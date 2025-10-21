#pragma once

#include <cmath>
#include <cstring>
#include <complex>

/**
 * Minimal, tested radix-2 FFT for N=512 and N=8192
 *
 * CRITICAL RULES:
 * - Forward FFT: NO scaling
 * - Inverse FFT: Scale by 1/N
 * - Complex multiply: (a+bi)(c+di) = (ac-bd) + (ad+bc)i
 */

namespace MinimalFFT {

// Compute twiddle factor on-demand (no global static init)
inline std::complex<float> ComputeTwiddle(size_t k, size_t N) {
    float angle = -2.0f * M_PI * static_cast<float>(k) / static_cast<float>(N);
    return std::complex<float>(cosf(angle), sinf(angle));
}

// Bit-reversal permutation
inline size_t BitReverse(size_t x, size_t log2n) {
    size_t result = 0;
    for (size_t i = 0; i < log2n; ++i) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}

// Generic radix-2 FFT (in-place, Cooley-Tukey)
// Computes twiddles on-the-fly (no precomputed table needed)
template<size_t N>
inline void FFT_Radix2(std::complex<float>* data, bool inverse) {
    constexpr size_t log2n = __builtin_ctz(N);  // log2(N)

    // Bit-reversal permutation
    for (size_t i = 0; i < N; ++i) {
        size_t j = BitReverse(i, log2n);
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }

    // Cooley-Tukey decimation-in-time
    for (size_t s = 1; s <= log2n; ++s) {
        size_t m = 1 << s;       // 2^s
        size_t m2 = m >> 1;      // m/2

        for (size_t k = 0; k < N; k += m) {
            for (size_t j = 0; j < m2; ++j) {
                size_t t_idx = (j * N) / m;  // Twiddle index

                std::complex<float> t = data[k + j + m2];
                std::complex<float> w = ComputeTwiddle(t_idx, N);
                if (inverse) w = std::conj(w);
                t *= w;

                std::complex<float> u = data[k + j];
                data[k + j] = u + t;
                data[k + j + m2] = u - t;
            }
        }
    }

    // Inverse FFT: scale by 1/N
    if (inverse) {
        float scale = 1.0f / static_cast<float>(N);
        for (size_t i = 0; i < N; ++i) {
            data[i] *= scale;
        }
    }
}

// Forward FFT: real input (N floats) -> complex output (N/2+1 complex)
// Input: time_N is N real samples
// Output: freq is N/2+1 complex bins (DC to Nyquist)
// NOTE: Only outputs bins [0..N/2], exploits Hermitian symmetry (freq[N-k] = conj(freq[k]))
template<size_t N>
inline void fft_forward_real(const float* time_N, std::complex<float>* freq) {
    static_assert((N & (N - 1)) == 0, "N must be power of 2");

    // Pack real input into complex (N/2 complex numbers)
    std::complex<float> packed[N / 2];
    for (size_t i = 0; i < N / 2; ++i) {
        packed[i] = std::complex<float>(time_N[2 * i], time_N[2 * i + 1]);
    }

    // FFT on N/2 complex
    FFT_Radix2<N / 2>(packed, false);

    // Unpack using symmetry - only compute [0..N/2]
    freq[0] = packed[0].real() + packed[0].imag();  // DC
    freq[N / 2] = packed[0].real() - packed[0].imag();  // Nyquist

    for (size_t k = 1; k < N / 2; ++k) {
        float angle = -M_PI * k / (N / 2);
        std::complex<float> w(cosf(angle), sinf(angle));

        std::complex<float> z1 = packed[k];
        std::complex<float> z2 = std::conj(packed[N / 2 - k]);

        std::complex<float> sum = z1 + z2;
        std::complex<float> diff = z1 - z2;

        freq[k] = 0.5f * (sum - std::complex<float>(0, 1) * diff * w);
        // Don't store freq[N-k], it's redundant (Hermitian symmetry)
    }
}

// Inverse FFT: complex input (N/2+1) -> real output (N floats)
// Input: freq is N/2+1 complex bins
// Output: time_N is N real samples (scaled by 1/N already)
// NOTE: Exploits Hermitian symmetry (assumes freq[N-k] = conj(freq[k]))
template<size_t N>
inline void fft_inverse_real(const std::complex<float>* freq, float* time_N) {
    static_assert((N & (N - 1)) == 0, "N must be power of 2");

    // Pack complex into N/2 complex
    std::complex<float> packed[N / 2];

    packed[0] = std::complex<float>(freq[0].real(), freq[N / 2].real());

    for (size_t k = 1; k < N / 2; ++k) {
        float angle = M_PI * k / (N / 2);
        std::complex<float> w(cosf(angle), sinf(angle));

        std::complex<float> z1 = freq[k];
        // Use Hermitian symmetry: freq[N-k] = conj(freq[k])
        std::complex<float> z2 = std::conj(freq[k]);

        std::complex<float> sum = z1 + z2;
        std::complex<float> diff = z1 - z2;

        packed[k] = sum + std::complex<float>(0, 1) * diff * w;
    }

    // IFFT on N/2 complex
    FFT_Radix2<N / 2>(packed, true);

    // Unpack real output
    for (size_t i = 0; i < N / 2; ++i) {
        time_N[2 * i] = packed[i].real();
        time_N[2 * i + 1] = packed[i].imag();
    }
}

} // namespace MinimalFFT
