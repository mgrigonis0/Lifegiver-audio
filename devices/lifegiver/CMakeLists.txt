cmake_minimum_required(VERSION 3.19)
project(lifegiver)

# Path to a checkout of https://github.com/Cycling74/max-sdk-base
set(MAX_SDK_BASE "${CMAKE_SOURCE_DIR}/max-sdk-base" CACHE PATH "max-sdk-base checkout")

# bundle metadata used by the SDK's Info.plist template
set(AUTHOR_DOMAIN "com.mgrigonis0")
set(BUNDLE_IDENTIFIER "lifegiver-tilde")
set(GIT_VERSION_TAG "1.0.0")
set(GIT_VERSION_MAJ 1)
set(GIT_VERSION_MIN 0)
set(GIT_VERSION_SUB 0)

add_subdirectory(source/projects/lifegiver_tilde)
