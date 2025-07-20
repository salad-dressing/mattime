cmake_minimum_required(VERSION 3.10)
project(mattime C)
set(CMAKE_C_STANDARD 99)

# Define source files
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
file(GLOB SRC_FILES ${SRC_DIR}/*.c)

# Define include files
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# Add executable target
add_executable(mattime ${SRC_FILES})

# Configure target
target_include_directories(mattime PRIVATE ${INCLUDE_DIR})
target_link_libraries(mattime sqlite3 m)
target_compile_options(mattime PRIVATE
    $<$<CONFIG:Debug>:-g -Wall -Wextra -Wpedantic>
    $<$<CONFIG:Release>:-O2 -Wall>
)
