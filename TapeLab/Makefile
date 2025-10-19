# Project Name
TARGET = tapelab

# Use DaisySP LGPL modules (reverbsc, tone, atone, etc.)
USE_DAISYSP_LGPL=1

# Sources and Hothouse support files
CPP_SOURCES = TapeLab.cpp ../hothouse.cpp
C_INCLUDES = -I..

# Library Locations
LIBDAISY_DIR = ../../HothouseExamples/libDaisy
DAISYSP_DIR = ../../HothouseExamples/DaisySP

# Core location and generic Makefile
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Global helpers
include ../Makefile
