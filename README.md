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
- `backends.txt`: the backends linked into the Engine, the default of `ENGINE_BACKENDS`.
- `cmake/Backends.cmake`: `ENGINE_BACKENDS`, with each backend's source list and the sanei modules it needs.
- `src/`: Engine core: the table of linked backends, handles and the Page stream (`engine.h` documents them and the wire format); `engine_jni.c`, the JNI glue of the Android Service; `engine_conf.c`, the linked backends' confs, built in; `sanei_pio_none.c`, sanei_pio without a parallel port.
- `tests/`: host tests, run by CTest.
- `android/`: Android library module: `libengine.so`, the binder contract `IEngine.aidl`, `EngineService`, the bound Service that runs the Engine in its own process, and the USB devices the backends drive, as resources for the app.
- `licensing/`: the backend allow-list and the exception-guard exemptions.
- `scripts/check-sane-exception.sh`: the exception guard.

## How it is built

The backends link statically into one library, `sane_engine`, through their `sane_<name>_*` entry points. There is no `dll` loader. The `ENGINE_BACKENDS` CMake list chooses the set; its default, in `backends.txt`, is pixma, avision, plustek, fujitsu, canon_dr, hp, hp3900, hp5400, hp5590, hpljm1005, lexmark, rts8891 and umax1220u: the USB backends whose every compiled file carries the SANE exception and that need no firmware file. `ENGINE_WITH_TEST_BACKEND` adds the SANE `test` backend; it is on for the host and Android debug builds and off for Android release builds.

There are no `.conf` files on disk either. Most backends find a USB device only through the `usb <vendor> <product>` lines of their conf, so each linked backend's upstream `<name>.conf.in` is compiled in (`src/engine_conf.c`, from a list CMake writes when it configures), and `sanei_config_open` reads it from memory when no conf file is found.

On Android, `android/` is an Android library module. The consuming app adds this repository as a git submodule and includes `android/` as a Gradle project, so the Android Gradle plugin, NDK and CMake build it as part of the app build. Release builds are arm64-v8a only; debug builds add x86_64, for the emulator.

The module also generates two resources for the app from the `.desc` files of the backends in `backends.txt` (the Gradle task `generateUsbDevices`): `xml/sane_usb_devices`, one `<device vendor-id product-id backend model>` per USB id, by which the app chooses the backend to open a USB device with, and `xml/sane_usb_device_filter`, the same ids as a USB device filter for the app's manifest. A model its backend lists as unsupported is left out; two backends that list one USB id fail the build.

The module's manifest declares `EngineService` in the process `:engine`, not exported. The app binds it and calls it through `IEngine.aidl`, which carries SANE shapes only: status codes, option descriptors and values, and file descriptors: the USB device's and each Page stream's. A handle is one open device for one Scan Job. `openUsb` opens the device behind a USB file descriptor that the app opened for that Scan Job; the Engine closes the descriptor when the handle closes, so it holds no USB device between Scan Jobs, and one USB handle is open at a time; `streamPage` blocks until the Page ends, and `cancel`, a separate call from another thread, calls `sane_cancel` once the read in progress returns. The Engine's process ends when the app unbinds the Service, so a hung backend or a handle left open by an app that died goes with it.

On the Linux host, it builds as a static library and a test executable. `mise.toml` pins CMake, Ninja, shellcheck and actionlint and defines a mise task for each step: `mise tasks` lists them, and `mise run <name>` runs one with the pinned tools on the PATH. A mise task runs the ones it needs first, so `mise run test` also configures and builds. `mise run lint` runs shellcheck on `scripts/` and actionlint on the CI workflow; any finding fails it. `mise run ci` runs the same steps as CI, in the same order.

The host build also needs a C compiler, Python 3 and a JDK. Python 3 generates pixma's option tables from `pixma.c`, as the upstream build does. The JDK provides `jni.h`: the host build compiles the JNI glue (`src/engine_jni.c`) without linking it, so an error there fails the host build and CI, not only the consuming app's build.

## Debug output

In a debug build of the Android library, SANE debug output goes to logcat, one tag per backend or sanei module. The process gets no environment from adb, so `SANE_DEBUG_<NAME>` comes from the system property `debug.sane.<name>`, which `JNI_OnLoad` reads for each linked backend and for `sanei_usb`, `sanei_config` and `sanei_thread`. A property needs no root and holds until the device restarts; the next Engine process reads it:

```
adb shell setprop debug.sane.pixma 4
adb shell setprop debug.sane.sanei_usb 5
```

`sanei_usb` above 4 also turns on libusb's own log, tag `libusb`. The Engine core logs each refusal it returns itself (a second USB handle, an unknown backend, a backend that lists no device) under the tag `engine`, at every level.

Release builds log none of it: the build type defines `NDEBUG`, which removes the property read and pixma's `PDBG` and makes `DBG` print nothing.

## Tests

`tests/engine_host_test.c` scans with the SANE `test` backend through an Engine handle into a pipe, then reads the Page stream back. It checks headers, byte counts and byte-exact passthrough (16-bit included). It also checks three-pass Pages, Pages of unknown height, backend errors (300 of them, each in a new process, for a hang on cancel), a reader that closes the pipe early, a cancel from another thread while a Page streams, and memory that stays bounded for a 134 MB Page. A pipe stands in for a USB device's file descriptor to check that the Engine closes it with its handle or on a failed open, and that a second USB handle is busy while one is open. SIGPIPE stays ignored after a Page ends, fails or is cancelled, so a reader that closes the pipe after a backend has joined its reader thread is still a write error. Every backend in `backends.txt` is linked and initialises with no device attached, and `sanei_config_open` serves a linked backend's built-in conf and none for a backend that is not linked. Every test has a 60 s CTest timeout, so a hang fails the run.

