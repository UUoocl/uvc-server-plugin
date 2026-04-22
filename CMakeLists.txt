cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

set(CMAKE_FRAMEWORK_PATH ${CMAKE_FRAMEWORK_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/Frameworks")

project(uvc-server VERSION ${_version})

if(APPLE)
  add_link_options("-F${CMAKE_CURRENT_SOURCE_DIR}/Frameworks")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)
include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  set(WrapOpenGL_AGL "/System/Library/Frameworks/OpenGL.framework" CACHE FILEPATH "" FORCE)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

include(FetchContent)

# libusb
# FetchContent_Declare(
#   libusb
#   GIT_REPOSITORY https://github.com/libusb/libusb.git
#   GIT_TAG v1.0.27
# )
# set(LIBUSB_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# set(LIBUSB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# set(LIBUSB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# FetchContent_MakeAvailable(libusb)

# Native macOS UVC implementation
if(APPLE)
  set(NATIVE_SOURCES
    src/native/UVCController.m
    src/native/UVCType.m
    src/native/UVCValue.m
    src/native/uvc_native.m
  )
  set_source_files_properties(${NATIVE_SOURCES} PROPERTIES COMPILE_FLAGS "-fno-objc-arc")
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE 
  src/plugin-main.cpp
  src/uvc-manager.cpp
  src/uvc-settings-dialog.cpp
  src/thirdparty/mongoose.c
  ${NATIVE_SOURCES}
)

if(APPLE)
  find_library(IOKIT_LIBRARY IOKit)
  find_library(COREAUDIO_LIBRARY CoreAudio)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  find_library(FOUNDATION_LIBRARY Foundation)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE 
    ${IOKIT_LIBRARY} 
    ${COREAUDIO_LIBRARY} 
    ${COREFOUNDATION_LIBRARY}
    ${FOUNDATION_LIBRARY}
  )
endif()

if(APPLE)
  set_source_files_properties(src/thirdparty/mongoose.c PROPERTIES COMPILE_FLAGS "-w")
elseif(MSVC)
  set_source_files_properties(src/thirdparty/mongoose.c PROPERTIES COMPILE_FLAGS "/W0")
endif()

set(WrapOpenGL_AGL "/System/Library/Frameworks/OpenGL.framework" CACHE FILEPATH "" FORCE)
set(OPENGL_agl_LIBRARY "/System/Library/Frameworks/OpenGL.framework" CACHE FILEPATH "" FORCE)

if(APPLE)
  get_target_property(LIBS ${CMAKE_PROJECT_NAME} LINK_LIBRARIES)
  if(LIBS)
    list(REMOVE_ITEM LIBS "-framework AGL")
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINK_LIBRARIES "${LIBS}")
  endif()
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
