# Project Name
TARGET = PSXVerb

# Build directory
BUILD_DIR = build

# Use DaisySP LGPL modules (tone, atone filters)
USE_DAISYSP_LGPL=1

# Sources
CPP_SOURCES = PSXVerb_SPU.cpp ../hothouse.cpp

# Library Locations
LIBDAISY_DIR = ../../HothouseExamples/libDaisy
DAISYSP_DIR = ../../HothouseExamples/DaisySP

# Core location
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core

# Includes
C_INCLUDES = -I..

# Compiler Flags
OPT = -O3

# C++ Standard
CPP_STANDARD = -std=gnu++17

# Optimization and debugging
DEBUG = 0
USE_BOOTLOADER = 1

include $(SYSTEM_FILES_DIR)/Makefile

# Global helpers
include ../Makefile

# Serial bootloader flashing (alternative to DFU)
program-serial: $(BUILD_DIR)/$(TARGET).bin
	dfu-util -a 0 -s 0x08000000:leave -D $(BUILD_DIR)/$(TARGET).bin -d ,0483:df11
