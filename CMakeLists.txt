cmake_minimum_required(VERSION 3.10)
project(Resizer VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# Executable for GUI version
add_executable(resizer_gui ${SRC_FILES})

target_link_libraries(resizer_gui user32 comctl32 gdi32 pdh psapi)

# Copy resources (icons) to output directory
add_custom_command(TARGET resizer_gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:resizer_gui>/resources)

# Ensure we can use Windows headers
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0600)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
