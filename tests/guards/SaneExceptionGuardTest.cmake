# scan-engine-android: self-test of scripts/check-sane-exception.sh.
# Copyright (C) 2026 Petar Popov
# GPL-2.0-or-later with the SANE exception; see README.md and COPYING.
#
# Builds a throwaway sane-backends-shaped tree in which one compiled
# backend file and one header it includes lack the exception text, then
# checks that the guard fails on exactly those files, passes once they are
# exempted, and refuses exemptions for GPL text or for files no longer
# compiled. Run with cmake -P; inputs: GUARD, WORK_DIR, NINJA.

# Split so that this file itself does not count as carrying the exception.
set(phrase "As a special")
string(APPEND phrase " exception")

set(tree ${WORK_DIR}/tree)
set(build ${WORK_DIR}/build)
file(REMOVE_RECURSE ${WORK_DIR})
file(WRITE ${tree}/backend/covered.c "/* ${phrase}, ... */\n#include \"uncovered.h\"\nint covered(void) { return UNCOVERED; }\n")
file(WRITE ${tree}/backend/uncovered.h "/* IJG-style permissive licence */\n#define UNCOVERED 1\n")
file(WRITE ${tree}/backend/uncovered.c "/* IJG-style permissive licence */\nint uncovered(void) { return 2; }\n")
file(WRITE ${tree}/backend/gpl_only.c "/* GNU General Public License, no exception */\nint gpl_only(void) { return 4; }\n")
file(WRITE ${tree}/sanei/helper.c "/* ${phrase}, ... */\nint helper(void) { return 3; }\n")
file(WRITE ${tree}/backend/unused.c "/* not compiled, so not checked */\n")
file(WRITE ${tree}/CMakeLists.txt "cmake_minimum_required(VERSION 3.22)\nproject(fixture C)\nadd_library(fixture STATIC backend/covered.c backend/uncovered.c backend/gpl_only.c sanei/helper.c)\n")

execute_process(COMMAND ${CMAKE_COMMAND} -S ${tree} -B ${build} -G Ninja -DCMAKE_MAKE_PROGRAM=${NINJA}
                RESULT_VARIABLE result OUTPUT_QUIET)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "cannot configure the fixture tree")
endif()

file(WRITE ${WORK_DIR}/no-exemptions.txt "# none\n")
execute_process(COMMAND ${GUARD} ${build} ${tree} ${WORK_DIR}/no-exemptions.txt
                RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(result EQUAL 0)
  message(FATAL_ERROR "the guard passed a build with uncovered files:\n${out}")
endif()
foreach(expected backend/uncovered.c backend/uncovered.h backend/gpl_only.c)
  if(NOT out MATCHES "${expected}")
    message(FATAL_ERROR "the guard did not name ${expected}:\n${out}")
  endif()
endforeach()
foreach(unexpected backend/covered.c sanei/helper.c backend/unused.c)
  if(out MATCHES "${unexpected}")
    message(FATAL_ERROR "the guard named ${unexpected}, which it must not:\n${out}")
  endif()
endforeach()

function(run_guard exemptions expect_pass why)
  file(WRITE ${WORK_DIR}/exemptions.txt "# path, then the reason\n${exemptions}")
  execute_process(COMMAND ${GUARD} ${build} ${tree} ${WORK_DIR}/exemptions.txt
                  RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE out)
  if(expect_pass AND NOT result EQUAL 0)
    message(FATAL_ERROR "the guard failed ${why}:\n${out}")
  elseif(NOT expect_pass AND result EQUAL 0)
    message(FATAL_ERROR "the guard passed ${why}:\n${out}")
  endif()
endfunction()

set(permissive "backend/uncovered.c  permissive\nbackend/uncovered.h  permissive\n")
run_guard("${permissive}" FALSE "while a compiled GPL file lacks the exception")
run_guard("${permissive}backend/gpl_only.c  wrongly exempted\n" FALSE
          "with an exemption for a file under the GNU GPL")
file(REMOVE ${tree}/backend/gpl_only.c)
file(WRITE ${tree}/CMakeLists.txt "cmake_minimum_required(VERSION 3.22)\nproject(fixture C)\nadd_library(fixture STATIC backend/covered.c backend/uncovered.c sanei/helper.c)\n")
run_guard("${permissive}" TRUE "although every uncovered file is exempted")
run_guard("${permissive}backend/unused.c  no longer compiled\n" FALSE
          "with an exemption for a file that is not compiled")
