cmake_minimum_required(VERSION 3.10)

project(TemStream VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(RECORD_PULSE_AUDIO "Record windows with pulse audio" ON)

set(BUILD_SANDBOX OFF)
set(SUPPORT_JPG ON)
set(SUPPORT_PNG ON)
set(SUPPORT_WEBP ON)
set(BUILD_SHARED_LIBS OFF)

configure_file(include/TemStreamConfig.h.in ../include/TemStreamConfig.h)

find_package(Freetype REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

add_subdirectory(cereal)
add_subdirectory(SDL)
add_subdirectory(SDL_image)
add_subdirectory(opus)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
add_library(ImGui STATIC)

target_sources(ImGui PRIVATE
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp)

target_compile_definitions(ImGui PUBLIC -DIMGUI_USE_WCHAR32 -DIMGUI_ENABLE_FREETYPE)

target_include_directories(ImGui PUBLIC 
  "${SDL2_SOURCE_DIR}/include"
  "${IMGUI_DIR}" 
  "${IMGUI_DIR}/backends"
  "${IMGUI_DIR}/misc/freetype"
  "${IMGUI_DIR}/misc/cpp"
  ${FREETYPE_INCLUDE_DIRS})

target_link_libraries(ImGui PRIVATE SDL2)

set(SOURCES 
    src/addrinfo.cpp 
    src/audio.cpp
    src/clientConnection.cpp
    src/colors.cpp
    src/connection.cpp 
    src/gui.cpp
    src/logger.cpp
    src/main.cpp
    src/memoryStream.cpp 
    src/misc.cpp
    src/query.cpp
    src/serverConnection.cpp
    src/socket.cpp
    src/stream.cpp
    src/streamDisplay.cpp
    src/workQueue.cpp)

set(HEADERS include/addrinfo.hpp
    include/clientConnection.hpp
    include/colors.hpp
    include/connection.hpp
    include/gui.hpp
    include/logger.hpp
    include/main.hpp
    include/memoryStream.hpp
    include/message.hpp
    include/query.hpp
    include/serverConnection.hpp
    include/socket.hpp
    include/streamDisplay.hpp
    include/TemStreamConfig.h)

if(RECORD_PULSE_AUDIO)
  set(AUDIO_SOURCE src/audioPulse.cpp)
endif()

add_executable(TemStream ${SOURCES} ${HEADERS} ${AUDIO_SOURCE})

if(MSVC)
  target_compile_options(TemStream PRIVATE /W4 /WX)
else()
  target_compile_options(TemStream PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_include_directories(TemStream PRIVATE 
  "${PROJECT_SOURCE_DIR}/include"
  "${CEREAL_SOURCE_DIR}/include"
  "${PROJECT_SOURCE_DIR}/imgui")

target_link_libraries(TemStream PRIVATE 
  SDL2 
  SDL2_image 
  cereal 
  opus
  ImGui 
  ${FREETYPE_LIBRARIES})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(TemStream PRIVATE Threads::Threads)