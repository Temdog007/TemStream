cmake_minimum_required(VERSION 3.10)

project(TemStream VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

configure_file(include/TemStreamConfig.h.in ../include/TemStreamConfig.h)

add_subdirectory(cereal)
add_subdirectory(SDL)

add_library(ImGui 
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_impl_sdl.cpp
    imgui/imgui_impl_sdlrenderer.cpp
    imgui/imgui_stdlib.cpp
    imgui/imgui.h
    imgui/imgui_internal.h
    imgui/imgui_impl_sdl.h
    imgui/imgui_impl_sdlrenderer.h
    imgui/imgui_stdlib.h)

target_include_directories(ImGui PRIVATE "${SDL2_SOURCE_DIR}/include")

target_link_libraries(ImGui PRIVATE SDL2)

set(SOURCES src/addrinfo.cpp 
    src/clientPeer.cpp
    src/colors.cpp
    src/gui.cpp
    src/main.cpp
    src/memoryStream.cpp 
    src/misc.cpp
    src/peer.cpp 
    src/producer.cpp
    src/serverPeer.cpp
    src/socket.cpp
    src/streamDisplay.cpp)

set(HEADERS include/addrinfo.hpp
    include/clientPeer.hpp
    include/colors.hpp
    include/main.hpp
    include/memoryStream.hpp
    include/message.hpp
    include/peer.hpp
    include/producer.hpp
    include/serverPeer.hpp
    include/socket.hpp
    include/streamDisplay.hpp
    include/TemStreamConfig.h)

add_executable(TemStream ${SOURCES} ${HEADERS})

if(MSVC)
  target_compile_options(TemStream PRIVATE /W4 /WX)
else()
  target_compile_options(TemStream PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_include_directories(TemStream PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_include_directories(TemStream PRIVATE "${SDL2_SOURCE_DIR}/include")
target_include_directories(TemStream PRIVATE "${CEREAL_SOURCE_DIR}/include")
target_include_directories(TemStream PRIVATE "${PROJECT_SOURCE_DIR}/imgui")

target_link_libraries(TemStream PRIVATE SDL2 cereal ImGui)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(TemStream PRIVATE Threads::Threads)