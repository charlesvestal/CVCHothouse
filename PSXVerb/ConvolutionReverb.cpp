#include "daisy_seed.h"
#include "ConvolutionReverb.h"

using namespace daisy;

// Static SDRAM buffers for IR spectra (allocated at compile time)
// Head: 32 partitions × 256 complex samples × 2 (real+imag) = 16,384 floats = 65 KB
// Tail: 192 partitions × 2048 complex samples × 2 = 786,432 floats = 3.15 MB
// Total: ~3.2 MB in SDRAM (well within 64MB budget)
DSY_SDRAM_BSS float g_head_spectra[32][512];     // [partition][real256 + imag256]
DSY_SDRAM_BSS float g_tail_spectra[192][4096];   // [partition][real2048 + imag2048]
