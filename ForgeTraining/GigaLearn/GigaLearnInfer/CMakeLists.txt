cmake_minimum_required (VERSION 3.10)

project("GigaLearnInfer")

# Add all headers and code files

set(FILES_SRC
    src/private/Agent.cpp
    src/private/AgentSetupSnapshot.cpp
    src/private/InferUnit.cpp
    src/private/ModelManager.cpp
    src/private/Models.cpp
)

add_library(GigaLearnInfer STATIC ${FILES_SRC})
target_compile_definitions(GigaLearnInfer PUBLIC -DGIGALEARN_INFER_STATIC)
target_compile_definitions(GigaLearnInfer PRIVATE -DWITHIN_GGL)
target_compile_definitions(GigaLearnInfer PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_include_directories(GigaLearnInfer PUBLIC
    "src/public/"
    "../libsrc/json")

include("${CMAKE_SOURCE_DIR}/cmake/PortableTorch.cmake")
rocketforge_link_portable_torch(GigaLearnInfer)

# Set C++ version to 20
set_target_properties(GigaLearnInfer PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(GigaLearnInfer PROPERTIES CXX_STANDARD 20)

target_link_libraries(GigaLearnInfer PUBLIC RLGym)
