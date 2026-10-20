# scan-engine-android: Engine build for Android and the Linux host.
# Copyright (C) 2026 Petar Popov
# GPL-2.0-or-later with the SANE exception; see README.md and COPYING.
#
# Targets:
#   usb          libusb-1.0, static (LGPL-2.1)
#   jpeg         libjpeg-turbo, static (IJG/BSD), built as an external project
#   sane_engine  the one static library: sanei helpers plus every backend in
#                ENGINE_BACKENDS, linked through sane_<name>_* entry points
#   engine       Engine core (backend table, handles, Page stream); on
#                Android libengine.so with the JNI glue, on the host a static
#                library
#   engine_host_test  host only: drives the test backend through the Engine core
cmake_minimum_required(VERSION 3.22)
project(scan_engine C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TP ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
set(SANE ${TP}/sane-backends)
set(LIBUSB ${TP}/libusb)

include(cmake/Backends.cmake)
engine_check_backend_allowlist()

# ---- libusb ------------------------------------------------------------------
add_library(usb STATIC
  ${LIBUSB}/libusb/core.c ${LIBUSB}/libusb/descriptor.c ${LIBUSB}/libusb/hotplug.c
  ${LIBUSB}/libusb/io.c ${LIBUSB}/libusb/sync.c ${LIBUSB}/libusb/strerror.c
  ${LIBUSB}/libusb/os/linux_usbfs.c ${LIBUSB}/libusb/os/events_posix.c
  ${LIBUSB}/libusb/os/threads_posix.c ${LIBUSB}/libusb/os/linux_netlink.c)
# libusb's Android config.h only names Linux features, so the host build shares it.
target_include_directories(usb PRIVATE ${LIBUSB}/android ${LIBUSB}/libusb ${LIBUSB}/libusb/os)
target_include_directories(usb INTERFACE ${LIBUSB}/libusb)
target_compile_options(usb PRIVATE -fvisibility=hidden -pthread -Wno-unused-parameter)
find_package(Threads REQUIRED)
target_link_libraries(usb PUBLIC Threads::Threads)
if(ANDROID)
  target_link_libraries(usb PUBLIC log)
endif()

# ---- libjpeg-turbo -------------------------------------------------------------
include(ExternalProject)
set(JPEG_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo)
set(JPEG_LIB ${JPEG_PREFIX}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}jpeg${CMAKE_STATIC_LIBRARY_SUFFIX})
set(JPEG_ARGS
  -DCMAKE_INSTALL_PREFIX=${JPEG_PREFIX}
  -DCMAKE_INSTALL_LIBDIR=lib
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
  -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  -DENABLE_SHARED=OFF -DENABLE_STATIC=ON
  -DWITH_TURBOJPEG=OFF -DWITH_TOOLS=OFF -DWITH_TESTS=OFF)
if(ANDROID)
  list(APPEND JPEG_ARGS
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
    -DANDROID_ABI=${ANDROID_ABI}
    -DANDROID_PLATFORM=${ANDROID_PLATFORM})
endif()
ExternalProject_Add(libjpeg_turbo
  SOURCE_DIR ${TP}/libjpeg-turbo
  CMAKE_ARGS ${JPEG_ARGS}
  BUILD_BYPRODUCTS ${JPEG_LIB})
file(MAKE_DIRECTORY ${JPEG_PREFIX}/include)
add_library(jpeg STATIC IMPORTED)
set_target_properties(jpeg PROPERTIES
  IMPORTED_LOCATION ${JPEG_LIB}
  INTERFACE_INCLUDE_DIRECTORIES ${JPEG_PREFIX}/include)
add_dependencies(jpeg libjpeg_turbo)

# ---- sane_engine: sanei helpers + backends, no dll loader ---------------------
# Compile settings every sane-backends source shares.
add_library(sane_common INTERFACE)
target_include_directories(sane_common INTERFACE ${SANE}/include ${SANE}/backend)
target_compile_definitions(sane_common INTERFACE HAVE_CONFIG_H=1)
target_compile_options(sane_common INTERFACE
  -Wno-unused-parameter -Wno-deprecated-declarations -Wno-unused-but-set-variable)
target_link_libraries(sane_common INTERFACE usb jpeg)

add_library(sane_engine STATIC
  ${SANE}/sanei/sanei_usb.c ${SANE}/sanei/sanei_config.c ${SANE}/sanei/sanei_init_debug.c
  ${SANE}/sanei/sanei_constrain_value.c ${SANE}/sanei/sanei_thread.c ${SANE}/sanei/sanei_jpeg.c
  ${SANE}/backend/sane_strstatus.c)
target_include_directories(sane_engine PUBLIC ${SANE}/include)
# log (Android) and Threads arrive through usb.
target_link_libraries(sane_engine PRIVATE sane_common PUBLIC usb jpeg m)
engine_add_backends(sane_engine ${SANE})

if(TARGET backend_pixma)
  # pixma's option tables are generated from pixma.c, as in the upstream build.
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(PIXMA_GEN ${CMAKE_CURRENT_BINARY_DIR}/generated/pixma)
  set(PIXMA_SCRIPT ${SANE}/backend/pixma/scripts/pixma_gen_options.py)
  add_custom_command(
    OUTPUT ${PIXMA_GEN}/pixma_sane_options.h ${PIXMA_GEN}/pixma_sane_options.c
    COMMAND ${CMAKE_COMMAND} -DPYTHON=${Python3_EXECUTABLE} -DSCRIPT=${PIXMA_SCRIPT}
            -DPIXMA_C=${SANE}/backend/pixma/pixma.c -DOUT_DIR=${PIXMA_GEN}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GeneratePixmaOptions.cmake
    DEPENDS ${SANE}/backend/pixma/pixma.c ${PIXMA_SCRIPT} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GeneratePixmaOptions.cmake
    VERBATIM)
  target_sources(backend_pixma PRIVATE ${PIXMA_GEN}/pixma_sane_options.h ${PIXMA_GEN}/pixma_sane_options.c)
  set_source_files_properties(${PIXMA_GEN}/pixma_sane_options.c PROPERTIES HEADER_FILE_ONLY ON)
  target_include_directories(backend_pixma PRIVATE ${SANE}/backend/pixma ${PIXMA_GEN})
endif()

# ---- engine: backend table + Page stream -------------------------------------
set(ENGINE_BACKEND_LIST "")
foreach(backend IN LISTS ENGINE_BACKENDS)
  string(APPEND ENGINE_BACKEND_LIST "ENGINE_BACKEND(${backend})\n")
endforeach()
configure_file(src/engine_backend_list.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/engine_backend_list.h @ONLY)

if(ANDROID)
  # engine_jni.c is the JNI glue of the Engine's Service (android/).
  add_library(engine SHARED src/engine.c src/engine_jni.c)
else()
  add_library(engine STATIC src/engine.c)
endif()
target_include_directories(engine
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_options(engine PRIVATE -Wall -Wextra -Werror)
target_link_libraries(engine PUBLIC sane_engine)

# ---- host tests ----------------------------------------------------------------
if(NOT ANDROID)
  enable_testing()
  add_subdirectory(tests)
endif()
