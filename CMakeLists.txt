cmake_minimum_required(VERSION 3.10)

project(TemStream VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(COMPILE_CLIENT "Compile TemStream client" ON)
option(COMPILE_SERVER "Compile TemStream server" ON)
option(COMPILE_CHAT_TEST "Compile TemStream chat test")
option(CUSTOM_ALLOCATOR "Use custom allocator instead of malloc" ON)
option(JSON_CONFIG "Serialize client configurations to JSON" ON)
option(VPX_ENCODING "Use libvpx to encode video")
option(OPENH264_ENCODING "Use OpenH264 to encode video" ON)

if(WIN32)
  message("Compiling for Windows")
else()
  message("Compiling for Linux")
endif()

set(BUILD_DOC OFF)
set(BUILD_SANDBOX OFF)
set(SKIP_PERFORMANCE_COMPARISON ON)
set(SUPPORT_JPG ON)
set(SUPPORT_PNG ON)
set(SUPPORT_WEBP OFF)
set(BUILD_SHARED_LIBS OFF)

configure_file(include/TemStreamConfig.h.in ../include/TemStreamConfig.h)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(WIN32)
  find_package(SDL2 CONFIG REQUIRED)
  find_package(sdl2-image CONFIG REQUIRED)
else()
  add_subdirectory(SDL)
  add_subdirectory(SDL_image)
endif()

add_subdirectory(cereal)
add_subdirectory(opus)

find_package(Freetype REQUIRED)
find_package(OpenSSL REQUIRED)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
add_library(ImGui STATIC)

target_sources(ImGui PRIVATE
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
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

if(WIN32)
  target_link_libraries(ImGui PRIVATE SDL2::SDL2 SDL2::SDL2main)
else()
  target_link_libraries(ImGui PRIVATE SDL2)
endif()

set(CLIENT_SOURCES
  src/audioSource.cpp
  src/clientConnection.cpp
  src/clientConfiguration.cpp
  src/clientMisc.cpp
  src/colors.cpp
  src/gui.cpp
  src/query.cpp
  src/sdl.cpp
  src/streamDisplay.cpp
  src/videoSource.cpp
  src/work.cpp
  )

if(WIN32)
  set(AUDIO_SOURCE src/audioWASAPI.cpp)
  set(VIDEO_SOURCE src/video_windows.cpp)
else()
  set(AUDIO_SOURCE src/audioPulse.cpp)
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
    src/access.cpp
    src/addrinfo.cpp 
    src/base64.cpp
    src/byteList.cpp
    src/connection.cpp 
    src/guid.cpp
    src/logger.cpp
    src/main.cpp
    src/memoryStream.cpp 
    src/misc.cpp
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

  if(WIN32)
  else()
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
    if(WIN32)
      find_library(OPENH264_LIBRARY openh264)
      target_link_libraries(TemStream PRIVATE ${OPENH264_LIBRARY})
    else()
      target_link_libraries(TemStream PRIVATE
        ${PROJECT_SOURCE_DIR}/openh264/libcommon.a
        ${PROJECT_SOURCE_DIR}/openh264/libconsole_common.a
        ${PROJECT_SOURCE_DIR}/openh264/libdecoder.a
        ${PROJECT_SOURCE_DIR}/openh264/libencoder.a
        ${PROJECT_SOURCE_DIR}/openh264/libopenh264.a
        ${PROJECT_SOURCE_DIR}/openh264/libprocessing.a)
    endif()
  endif()

  if(MSVC)
    target_compile_options(TemStream PRIVATE /WX)
    target_link_libraries(TemStream PRIVATE wsock32 ws2_32)
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

  find_package(OpenCV REQUIRED)

  target_link_libraries(TemStream PRIVATE 
    cereal
    opus
    ImGui 
    ${FREETYPE_LIBRARIES}
    ${OpenCV_LIBS})

  if(WIN32)
    target_link_libraries(TemStream PRIVATE SDL2::SDL2 SDL2::SDL2main SDL2::SDL2_image)
  else()
    target_link_libraries(TemStream PRIVATE SDL2_image)
  endif()

  target_link_libraries(TemStream PRIVATE OpenSSL::SSL OpenSSL::Crypto)

  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  target_link_libraries(TemStream PRIVATE Threads::Threads)
  
endif()

# Compile Server
if(COMPILE_SERVER)
  set(SERVER_SOURCES
    src/serverConnection.cpp
    src/serverConfiguration.cpp
  )
  add_executable(TemStreamServer ${SOURCES} ${SERVER_SOURCES})

  if(MSVC)
    target_compile_options(TemStreamServer PRIVATE /WX)
    target_link_libraries(TemStreamServer PRIVATE wsock32 ws2_32)
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

  target_link_libraries(TemStreamServer PRIVATE cereal OpenSSL::SSL OpenSSL::Crypto)

  if(WIN32)
  else()
    target_link_libraries(TemStreamServer PRIVATE dl)
  endif()

  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  target_link_libraries(TemStreamServer PRIVATE Threads::Threads)
endif()

# Compile Chat Test
if(COMPILE_CHAT_TEST)
  add_executable(TemStreamChatTest ${SOURCES} tests/chatTester.cpp)

  if(MSVC)
    target_compile_options(TemStreamChatTest PRIVATE /WX)
    target_link_libraries(TemStreamChatTest PRIVATE wsock32 ws2_32)
  else()
    target_compile_options(TemStreamChatTest PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()

  if(CUSTOM_ALLOCATOR)
    target_compile_definitions(TemStreamChatTest PRIVATE -DTEMSTREAM_USE_CUSTOM_ALLOCATOR)
  endif()

  target_compile_definitions(TemStreamChatTest PRIVATE -DTEMSTREAM_CHAT_TEST)

  target_include_directories(TemStreamChatTest PRIVATE 
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/tests"
    "${CEREAL_SOURCE_DIR}/include")

  target_link_libraries(TemStreamChatTest PRIVATE cereal)

  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  target_link_libraries(TemStreamChatTest PRIVATE Threads::Threads)
endif()