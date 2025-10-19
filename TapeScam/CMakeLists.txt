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

add_executable(TapeScam
  TapeScam.cpp
  GainStageModule.cpp
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
  "${DAISYSP_SOURCE_DIR}/Source"
  "${DAISYSP_SOURCE_DIR}/Source/Utility"
  "${DAISYSP_LGPL_DIR}"
)
target_link_libraries(TapeScam PRIVATE DaisySP)
