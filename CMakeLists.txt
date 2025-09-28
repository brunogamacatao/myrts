cmake_minimum_required(VERSION 3.10)
project(myrts CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

# Enable warnings and optimizations
add_compile_options(-Wall -Wextra -O2)

# Source files
set(SOURCES
    src/main.cpp
    src/sprite.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Use pkg-config to find Allegro
find_package(PkgConfig REQUIRED)
pkg_check_modules(ALLEGRO REQUIRED allegro-5 allegro_font-5 allegro_image-5)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${ALLEGRO_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json ${ALLEGRO_LIBRARIES})