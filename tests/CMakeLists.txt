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
    backend_error_right_after_start_never_hangs
    app_closing_the_pipe_cancels_the_page
    cancel_from_another_thread_ends_the_page_promptly
    sigpipe_stays_ignored_after_every_kind_of_page_end
    sigpipe_stays_ignored_after_a_pixma_style_reader_is_joined
    app_closing_the_pipe_on_a_later_page_is_a_write_error
    large_page_streams_in_bounded_memory
    linked_backends_are_found_by_name
    release_backends_initialise_and_list_no_device
    linked_backends_read_their_upstream_conf
    usb_fd_stays_open_until_its_handle_closes
    usb_fd_is_closed_when_no_device_opens
    usb_open_without_an_fd_is_refused_and_leaves_usb_free
    second_usb_handle_is_busy_until_the_first_closes)
  add_test(NAME ${name} COMMAND engine_host_test ${name})
endforeach()

# ---- licence guards ------------------------------------------------------------

# Configuring with a backend outside licensing/backend-allowlist.txt fails:
# one test for each backend the allow-list header names as excluded.
foreach(excluded
    genesys epson2 epsonds xerox_mfp canon_lide70 kodakaio kvs1025 kvs20xx
    kvs40xx magicolor hp4200 stv680 coolscan3 epson canon630u gt68xx snapscan
    epjitsu pieusb coolscan2)
  set(name allowlist_guard_rejects_${excluded})
  add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND} -S ${PROJECT_SOURCE_DIR} -B ${CMAKE_CURRENT_BINARY_DIR}/allowlist-guard/${excluded}
            -G ${CMAKE_GENERATOR} -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM} "-DENGINE_BACKENDS=pixma$<SEMICOLON>${excluded}")
  set_tests_properties(${name} PROPERTIES
    PASS_REGULAR_EXPRESSION "SANE backend '${excluded}' is not in licensing/backend-allowlist.txt")
endforeach()

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

# A hang fails its test instead of holding the run: every test above runs in
# about 2 s or less.
get_property(host_tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${host_tests} PROPERTIES TIMEOUT 60)
