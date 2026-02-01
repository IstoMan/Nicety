cmake_minimum_required(VERSION 3.10)
project(nicety)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find SDL3
find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(SDL3_ttf REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MUPDF REQUIRED mupdf)

# Define source files
file(GLOB_RECURSE SOURCES src/*.c vendor/*.c)
file(GLOB_RECURSE HEADERS src/*.h vendor/*.h)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# include directories
include_directories(vendor)
target_include_directories(${PROJECT_NAME} PRIVATE
${CMAKE_SOURCE_DIR}/vendor
${MUPDF_INCLUDE_DIRS})

# Create executable
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -g)
# target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic
# -g -fsanitize=address)

# Link SDL3
target_link_libraries(
  ${PROJECT_NAME} PRIVATE
  SDL3::SDL3
  SDL3_image::SDL3_image
  SDL3_ttf::SDL3_ttf
  ${MUPDF_LIBRARIES}
)
