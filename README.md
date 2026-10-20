# scan-engine-android

A port of [sane-backends](https://gitlab.com/sane-project/backends) to Android. It drives USB scanners from an app's own process over a USB file descriptor that Android's USB host API hands to the app, with no root and no `/dev/bus/usb` access.

The Engine runs in a separate process of the app that uses it. Everything that runs in that process is in this repository.

## Licence

GPL-2.0-or-later, with the SANE exception kept in every modified and new source file. See [COPYING](COPYING) and the notice at the top of each file. Vendored libraries keep their own licences:

| Directory | Version | Licence |
|---|---|---|
| `third_party/sane-backends` | 1.4.0, patched (see below) | GPL-2.0-or-later with the SANE exception |
| `third_party/libusb` | 1.0.30, unmodified | LGPL-2.1-or-later |
| `third_party/libjpeg-turbo` | 3.2.0, unmodified | IJG, BSD-3-Clause, zlib |

## Layout

- `CMakeLists.txt`: one build for Android and the Linux host.
- `cmake/Backends.cmake`: `ENGINE_BACKENDS`, the backends linked into the Engine, with each backend's source list.
- `src/`: Engine core: the table of linked backends and the Page stream (`engine.h` documents the wire format).
- `tests/`: host tests, run by CTest.
- `android/`: Android library module that builds `libengine.so`.
- `licensing/`: the backend allow-list and the exception-guard exemptions.
- `scripts/check-sane-exception.sh`: the exception guard.

## How it is built

The backends link statically into one library, `sane_engine`, through their `sane_<name>_*` entry points. There is no `dll` loader and there are no `.conf` files. The `ENGINE_BACKENDS` CMake list chooses the set (default `pixma`). `ENGINE_WITH_TEST_BACKEND` adds the SANE `test` backend; it is on for the host and Android debug builds and off for Android release builds.

On Android, `android/` is an Android library module. The consuming app adds this repository as a git submodule and includes `android/` as a Gradle project, so the Android Gradle plugin, NDK and CMake build it as part of the app build. The ABIs are arm64-v8a and x86_64.

On the Linux host, it builds as a static library and a test executable. `mise.toml` pins CMake and Ninja and defines a mise task for each step: `mise tasks` lists them, and `mise run <name>` runs one with the pinned tools on the PATH. A mise task runs the ones it needs first, so `mise run test` also configures and builds. `mise run ci` runs the same steps as CI, in the same order.

The host build also needs a C compiler and Python 3. Python 3 generates pixma's option tables from `pixma.c`, as the upstream build does.

## Tests

`tests/engine_host_test.c` scans with the SANE `test` backend through the Engine core into a pipe, then reads the Page stream back. It checks headers, byte counts and byte-exact passthrough (16-bit included). It also checks three-pass Pages, Pages of unknown height, backend errors (300 of them, each in a new process, for a hang on cancel), a reader that closes the pipe early, and memory that stays bounded for a 134 MB Page. Every test has a 60 s CTest timeout, so a hang fails the run.

## Licence guards

CI fails when:

- `ENGINE_BACKENDS` names a backend that is not in `licensing/backend-allowlist.txt`. The check runs when CMake configures the build.
- An Engine source file (`.c`, `.h`, `.in`, `.kt`, `.java` or `.aidl` outside `third_party/`) does not contain the SANE exception text: `scripts/check-licence-notices.sh`.
- A compiled file under `sane-backends/backend` or `sane-backends/sanei` does not contain the SANE exception text: `scripts/check-sane-exception.sh`. This includes every header and `#include`d `.c` file, read from Ninja's dependency log. A file is exempt only when it is under a non-GPL permissive licence and is listed in `licensing/sane-exception-exemptions.txt` with the reason.

One exception-free GPL file in the Engine process would put the whole process under the plain GPL. Run the guards again after every upstream rebase: a maintainer can withdraw the exception.

## Changes to sane-backends 1.4.0

The first commit imports the three upstream trees unchanged, so `git log -p third_party/` shows every change. In short:

1. `include/sane/config.h` and `include/_stdint.h`: hand-written for Android (bionic) and glibc. Upstream generates them with configure.
2. `sanei/sanei_usb.c`, `include/sane/sanei_usb.h`: `sanei_usb_set_android_fd()`. libusb skips device discovery on Android; the app's USB file descriptor is wrapped with `libusb_wrap_sys_device` and registered as `libusb:android:<fd>`. Open and close reuse that handle.
3. `sanei/sanei_init_debug.c`: on Android, debug output goes to logcat.
4. `sanei/sanei_thread.c`: bionic has no `pthread_cancel`; Android uses the existing macOS path (`SIGUSR2` and `pthread_kill`).
5. `backend/test.c`: in thread mode, `finish_pass` no longer cancels the reader thread. It closes the pipe, so the reader's next write fails and the thread ends, and then joins it. The reader blocks SIGPIPE and frees its buffer on that path. `sanei_thread_kill` cancels a thread asynchronously on Linux (`SIGUSR2` and `pthread_exit` on Android), and a cancel in the reader's first `malloc` left the join waiting for ever in about 1 newly started process in 10. pixma already stops its reader this way.
6. Build: CMake instead of autotools. pixma's generated option tables are made at build time.

## Updating upstream

1. `scripts/import-upstream.sh <name> <unpacked release>` replaces `third_party/<name>` and stages it. A plain `git add` drops the files that upstream's own `.gitignore` files match; the script adds them all and fails if any is missing.
2. Commit the import alone.
3. Reapply the changes above, then build and run the tests and the guards: `mise run ci`.

## Working on the Engine

The Engine is consumed by a proprietary Android app as the git submodule `engine/`, built in the app's Gradle build. A change reaches the app in this order:

1. Change, build and test it here, on a branch of this repository, and get it onto `main` here.
2. Move the app's submodule pointer to that commit, in the app repository.

Every new source file starts with the notice in `src/engine.h`: GPL-2.0-or-later with the SANE exception and a copyright line. Build scripts carry the one-line notice used in `CMakeLists.txt`. This repository is public: its commits, comments and docs name no product or company.
