cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{DOLCESDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{DOLCESDK}/share/dolce.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define DOLCESDK to point to your SDK path!")
  endif()
endif()

project(bootsound)
include("${DOLCESDK}/share/dolce.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -nostdlib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

add_executable(bootsound
  src/main.c
)

target_link_libraries(bootsound
  gcc
  c
  taihen_stub 
  SceLibKernel_stub
  SceSysmem_stub
  SceKernelThreadMgr_stub
  SceIofilemgr_stub
  SceAudio_stub
)

dolce_create_self(bootsound.suprx bootsound
  CONFIG exports.yml
  UNSELF
)
