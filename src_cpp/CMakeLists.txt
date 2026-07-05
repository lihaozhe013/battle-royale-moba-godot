cmake_minimum_required(VERSION 3.17)

set(GODOTCPP_TARGET "template_debug" CACHE STRING "")
set(GODOTCPP_API_VERSION "4.7" CACHE STRING "")
set(GODOTCPP_PRECISION "single" CACHE STRING "")

project(topdown_sim LANGUAGES CXX)

add_subdirectory(godot-cpp)

add_library(topdown_sim SHARED
    register_types.cpp
    sim_server.cpp
    sim/world.cpp
)

target_compile_features(topdown_sim PRIVATE cxx_std_17)

target_include_directories(topdown_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sim
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(topdown_sim PRIVATE godot-cpp)

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/../addons/topdown_sim")
set_target_properties(topdown_sim PROPERTIES
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
