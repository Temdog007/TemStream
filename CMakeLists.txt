cmake_minimum_required(VERSION 3.10)

project(TemStream VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(COMPILE_CLIENT "Compile TemStream client" ON)
option(COMPILE_SERVER "Compile TemStream server" ON)
option(CUSTOM_ALLOCATOR "Use custom allocator instead of malloc" ON)
option(JSON_CONFIG "Serialize client configurations to JSON" ON)
option(RECORD_PULSE_AUDIO "Record windows' audio with pulse audio" ON)
option(XCB_RECORD "Record windows with XCB" ON)
option(VPX_ENCODING "Use libvpx to encode video" ON)
option(OPENH264_ENCODING "Use OpenH264 to encode video" OFF)
option(USE_OPEN_CV "Use OpenCV library" ON)

set(OPENH264_ENCODING ON)
set(VPX_ENCODING OFF)
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

set(CLIENT_SOURCES
  src/audio.cpp
  src/clientConnection.cpp
  src/clientConfiguration.cpp
  src/clientMisc.cpp
  src/gui.cpp
  src/query.cpp
  src/sdl.cpp
  src/streamDisplay.cpp
  src/video.cpp
  src/work.cpp
  )
if(RECORD_PULSE_AUDIO)
  set(AUDIO_SOURCE src/audioPulse.cpp)
endif()

if(XCB_RECORD)
  set(VIDEO_SOURCE src/video_x11.cpp)
endif()

if(VPX_ENCODING)
  set(ENCODING_SOURCE src/vpx.cpp)
endif()

if(OPENH264_ENCODING)
  set(ENCODING_SOURCE 
    src/openH264.cpp
  )
endif()

set(SOURCES 
    src/addrinfo.cpp 
    src/byteList.cpp
    src/connection.cpp 
    src/logger.cpp
    src/main.cpp
    src/memoryStream.cpp 
    src/misc.cpp
    src/stream.cpp
    src/socket.cpp
    src/time.cpp
  )

# Compile client
if(COMPILE_CLIENT)
  add_executable(TemStream 
    ${SOURCES} 
    ${CLIENT_SOURCES} 
    ${AUDIO_SOURCE} 
    ${VIDEO_SOURCE} 
    ${ENCODING_SOURCE})

  if(XCB_RECORD)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
    include(FindXCB)
    find_package(XCB REQUIRED)
    target_include_directories(TemStream PRIVATE ${X11_XCB_INCLUDE_DIR})
    target_link_libraries(TemStream PRIVATE XCB::XCB)
  endif()

  if(VPX_ENCODING)
    target_link_libraries(TemStream PRIVATE vpx)
  endif()

  if(OPENH264_ENCODING)
    target_include_directories(TemStream PRIVATE ${PROJECT_SOURCE_DIR})
    target_link_libraries(TemStream PRIVATE
    ${PROJECT_SOURCE_DIR}/openh264/libcommon.a
    ${PROJECT_SOURCE_DIR}/openh264/libconsole_common.a
    ${PROJECT_SOURCE_DIR}/openh264/libdecoder.a
    ${PROJECT_SOURCE_DIR}/openh264/libencoder.a
    ${PROJECT_SOURCE_DIR}/openh264/libopenh264.a
    ${PROJECT_SOURCE_DIR}/openh264/libprocessing.a)
  endif()

  if(MSVC)
    target_compile_options(TemStream PRIVATE /W4 /WX)
  else()
    target_compile_options(TemStream PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()

  target_include_directories(TemStream PRIVATE 
    "${PROJECT_SOURCE_DIR}/include"
    "${CEREAL_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/imgui")

  if(JSON_CONFIG)
    target_compile_definitions(TemStream PRIVATE -DTEMSTREAM_CLIENT_JSON_CONFIG)
  endif()

  if(CUSTOM_ALLOCATOR)
    target_compile_definitions(TemStream PRIVATE -DTEMSTREAM_USE_CUSTOM_ALLOCATOR)
  endif()

  if(OPENH264_ENCODING)
    target_compile_definitions(TemStream PRIVATE -DTEMSTREAM_USE_OPENH264)
  endif()

  if(USE_OPEN_CV)
    find_package(OpenCV REQUIRED)
    target_compile_definitions(TemStream PRIVATE -DTEMSTREAM_USE_OPENCV)
    target_link_libraries(TemStream PRIVATE ${OpenCV_LIBS})
  endif()

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

  set(SERVER_SOURCES
    src/serverConnection.cpp
    src/serverConfiguration.cpp
    )
endif()

# Compile Server
if(COMPILE_SERVER)
  add_executable(TemStreamServer ${SOURCES} ${SERVER_SOURCES})

  if(MSVC)
    target_compile_options(TemStreamServer PRIVATE /W4 /WX)
  else()
    target_compile_options(TemStreamServer PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()

  if(CUSTOM_ALLOCATOR)
    target_compile_definitions(TemStreamServer PRIVATE -DTEMSTREAM_USE_CUSTOM_ALLOCATOR)
  endif()

  target_compile_definitions(TemStreamServer PRIVATE -DTEMSTREAM_SERVER)

  target_include_directories(TemStreamServer PRIVATE 
    "${PROJECT_SOURCE_DIR}/include"
    "${CEREAL_SOURCE_DIR}/include")

  target_link_libraries(TemStreamServer PRIVATE cereal)

  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  target_link_libraries(TemStreamServer PRIVATE Threads::Threads)
endif()