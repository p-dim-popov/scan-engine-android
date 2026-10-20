# scan-engine-android: self-test of scripts/check-licence-notices.sh.
# Copyright (C) 2026 Petar Popov
# GPL-2.0-or-later with the SANE exception; see README.md and COPYING.
#
# Builds a throwaway git repository in which Engine source files lack the
# exception text, committed or not yet added, then checks that the guard
# fails on exactly those files and ignores third_party/, git-ignored build
# output and non-source files. Run with cmake -P; inputs: GUARD, WORK_DIR.

# Split so that this file itself does not count as carrying the exception.
set(phrase "As a special")
string(APPEND phrase " exception")

set(repo ${WORK_DIR}/repo)
file(REMOVE_RECURSE ${WORK_DIR})
file(WRITE ${repo}/src/covered.c "/* ${phrase}, ... */\n")
file(WRITE ${repo}/src/covered.kt "/* ${phrase}, ... */\n")
file(WRITE ${repo}/src/uncovered.h "/* no notice */\n")
file(WRITE ${repo}/android/src/Uncovered.aidl "// no notice\n")
file(WRITE ${repo}/third_party/lib/upstream.c "/* upstream's own licence */\n")
file(WRITE ${repo}/README.md "no notice needed\n")
file(WRITE ${repo}/src/untracked.c "/* new, not yet added to git */\n")
file(WRITE ${repo}/.gitignore "build/\n")
file(WRITE ${repo}/build/generated.c "/* build output */\n")

function(run)
  execute_process(COMMAND ${ARGN} WORKING_DIRECTORY ${repo} RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "fixture setup failed: ${ARGN}")
  endif()
endfunction()
run(git init -q)
run(git add src/covered.c src/covered.kt src/uncovered.h android third_party README.md .gitignore)

execute_process(COMMAND ${GUARD} ${repo} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(result EQUAL 0)
  message(FATAL_ERROR "the guard passed a source file without the exception:\n${out}")
endif()
foreach(expected src/uncovered.h android/src/Uncovered.aidl src/untracked.c)
  if(NOT out MATCHES "${expected}")
    message(FATAL_ERROR "the guard did not name ${expected}:\n${out}")
  endif()
endforeach()
foreach(unexpected covered.c covered.kt upstream.c README.md generated.c)
  if(out MATCHES "[/ ]${unexpected}")
    message(FATAL_ERROR "the guard named ${unexpected}, which it must not:\n${out}")
  endif()
endforeach()

file(WRITE ${repo}/src/uncovered.h "/* ${phrase}, ... */\n")
file(WRITE ${repo}/android/src/Uncovered.aidl "// ${phrase}, ...\n")
file(WRITE ${repo}/src/untracked.c "/* ${phrase}, ... */\n")
execute_process(COMMAND ${GUARD} ${repo} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "the guard failed although every source file carries the exception:\n${out}")
endif()
