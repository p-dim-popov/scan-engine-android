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

On the Linux host, it builds as a static library and a test executable:

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
scripts/check-sane-exception.sh build third_party/sane-backends licensing/sane-exception-exemptions.txt
```

The host build needs CMake 3.22 or later, Ninja, a C compiler and Python 3. Python 3 generates pixma's option tables from `pixma.c`, as the upstream build does.

## Tests

`tests/engine_host_test.c` scans with the SANE `test` backend through the Engine core into a pipe, then reads the Page stream back. It checks headers, byte counts and byte-exact passthrough (16-bit included). It also checks three-pass Pages, Pages of unknown height, backend errors, a reader that closes the pipe early, and memory that stays bounded for a 134 MB Page.

## Licence guards

CI fails when:

- `ENGINE_BACKENDS` names a backend that is not in `licensing/backend-allowlist.txt`. The check runs when CMake configures the build.
- A compiled file under `sane-backends/backend` or `sane-backends/sanei` does not contain the SANE exception text. This includes every header and `#include`d `.c` file, read from Ninja's dependency log. A file is exempt only when it is under a non-GPL permissive licence and is listed in `licensing/sane-exception-exemptions.txt` with the reason.

One exception-free GPL file in the Engine process would put the whole process under the plain GPL. Run both guards again after every upstream rebase: a maintainer can withdraw the exception.

## Changes to sane-backends 1.4.0

The first commit imports the three upstream trees unchanged, so `git log -p third_party/` shows every change. In short:

1. `include/sane/config.h` and `include/_stdint.h`: hand-written for Android (bionic) and glibc. Upstream generates them with configure.
2. `sanei/sanei_usb.c`, `include/sane/sanei_usb.h`: `sanei_usb_set_android_fd()`. libusb skips device discovery on Android; the app's USB file descriptor is wrapped with `libusb_wrap_sys_device` and registered as `libusb:android:<fd>`. Open and close reuse that handle.
3. `sanei/sanei_init_debug.c`: on Android, debug output goes to logcat.
4. `sanei/sanei_thread.c`: bionic has no `pthread_cancel`; Android uses the existing macOS path (`SIGUSR2` and `pthread_kill`).
5. Build: CMake instead of autotools. pixma's generated option tables are made at build time.

## Updating upstream

Replace a `third_party/` tree with the new release in one commit. Then reapply the changes above, build, and run the tests and both guards.
