cmake_minimum_required(VERSION 3.10)
project(black_hole_simulation)

set(CMAKE_CXX_STANDARD 17)

# Include + lib paths for MSYS2
include_directories("C:/msys64/mingw64/include")
link_directories("C:/msys64/mingw64/lib")

add_executable(black_hole_simulation 
    src/main.cpp
)

# Use the actual MSYS2 library name
target_link_libraries(black_hole_simulation
    glfw3
    glew32
    opengl32
)
