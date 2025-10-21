# PSXVerb - Non-Uniform Partitioned Convolution Reverb

## Overview

This implements full-length PS1 impulse response convolution (389,646 samples = 8.1 seconds @ 48kHz) using a two-tier non-uniform partitioned convolution architecture with frequency-domain delay lines (FDL) and overlap-add (OLA).

## Architecture

### Two-Tier Processing

**HEAD (Early Reflections)**
- Hop size: L_head = 256 samples
- FFT size: N_head = 512 samples
- Covers: First 2048 samples (~42.7ms) of IR
- Latency: ~5.3ms
- Processing rate: ~187.5 blocks/second (48kHz / 256)
- Partitions: 8 partitions × 256 samples each

**TAIL (Long Decay)**
- Hop size: L_tail = 4096 samples
- FFT size: N_tail = 8192 samples
- Covers: Samples 2048 to end (remaining ~8+ seconds)
- Latency: ~85.3ms
- Processing rate: ~11.72 blocks/second (48kHz / 4096)
- Partitions: ~95 partitions × 4096 samples each

### Crossfade

At the 2048-sample boundary between head and tail:
- Head fades OUT over last 256 samples (raised cosine: 1 → 0)
- Tail fades IN over first 256 samples (raised cosine: 0 → 1)
- Prevents seam artifacts at the transition

## Hardware Callback Integration

The hardware audio callback runs at **16 samples** (maintaining low latency for UI/controls), but internally stages to larger blocks:

1. **Input FIFO**: Accumulates incoming 16-sample chunks
2. **Head Engine**: Processes when 256 samples available
   - Reads 256 samples from input FIFO
   - Runs FDL convolution
   - Writes 256 samples to output FIFO
3. **Tail Engine**: Processes when 4096 samples available
   - Reads 4096 samples from input FIFO
   - Runs FDL convolution
   - **ADDS** 4096 samples to output FIFO (mixes with head)
4. **Output FIFO**: Pops 16 samples to DAC each callback

## Algorithm: FDL Overlap-Add (OLA)

For each partition size L with FFT size N = 2L:

```
Input: x[n] (L samples per block)

1. Zero-pad to N: x_padded = [x[0]...x[L-1], 0...0]
2. FFT forward: X = FFT(x_padded)
3. Store X in circular history: X_hist[block_idx mod Q]
4. Frequency-domain accumulation:
   Y = Σ(q=0 to Q-1) H[q] × X_hist[(block_idx - q) mod Q]
   where H[q] = precomputed FFT of IR partition q
5. IFFT (scaled by 1/N): y_time = IFFT(Y) / N
6. OLA: Output y_time[L..2L-1] (discard first L, keep last L)
```

### Critical OLA Invariants

- ✅ Zero-pad input block to N (append L zeros)
- ✅ Forward FFT: NO scaling
- ✅ Inverse FFT: Scale by 1/N ONCE
- ✅ Complex multiply: (a+bi)(c+di) = (ac-bd) + (ad+bc)i
- ✅ Clear accumulator Y each block
- ✅ Discard first L samples, output last L samples
- ✅ Circular buffer indices: (idx) & (size-1) with power-of-2 sizes

## Memory Usage

**SDRAM (30.3 MB / 64 MB = 45.15%)**

Per channel (L+R):
- Head H[q]: 8 partitions × 257 complex × 8 bytes = 16.5 KB
- Head X_hist: 128 slots × 257 complex × 8 bytes = 263 KB
- Tail H[q]: 95 partitions × 4097 complex × 8 bytes = 3.1 MB
- Tail X_hist: 128 slots × 4097 complex × 8 bytes = 4.2 MB

Total per channel: ~7.6 MB
Both channels: ~15.2 MB

IR data (4 IRs): ~12.5 MB

**SRAM (233 KB / 512 KB = 44.45%)**
- FIFOs, working buffers, stack

**Flash (94 KB / 128 KB = 71.68%)**
- Code + twiddle tables

## FFT Implementation

### MinimalFFT.h

Radix-2 Cooley-Tukey FFT for N=512 and N=8192:
- Pre-computed twiddle factors (cosine/sine tables)
- Bit-reversal permutation
- In-place butterfly computation
- Real FFT optimization (pack/unpack via N/2 complex FFT)
- Hermitian symmetry: only store bins [0..N/2]

### Numerical Precision

- Forward FFT: NO scaling
- Inverse FFT: Scale by exactly 1/N
- All operations in 32-bit float
- FPU hardware acceleration on Cortex-M7

## CPU Budget Estimate

**Head Engine** (L=256, N=512):
- FFT: 2 × (N/2 × log₂(N/2)) = 2 × (256 × 8) = 4096 ops
- 8 partitions × 257 complex MACs = 2056 ops
- Total per head block: ~6k ops
- Rate: 187.5 Hz → 1.1M ops/sec

**Tail Engine** (L=4096, N=8192):
- FFT: 2 × (N/2 × log₂(N/2)) = 2 × (4096 × 12) = 98k ops
- 95 partitions × 4097 complex MACs = 389k ops
- Total per tail block: ~487k ops
- Rate: 11.72 Hz → 5.7M ops/sec

**Total: ~6.8M ops/sec on 480 MHz Cortex-M7 with FPU**

Expected CPU usage: **~15-20%** (leaving headroom for UI, etc.)

## Files

- `MinimalFFT.h` - Radix-2 FFT with twiddle tables (N=512, 8192)
- `FDLConvolver.h` - Template class for FDL OLA convolution
- `PartitionedReverb.h` - Two-tier head/tail engine with FIFO staging
- `PSXVerb.cpp` - Main application (unchanged UI)
- `IRData.h` - 4 PS1 IRs (Studio, Church, Dome, Hall)

## Diagnostics

### Mode Toggle (Switch 3)

- **UP**: Diagnostic Bypass (pass-through, verify signal path)
- **MIDDLE**: Full Partitioned Convolution Reverb
- **DOWN**: Diagnostic FIR-1024 (time-domain 1024-tap test)

### LED Indicators

- **LED 1**: Bypass state (off = bypassed, on = active)
- **LED 2**: IR selection (brightness = IR index / 3)

### Testing

To verify the implementation matches the original IR:

1. **Impulse Test**: Send a single impulse (1.0 followed by zeros)
   - Capture output over 8+ seconds
   - Compare to original IR
   - Expected RMS error: < 1e-5 (after crossfade artifacts)

2. **FFT Self-Test**: Forward FFT → Inverse FFT of white noise
   - Should return original signal within floating-point tolerance

3. **FIR Comparison**: Compare 1024-tap time-domain (Diag mode) vs partitioned convolution
   - First 1024 samples should match exactly

## Known Limitations

- **Startup latency**: Output FIFO must fill before reverb output begins (~L_tail = 85ms)
- **IR change glitch**: Reinitializing IR causes brief audio dropout (acceptable for this application)
- **Crossfade artifacts**: Minor amplitude dip at 2048-sample boundary (< -40dB)

## Controls

- **Footswitch 1**: Bypass toggle
- **Knob 1**: IR selection (4 IRs: Studio, Church, Dome, Hall)
- **Knob 6**: Wet/Dry mix
- **Toggle 3**: Processing mode (Bypass / Convolution / FIR-1024)

## Build

```bash
make clean && make
```

Flash to Daisy Seed and test!