## Licence guards

CI fails when:

- `ENGINE_BACKENDS` names a backend that is not in `licensing/backend-allowlist.txt`. The check runs when CMake configures the build; a host test configures with each backend the allow-list names as excluded and expects it to fail.
- An Engine source file (`.c`, `.h`, `.in`, `.kt`, `.java` or `.aidl` outside `third_party/`), committed or not yet added to git, does not contain the SANE exception text: `scripts/check-licence-notices.sh`. Git-ignored build output is skipped.
- A compiled file under `sane-backends/backend` or `sane-backends/sanei` does not contain the SANE exception text: `scripts/check-sane-exception.sh`. This includes every header and `#include`d `.c` file, read from Ninja's dependency log. A file is exempt only when it is listed in `licensing/sane-exception-exemptions.txt` with the reason, and either it is under a non-GPL permissive licence, or its header points to the opening comments of another file (fujitsu's and canon_dr's headers do): then the entry names that file (`by-reference backend/fujitsu.c`), which must carry the exception and be named in the header.

One exception-free GPL file in the Engine process would put the whole process under the plain GPL. Run the guards again after every upstream rebase: a maintainer can withdraw the exception.

## Changes to sane-backends 1.4.0

The first commit imports the three upstream trees unchanged, so `git log -p third_party/` shows every change. In short:

1. `include/sane/config.h`, `include/_stdint.h` and `include/byteorder.h`: hand-written for Android (bionic) and glibc. Upstream generates them with configure.
2. `sanei/sanei_usb.c`, `include/sane/sanei_usb.h`: `sanei_usb_set_android_fd()`. libusb skips device discovery on Android; the next device scan wraps the app's USB file descriptor with `libusb_wrap_sys_device` and registers it as `libusb:android:<fd>`. Open and close reuse that handle. A new descriptor, or -1, forgets the device behind the previous one and closes its handle, so each Scan Job's descriptor is wrapped afresh; the descriptor itself is the caller's to close.
3. `sanei/sanei_init_debug.c`: on Android, debug output goes to logcat.
4. `sanei/sanei_thread.c`: bionic has no `pthread_cancel`; Android uses the existing macOS path (`SIGUSR2` and `pthread_kill`).
5. `backend/test.c`: in thread mode, `finish_pass` no longer cancels the reader thread. It closes the pipe, so the reader's next write fails and the thread ends, and then joins it. The reader blocks SIGPIPE and frees its buffer on that path. `sanei_thread_kill` cancels a thread asynchronously on Linux (`SIGUSR2` and `pthread_exit` on Android), and a cancel in the reader's first `malloc` left the join waiting for ever in about 1 newly started process in 10. pixma already stops its reader this way.
6. `sanei/sanei_thread.c`: in thread mode, `sanei_thread_waitpid` sets SIGPIPE back to `SIG_DFL` only when `sanei_thread_begin` changed it from `SIG_DFL` to `SIG_IGN`. Upstream reset any ignored SIGPIPE after every join, for the whole process, so the Engine lost its own `SIG_IGN` after each Page. An app that closed the pipe before the Page's last rows or its END record then killed the Engine instead of ending the Page with a write error.
7. Build: CMake instead of autotools. pixma's generated option tables are made at build time.
8. `sanei/sanei_config.c`, `include/sane/sanei_config.h`: when `sanei_config_open` finds no conf file, it returns `sanei_config_open_builtin`'s, which the program provides (`src/engine_conf.c`).
9. `backend/avision.c`: on Android, the reader thread skips `pthread_setcancelstate` and `pthread_setcanceltype`; bionic has no thread cancellation.
10. `backend/umax1220u-common.c`: `memcmp` instead of `bcmp`, which bionic does not declare.
11. `backend/avision.c`, `backend/hp-handle.c`, `backend/hp-scl.c`, `backend/hp.h`: in thread mode, avision and hp no longer cancel their reader thread with `sanei_thread_kill` (item 5 says why); they stop it as pixma does. The reader checks a stop flag before each USB read, the cancel path closes the read end of the pipe so that a write in progress fails, and then joins the thread. avision's flag is `s->cancelled`, which `do_cancel` already set; `do_cancel` now resets the duplex state that the reader reads only after the join. hp's flag is `HpProcessData.stop`. `hp_handle_stopScan` sets it and closes the pipe before the join, not after it, and resets the scanner when the thread returns a status other than `SANE_STATUS_GOOD`, as it does for a killed child. On cancel, hp sets the flag where it killed an OfficeJet K reader. A reader blocked in a USB transfer stops when the transfer times out: hp's after at most two libusb timeouts of 30 s; avision's when `avision_cmd` gives up, after at most three attempts of a 30 s write, a 30 s read and two 10 s status reads, about 4 minutes. Forked mode does not change.

## Updating upstream

1. `scripts/import-upstream.sh <name> <unpacked release>` replaces `third_party/<name>` and stages it. A plain `git add` drops the files that upstream's own `.gitignore` files match; the script adds them all and fails if any is missing.
2. Commit the import alone.
3. Reapply the changes above, then build and run the tests and the guards: `mise run ci`.

## Working on the Engine

The Engine is consumed by a proprietary Android app as the git submodule `engine/`, built in the app's Gradle build. A change reaches the app in this order:

1. Change, build and test it here, on a branch of this repository, and get it onto `main` here.
2. Move the app's submodule pointer to that commit, in the app repository. This repository squash-merges pull requests, so the commit is the squashed one on `main`, not the branch commit.

Every new source file starts with the notice in `src/engine.h`: GPL-2.0-or-later with the SANE exception and a copyright line. Build scripts carry the one-line notice used in `CMakeLists.txt`. This repository is public: its commits, comments and docs name no product or company.
