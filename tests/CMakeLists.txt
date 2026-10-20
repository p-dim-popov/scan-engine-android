# scan-engine-android: host tests, run by CTest.
# Copyright (C) 2026 Petar Popov
# GPL-2.0-or-later with the SANE exception; see README.md and COPYING.

add_executable(engine_host_test engine_host_test.c)
target_compile_options(engine_host_test PRIVATE -Wall -Wextra -Werror)
target_link_libraries(engine_host_test PRIVATE engine Threads::Threads)

# One CTest test per host test; the names must match the table in
# engine_host_test.c.
foreach(name
    gray_page_is_one_header_its_rows_and_a_good_end
    colour_16_bit_rows_pass_through_unchanged
    three_pass_page_has_a_header_per_colour_frame
    page_of_unknown_height_still_ends_cleanly
    backend_error_ends_the_page_with_its_status
    app_closing_the_pipe_cancels_the_page
    large_page_streams_in_bounded_memory
    linked_backends_are_found_by_name)
  add_test(NAME ${name} COMMAND engine_host_test ${name})
endforeach()

# ---- licence guards ------------------------------------------------------------

# Configuring with a backend outside licensing/backend-allowlist.txt fails.
add_test(NAME allowlist_guard_rejects_a_backend_outside_the_list
  COMMAND ${CMAKE_COMMAND} -S ${PROJECT_SOURCE_DIR} -B ${CMAKE_CURRENT_BINARY_DIR}/allowlist-guard
          -G ${CMAKE_GENERATOR} -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM} "-DENGINE_BACKENDS=pixma$<SEMICOLON>genesys")
set_tests_properties(allowlist_guard_rejects_a_backend_outside_the_list PROPERTIES
  PASS_REGULAR_EXPRESSION "SANE backend 'genesys' is not in licensing/backend-allowlist.txt")

# check-sane-exception.sh fails on compiled backend/ and sanei/ files without
# the exception, and only on those.
add_test(NAME exception_guard_rejects_compiled_files_without_the_exception
  COMMAND ${CMAKE_COMMAND} -DGUARD=${PROJECT_SOURCE_DIR}/scripts/check-sane-exception.sh
          -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/exception-guard -DNINJA=${CMAKE_MAKE_PROGRAM}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/guards/SaneExceptionGuardTest.cmake)

# check-licence-notices.sh fails on an Engine source file outside third_party/
# without the exception.
add_test(NAME notice_guard_rejects_engine_sources_without_the_exception
  COMMAND ${CMAKE_COMMAND} -DGUARD=${PROJECT_SOURCE_DIR}/scripts/check-licence-notices.sh
          -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/notice-guard
          -P ${CMAKE_CURRENT_SOURCE_DIR}/guards/LicenceNoticeGuardTest.cmake)
