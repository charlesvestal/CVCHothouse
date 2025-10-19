# Project Name
TARGET = tapescam

# Use DaisySP LGPL modules where needed
USE_DAISYSP_LGPL=1

# Sources and shared Hothouse support files
CPP_SOURCES = TapeScam.cpp GainStageModule.cpp TapeSatModule.cpp WowFlutterModule.cpp HissDropModule.cpp ToneModule.cpp ReverbModule.cpp ../hothouse.cpp
C_INCLUDES = -I..

# Library Locations
LIBDAISY_DIR = ../../HothouseExamples/libDaisy
DAISYSP_DIR = ../../HothouseExamples/DaisySP

# Core location and generic Makefile
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Global helpers
include ../Makefile
