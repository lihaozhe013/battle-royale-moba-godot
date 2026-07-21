cmake_minimum_required(VERSION 3.17)

set(GODOTCPP_TARGET "template_release" CACHE STRING "")
set(GODOTCPP_API_VERSION "4.7" CACHE STRING "")
set(GODOTCPP_PRECISION "single" CACHE STRING "")
set(GODOTCPP_DEPRECATED OFF CACHE BOOL "")
set(GODOTCPP_GENERATE_TEMPLATE_GET_NODE OFF CACHE BOOL "")
set(GODOTCPP_BUILD_PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/build_profile.json" CACHE PATH "")

project(battle_royale_sim LANGUAGES CXX)

add_subdirectory(godot-cpp)

add_library(battle_royale_sim SHARED
    register_types.cpp
    sim_server.cpp
    sim/world.cpp
    sim/snapshot_builder.cpp
    sim/snapshot_bindings.cpp
    sim/heroes/hero_registry.cpp
    sim/skills/skill_registry.cpp
)

target_compile_features(battle_royale_sim PRIVATE cxx_std_17)

target_include_directories(battle_royale_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sim
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(battle_royale_sim PRIVATE godot-cpp)

set_property(TARGET battle_royale_sim PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/../addons/battle_royale_sim")
set_target_properties(battle_royale_sim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${OUTPUT_DIR}"
)
