# scan-engine-android: the SANE backend set.
# Copyright (C) 2026 Petar Popov
# GPL-2.0-or-later with the SANE exception; see README.md and COPYING.
#
# ENGINE_BACKENDS lists the backends linked into the Engine. Each one is
# compiled with BACKEND_NAME=<name> and reached through its
# sane_<name>_* entry points; there is no dll loader. Configuring fails
# when a backend is missing from licensing/backend-allowlist.txt.
# ENGINE_WITH_TEST_BACKEND adds the test backend: on for the host and
# Android debug builds, off for Android release builds.

set(ENGINE_BACKENDS "pixma" CACHE STRING "SANE backends linked into the Engine")
option(ENGINE_WITH_TEST_BACKEND "Link the SANE test backend (debug and host builds)" ON)
if(ENGINE_WITH_TEST_BACKEND AND NOT "test" IN_LIST ENGINE_BACKENDS)
  list(APPEND ENGINE_BACKENDS test)
endif()

# Sources per backend, relative to sane-backends/backend.
set(ENGINE_BACKEND_pixma_SOURCES
  pixma/pixma.c pixma/pixma_io_sanei.c pixma/pixma_common.c
  pixma/pixma_mp150.c pixma/pixma_mp730.c pixma/pixma_mp750.c
  pixma/pixma_mp800.c pixma/pixma_imageclass.c pixma/pixma_bjnp.c)
set(ENGINE_BACKEND_test_SOURCES test.c)

function(engine_check_backend_allowlist)
  file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/licensing/backend-allowlist.txt lines)
  set(allowed "")
  foreach(line IN LISTS lines)
    string(STRIP "${line}" line)
    if(line AND NOT line MATCHES "^#")
      list(APPEND allowed "${line}")
    endif()
  endforeach()
  foreach(backend IN LISTS ENGINE_BACKENDS)
    if(NOT backend IN_LIST allowed)
      message(FATAL_ERROR
        "SANE backend '${backend}' is not in licensing/backend-allowlist.txt. "
        "Only backends whose every compiled file carries the SANE exception may enter the Engine.")
    endif()
  endforeach()
endfunction()

# Adds an object library per backend to the target sane_engine.
function(engine_add_backends target sane_dir)
  foreach(backend IN LISTS ENGINE_BACKENDS)
    if(NOT DEFINED ENGINE_BACKEND_${backend}_SOURCES)
      message(FATAL_ERROR "SANE backend '${backend}' has no source list in cmake/Backends.cmake")
    endif()
    list(TRANSFORM ENGINE_BACKEND_${backend}_SOURCES PREPEND ${sane_dir}/backend/ OUTPUT_VARIABLE sources)
    add_library(backend_${backend} OBJECT ${sources})
    target_compile_definitions(backend_${backend} PRIVATE BACKEND_NAME=${backend})
    target_link_libraries(backend_${backend} PRIVATE sane_common)
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:backend_${backend}>)
  endforeach()
endfunction()
