cmake_minimum_required(VERSION 3.10)

project(TemStream VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

configure_file(include/TemStreamConfig.h.in ../include/TemStreamConfig.h)

add_subdirectory(cereal)
add_subdirectory(SDL)

set(SOURCES src/addrinfo.cpp 
    src/main.cpp
    src/misc.cpp 
    src/peer.cpp 
    src/producer.cpp
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp)

set(HEADERS include/addrinfo.hpp
    include/main.hpp
    include/message.hpp
    include/peer.hpp
    include/producer.hpp
    include/TemStreamConfig.h
    imgui/imgui.h)

add_executable(TemStream ${SOURCES} ${HEADERS})

target_include_directories(TemStream PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_include_directories(TemStream PRIVATE "${SDL2_SOURCE_DIR}/include")
target_include_directories(TemStream PRIVATE "${SDL2_SOURCE_DIR}/imgui")
target_include_directories(TemStream PRIVATE "${CEREAL_SOURCE_DIR}/include")

target_link_libraries(TemStream PRIVATE cereal)