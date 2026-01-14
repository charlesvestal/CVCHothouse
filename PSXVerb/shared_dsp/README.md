# PSXVerb Shared DSP

This folder contains the platform-neutral PSX reverb core (headers only) used by both the Daisy firmware and the VSX-32 JUCE plugin. Treat this as the canonical copy of the DSP code; consumers should include headers from here and avoid duplicating them elsewhere.

Files:
- PsxReverb.h: DSP engine wrapper and block processor
- PsxPreset.h: PSX SPU preset definitions and helpers
- WorkArea.h: circular buffer helper for the PSX-style memory layout
- Halfband39.h: halfband resampler used for 48k↔24k conversion
