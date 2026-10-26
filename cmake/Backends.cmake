# scan-engine-android: the SANE backend set.
# Copyright (C) 2026 Petar Popov
# GPL-2.0-or-later with the SANE exception; see README.md and COPYING.
#
# ENGINE_BACKENDS lists the backends linked into the Engine; backends.txt
# holds its default. Each one is compiled with BACKEND_NAME=<name> and
# reached through its sane_<name>_* entry points; there is no dll loader.
# Configuring fails when a backend is missing from
# licensing/backend-allowlist.txt. ENGINE_WITH_TEST_BACKEND adds the test
# backend: on for the host and Android debug builds, off for Android release
# builds.

function(engine_read_list file out)
  file(STRINGS ${file} lines)
  set(entries "")
  foreach(line IN LISTS lines)
    string(STRIP "${line}" line)
    if(line AND NOT line MATCHES "^#")
      list(APPEND entries "${line}")
    endif()
  endforeach()
  set(${out} "${entries}" PARENT_SCOPE)
endfunction()

engine_read_list(${CMAKE_CURRENT_SOURCE_DIR}/backends.txt default_backends)
set(ENGINE_BACKENDS "${default_backends}" CACHE STRING "SANE backends linked into the Engine")
option(ENGINE_WITH_TEST_BACKEND "Link the SANE test backend (debug and host builds)" ON)
if(ENGINE_WITH_TEST_BACKEND AND NOT "test" IN_LIST ENGINE_BACKENDS)
  list(APPEND ENGINE_BACKENDS test)
endif()

# Sources per backend, relative to sane-backends/backend.
set(ENGINE_BACKEND_pixma_SOURCES
  pixma/pixma.c pixma/pixma_io_sanei.c pixma/pixma_common.c
  pixma/pixma_mp150.c pixma/pixma_mp730.c pixma/pixma_mp750.c
  pixma/pixma_mp800.c pixma/pixma_imageclass.c pixma/pixma_bjnp.c)
set(ENGINE_BACKEND_avision_SOURCES avision.c)
set(ENGINE_BACKEND_plustek_SOURCES plustek.c)
set(ENGINE_BACKEND_fujitsu_SOURCES fujitsu.c)
set(ENGINE_BACKEND_canon_dr_SOURCES canon_dr.c)
set(ENGINE_BACKEND_hp_SOURCES
  hp.c hp-accessor.c hp-device.c hp-handle.c hp-hpmem.c hp-option.c hp-scl.c)
set(ENGINE_BACKEND_hp3900_SOURCES hp3900.c)
set(ENGINE_BACKEND_hp5400_SOURCES hp5400.c)
set(ENGINE_BACKEND_hp5590_SOURCES hp5590.c)
set(ENGINE_BACKEND_hpljm1005_SOURCES hpljm1005.c)
set(ENGINE_BACKEND_lexmark_SOURCES lexmark.c lexmark_low.c)
set(ENGINE_BACKEND_rts8891_SOURCES rts8891.c rts88xx_lib.c)
set(ENGINE_BACKEND_umax1220u_SOURCES umax1220u.c)
set(ENGINE_BACKEND_test_SOURCES test.c)

# sanei modules a backend needs beyond those every backend shares
# (CMakeLists.txt), as in upstream's backend/Makefile.am. sanei_pio, the
# parallel port, is src/sanei_pio_none.c: a phone has no parallel port, and
# upstream's sanei_pio.c needs sanei_directio.c, which carries no licence.
set(ENGINE_BACKEND_avision_SANEI sanei_config2 sanei_scsi)
set(ENGINE_BACKEND_plustek_SANEI sanei_lm983x sanei_access)
set(ENGINE_BACKEND_fujitsu_SANEI sanei_config2 sanei_scsi sanei_magic)
set(ENGINE_BACKEND_canon_dr_SANEI sanei_config2 sanei_scsi sanei_magic)
set(ENGINE_BACKEND_hp_SANEI sanei_config2 sanei_scsi sanei_pio)
set(ENGINE_BACKEND_rts8891_SANEI sanei_scsi)
set(ENGINE_BACKEND_umax1220u_SANEI sanei_pv8630)

function(engine_check_backend_allowlist)
  engine_read_list(${CMAKE_CURRENT_SOURCE_DIR}/licensing/backend-allowlist.txt allowed)
  foreach(backend IN LISTS ENGINE_BACKENDS)
    if(NOT backend IN_LIST allowed)
      message(FATAL_ERROR
        "SANE backend '${backend}' is not in licensing/backend-allowlist.txt. "
        "Only backends whose every compiled file carries the SANE exception may enter the Engine.")
    endif()
  endforeach()
endfunction()

# Adds an object library per backend, and the sanei modules they need, to
# the target sane_engine.
function(engine_add_backends target sane_dir)
  set(sanei "")
  foreach(backend IN LISTS ENGINE_BACKENDS)
    list(APPEND sanei ${ENGINE_BACKEND_${backend}_SANEI})
    if(NOT DEFINED ENGINE_BACKEND_${backend}_SOURCES)
      message(FATAL_ERROR "SANE backend '${backend}' has no source list in cmake/Backends.cmake")
    endif()
    list(TRANSFORM ENGINE_BACKEND_${backend}_SOURCES PREPEND ${sane_dir}/backend/ OUTPUT_VARIABLE sources)
    add_library(backend_${backend} OBJECT ${sources})
    target_compile_definitions(backend_${backend} PRIVATE BACKEND_NAME=${backend})
    target_link_libraries(backend_${backend} PRIVATE sane_common)
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:backend_${backend}>)
  endforeach()
  list(REMOVE_DUPLICATES sanei)
  foreach(module IN LISTS sanei)
    if(module STREQUAL "sanei_pio")
      target_sources(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/sanei_pio_none.c)
    else()
      target_sources(${target} PRIVATE ${sane_dir}/sanei/${module}.c)
    endif()
  endforeach()
endfunction()

# Writes out_file with one ENGINE_CONF("<name>.conf", "<text>") line per
# linked backend that has an upstream <name>.conf.in, for src/engine_conf.c.
# The confs need no @...@ substitution; configuring fails if one does.
function(engine_write_conf_list sane_dir out_file)
  set(list "")
  foreach(backend IN LISTS ENGINE_BACKENDS)
    set(conf ${sane_dir}/backend/${backend}.conf.in)
    if(NOT EXISTS ${conf})
      continue()
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${conf})
    file(READ ${conf} text)
    if(text MATCHES "@[A-Z_]+@")
      message(FATAL_ERROR "${backend}.conf.in needs ${CMAKE_MATCH_0} filled in; src/engine_conf.c serves it unchanged")
    endif()
    string(REPLACE "\\" "\\\\" text "${text}")
    string(REPLACE "\"" "\\\"" text "${text}")
    string(REPLACE "\n" "\\n\"\n  \"" text "${text}")
    string(APPEND list "ENGINE_CONF(\"${backend}.conf\",\n  \"${text}\")\n")
  endforeach()
  # @ONLY leaves ${...} in a conf alone; @...@ was refused above.
  file(CONFIGURE OUTPUT ${out_file} CONTENT "${list}" @ONLY)
endfunction()
