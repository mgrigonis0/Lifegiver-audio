cmake_minimum_required(VERSION 3.19)

# Legacy Max SDK scripts: the folder name "lifegiver_tilde" becomes the external "lifegiver~".
include(${MAX_SDK_BASE}/script/max-pretarget.cmake)

include_directories("${C74_INCLUDES}")

add_library(${PROJECT_NAME} MODULE lifegiver_tilde.cpp
    ../../engine/version.h ../../engine/state.h ../../engine/ring.h ../../engine/smooth.h ../../engine/dsp.h
    ../../engine/fft.h ../../engine/measure.h ../../engine/capture.h ../../engine/modules.h ../../engine/profile.h ../../engine/chain.h
    ../../engine/brain.h ../../engine/processor.h ../../engine/controller.h)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_compile_options(${PROJECT_NAME} PRIVATE -O3)

include(${MAX_SDK_BASE}/script/max-posttarget.cmake)
