// scan-engine-android: Android library module that builds libengine.so.
// Copyright (C) 2026 Petar Popov
// GPL-2.0-or-later with the SANE exception; see README.md and COPYING.
//
// The module is built inside the consuming app's Gradle build (the app adds
// this repository as a git submodule and includes this directory as a
// project), so the Android Gradle plugin version comes from that build.

plugins {
    id("com.android.library")
}

android {
    namespace = "io.github.pdimpopov.scanengine"
    compileSdk = 37
    ndkVersion = "27.2.12479018"

    defaultConfig {
        minSdk = 26
        ndk {
            // arm64-v8a ships; x86_64 runs the Engine with the test backend on the emulator.
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DANDROID_STL=none",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
                )
            }
        }
    }

    buildFeatures {
        // IEngine.aidl, the binder contract between the app and the Engine's Service.
        aidl = true
    }

    buildTypes {
        release {
            externalNativeBuild {
                cmake {
                    // The test backend is for the emulator and instrumented tests only.
                    arguments += "-DENGINE_WITH_TEST_BACKEND=OFF"
                }
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../CMakeLists.txt")
            version = "3.22.1"
        }
    }
}
