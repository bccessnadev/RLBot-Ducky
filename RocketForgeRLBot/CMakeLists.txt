cmake_minimum_required(VERSION 3.8)

project(RLBot)

include(cmake/PrepareCppInterface.cmake)
prepare_cpp_interface(
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/cpp-interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/cpp-interface.patch"
    RLBOT_CPP_SOURCE_DIR)
add_subdirectory("${RLBOT_CPP_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/inc/cpp-interface")

# Torch-linked runtime core.
add_executable(RLBotCore
    src/Runtime/PortableMain.cpp
    src/Runtime/RLBotClient.cpp
    src/Runtime/RLBotCapture.cpp
    src/Runtime/RLBotDevice.cpp
    src/Runtime/RLBotPolicyRuntime.cpp
    src/Runtime/RunRLBot.cpp
)
set_target_properties(RLBotCore PROPERTIES OUTPUT_NAME ${RLBOT_CORE_NAME})
target_include_directories(RLBotCore PRIVATE src)
target_compile_features(RLBotCore PRIVATE cxx_std_20)
target_compile_definitions(RLBotCore PRIVATE
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
    RLBOT_USE_GPU=$<IF:$<BOOL:${RLBOT_USE_GPU}>,1,0>
    RLBOT_HAS_CUDA=$<BOOL:${RLBOT_USE_GPU}>
)
target_link_libraries(RLBotCore PUBLIC GigaLearnInfer Agents RLBotCPP-static)

# Dependency-free launcher which resolves RLBot's shared Torch runtime.
add_executable(RLBot src/Runtime/RLBotLauncher.cpp)
target_compile_features(RLBot PRIVATE cxx_std_20)
target_compile_definitions(RLBot PRIVATE RLBOT_USE_GPU=$<BOOL:${RLBOT_USE_GPU}>)
add_dependencies(RLBot RLBotCore)
if(WIN32)
    target_link_libraries(RLBot PRIVATE shell32)
endif()
