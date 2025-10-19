cmake_minimum_required(VERSION 3.15)
project(TapeLabFile)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
  DaisySP
  GIT_REPOSITORY https://github.com/electro-smith/DaisySP.git
  GIT_TAG master
)
set(FETCHCONTENT_UPDATES_DISCONNECTED_DAISYSP ON)
FetchContent_MakeAvailable(DaisySP)

# libsndfile for reading/writing WAV/AIFF
find_package(SndFile QUIET)
if(NOT SndFile_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SNDFILE REQUIRED sndfile)
endif()

add_executable(TapeLab TapeLab.cpp)
if(NOT DEFINED DAISYSP_SOURCE_DIR)
  message(FATAL_ERROR "DAISYSP_SOURCE_DIR not defined; DaisySP FetchContent failed?")
endif()
set(DAISYSP_LGPL_DIR "${DAISYSP_SOURCE_DIR}/DaisySP-LGPL/Source")
target_sources(TapeLab PRIVATE
  "${DAISYSP_LGPL_DIR}/Effects/reverbsc.cpp"
  "${DAISYSP_LGPL_DIR}/Filters/atone.cpp"
  "${DAISYSP_LGPL_DIR}/Filters/tone.cpp"
)
target_include_directories(TapeLab PRIVATE
  "${DAISYSP_SOURCE_DIR}/Source"
  "${DAISYSP_SOURCE_DIR}/Source/Utility"
  "${DAISYSP_LGPL_DIR}"
)
target_link_libraries(TapeLab PRIVATE DaisySP)
if(SndFile_FOUND)
  target_link_libraries(TapeLab PRIVATE SndFile::sndfile)
else()
  target_include_directories(TapeLab PRIVATE ${SNDFILE_INCLUDE_DIRS})
  target_link_libraries(TapeLab PRIVATE ${SNDFILE_LIBRARIES})
endif()
