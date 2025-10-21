cmake_minimum_required(VERSION 3.14)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../libDaisy/cmake/toolchains/stm32h750xx.cmake)

project(PSXVerb)

set(TARGET_NAME PSXVerb)

add_subdirectory(../../libDaisy libDaisy)
add_subdirectory(../../DaisySP DaisySP)
target_sources(${TARGET_NAME} PRIVATE
    PSXVerb.cpp
    ../hothouse.cpp
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(${TARGET_NAME} PRIVATE
    daisy
    DaisySP
)

set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    SUFFIX ".elf"
    LINK_DEPENDS ${LINKER_SCRIPT}
)
