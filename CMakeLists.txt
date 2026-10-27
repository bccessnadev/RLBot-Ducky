cmake_minimum_required(VERSION 3.22)

project(RocketForge_Ducky)

set(DEVICE_FILE "${CMAKE_SOURCE_DIR}/rlbot/device.txt")
include(cmake/ReadDevice.cmake)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DEVICE_FILE}")
if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo" CACHE STRING "Supported configurations" FORCE)
endif()

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(RLBOT ON CACHE BOOL "Build the portable RLBot runtime" FORCE)

set(RLBOT_CPP_BUILD_BENCHMARK OFF CACHE BOOL "Exclude RLBotCPP benchmark" FORCE)

set(ROCKETFORGE_ENABLE_V3 ON CACHE STRING "Build RocketSim v3 for a bot that trains or predicts with it" FORCE)

# RLBot's shared PyTorch archive uses the pre-C++11 libstdc++ string ABI.
if(NOT WIN32)
    add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
endif()

add_subdirectory(ForgeTraining/RLGym)
add_subdirectory(ForgeTraining/GigaLearn/GigaLearnInfer)
add_subdirectory(Bots)
add_subdirectory(RocketForgeRLBot)

target_compile_definitions(RLBotCore PRIVATE
    RLBOT_DEFAULT_AGENT="ducky-agent"
)
