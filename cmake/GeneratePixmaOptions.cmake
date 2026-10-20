# scan-engine-android: generates pixma_sane_options.[ch] from pixma.c, as the
# upstream Makefile.am does. Run with cmake -P.
# Copyright (C) 2026 Petar Popov
# GPL-2.0-or-later with the SANE exception; see README.md and COPYING.
#
# Inputs: PYTHON, SCRIPT, PIXMA_C, OUT_DIR.
file(MAKE_DIRECTORY ${OUT_DIR})
foreach(kind h c)
  if(kind STREQUAL "h")
    set(args h)
  else()
    set(args "")
  endif()
  execute_process(
    COMMAND ${PYTHON} ${SCRIPT} ${args}
    INPUT_FILE ${PIXMA_C}
    OUTPUT_FILE ${OUT_DIR}/pixma_sane_options.${kind}
    RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "pixma_gen_options.py failed for pixma_sane_options.${kind}")
  endif()
endforeach()
