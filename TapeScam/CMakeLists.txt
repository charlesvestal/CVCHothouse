cmake_minimum_required(VERSION 3.15)
project(TapeScamFile)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
  DaisySP
  GIT_REPOSITORY https://github.com/electro-smith/DaisySP.git
  GIT_TAG master
)
set(FETCHCONTENT_UPDATES_DISCONNECTED_DAISYSP ON)
FetchContent_MakeAvailable(DaisySP)

set(TAPESCAM_SHARED_DSP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shared_dsp")
set(LIBDAISY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../extern/libDaisy")

add_subdirectory(${LIBDAISY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/libDaisy)

add_executable(TapeScam
  TapeScam.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../hothouse.cpp
  ${TAPESCAM_SHARED_DSP_DIR}/GainStageModule.cpp
  ${TAPESCAM_SHARED_DSP_DIR}/TapeSatModule.cpp
  ${TAPESCAM_SHARED_DSP_DIR}/WowFlutterModule.cpp
  ${TAPESCAM_SHARED_DSP_DIR}/HissDropModule.cpp
  ${TAPESCAM_SHARED_DSP_DIR}/ToneModule.cpp
  ${TAPESCAM_SHARED_DSP_DIR}/LoFiCompressor.cpp
)
if(NOT DEFINED DAISYSP_SOURCE_DIR)
  message(FATAL_ERROR "DAISYSP_SOURCE_DIR not defined; DaisySP FetchContent failed?")
endif()
set(DAISYSP_LGPL_DIR "${DAISYSP_SOURCE_DIR}/DaisySP-LGPL/Source")
target_sources(TapeScam PRIVATE
  "${DAISYSP_LGPL_DIR}/Effects/reverbsc.cpp"
  "${DAISYSP_LGPL_DIR}/Filters/atone.cpp"
  "${DAISYSP_LGPL_DIR}/Filters/tone.cpp"
)
target_include_directories(TapeScam PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${TAPESCAM_SHARED_DSP_DIR}
  ${LIBDAISY_DIR}/src
  ${LIBDAISY_DIR}/src/sys
  ${LIBDAISY_DIR}/src/usbh
  ${LIBDAISY_DIR}/Drivers/CMSIS/DSP/Include
  ${LIBDAISY_DIR}/Drivers/CMSIS/Include
  ${LIBDAISY_DIR}/Drivers/CMSIS-Device/ST/STM32H7xx/Include
  ${LIBDAISY_DIR}/Drivers/CMSIS_5/CMSIS/Core/Include
  ${LIBDAISY_DIR}/Drivers/STM32H7xx_HAL_Driver/Inc
  ${LIBDAISY_DIR}/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
  ${LIBDAISY_DIR}/Core/Inc
  ${LIBDAISY_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
  ${LIBDAISY_DIR}/Middlewares/ST/STM32_USB_Host_Library/Core/Inc
  ${LIBDAISY_DIR}/Middlewares/Third_Party/FatFs/src
  "${DAISYSP_SOURCE_DIR}/Source"
  "${DAISYSP_SOURCE_DIR}/Source/Utility"
  "${DAISYSP_LGPL_DIR}"
)
target_link_libraries(TapeScam PRIVATE DaisySP daisy)

target_compile_definitions(TapeScam PRIVATE
  FILEIO_ENABLE_FATFS_READER
  STM32H750xx
  USE_HAL_DRIVER
)
