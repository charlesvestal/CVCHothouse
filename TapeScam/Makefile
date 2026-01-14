# Project Name
TARGET = tapescam

# Use DaisySP LGPL modules where needed
USE_DAISYSP_LGPL=1

# Use C++17 for std::clamp and other modern features
CPP_STANDARD = -std=gnu++17

# Sources and shared Hothouse support files
CPP_SOURCES = TapeScam.cpp shared_dsp/GainStageModule.cpp shared_dsp/TapeSatModule.cpp shared_dsp/WowFlutterModule.cpp shared_dsp/HissDropModule.cpp shared_dsp/DropoutModule.cpp shared_dsp/ToneModule.cpp shared_dsp/LoFiCompressor.cpp ../hothouse.cpp
C_INCLUDES = -I.. -Ishared_dsp

# Library Locations
LIBDAISY_DIR = ../../HothouseExamples/libDaisy
DAISYSP_DIR = ../../HothouseExamples/DaisySP

# Core location and generic Makefile
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Global helpers
include ../Makefile
