cmake_minimum_required(VERSION 3.8)

project(RLGym)

add_library(RLGym STATIC
    src/RLGymCPP/Gamestates/GameState.cpp
    src/RLGymCPP/Gamestates/Player.cpp
    src/RLGymCPP/Gamestates/StateUtil.cpp
    src/RLGymCPP/BallPrediction/PredictionContext.cpp
    src/RLGymCPP/ActionParsers/DefaultAction.cpp
    src/RLGymCPP/Gamestates/StateUtil.cpp
)
target_include_directories(RLGym PUBLIC src/)
target_compile_features(RLGym PUBLIC cxx_std_20)

if(ROCKETFORGE_ENABLE_V3)
    add_subdirectory(RocketSimV3)
else()
    set(ROCKETSIM_V3_REASON "RocketSim v3 isn't built: this package's bot uses only RocketSim v2.")
endif()
add_subdirectory(Sim)
add_subdirectory(RocketSim)
target_link_libraries(RLGym PUBLIC RocketForgeSim)
