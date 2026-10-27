cmake_minimum_required(VERSION 3.8)

project(ExportedBot)

add_library(Agents INTERFACE)
add_library(ExportedAgent OBJECT
    "ExportedRuntimeRegistration.cpp"
    "ducky-bot/DuckyAgent/src/DuckyObs.cpp"
)

target_include_directories(ExportedAgent PRIVATE
    "ducky-bot/DuckyAgent/src"
)
target_compile_features(ExportedAgent PRIVATE cxx_std_20)
target_link_libraries(ExportedAgent PUBLIC GigaLearnInfer)
target_sources(Agents INTERFACE $<TARGET_OBJECTS:ExportedAgent>)
