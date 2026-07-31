cmake_minimum_required(VERSION 3.21)
project(archipelago LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)

add_executable(archipelago_sim src/main.cpp)

target_link_libraries(archipelago_sim PRIVATE
    SDL3::SDL3
    GLEW::GLEW
    OpenGL::GL
    imgui::imgui
    glm::glm
    Jolt::Jolt
)
