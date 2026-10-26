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
            // Release builds ship arm64-v8a only; debug builds add x86_64 below.
            abiFilters += "arm64-v8a"
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
        debug {
            ndk {
                // x86_64 runs the Engine with the test backend on the emulator.
                abiFilters += "x86_64"
            }
        }
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

// The USB Scanners the linked backends drive, generated as resources of this library for the
// consuming app: the table it routes a USB device by, and its manifest's USB device filter.
val generateUsbDevices = tasks.register<GenerateUsbDevices>("generateUsbDevices") {
    backends = layout.projectDirectory.file("../backends.txt")
    descriptions = layout.projectDirectory.dir("../third_party/sane-backends/doc/descriptions")
}

androidComponents {
    onVariants { variant ->
        variant.sources.res?.addGeneratedSourceDirectory(generateUsbDevices, GenerateUsbDevices::outputDirectory)
    }
}

/**
 * Writes the USB Scanners that the backends in backends.txt drive, read from their .desc files in
 * sane-backends, as two resources:
 *
 * - xml/sane_usb_devices: one `<device vendor-id product-id backend model>` per USB id, the table
 *   the app routes a USB device by;
 * - xml/sane_usb_device_filter: the same ids as `<usb-device>` entries, the USB device filter of the
 *   app's manifest, so attaching one of them offers the app.
 *
 * A model the backend lists as unsupported, or without a USB id, is left out. Models that share a
 * USB id share its row, their names joined. Two backends that list one USB id fail the build: the
 * app could not tell which one to open it with.
 */
@CacheableTask
abstract class GenerateUsbDevices : DefaultTask() {
    @get:InputFile
    @get:PathSensitive(PathSensitivity.NONE)
    abstract val backends: RegularFileProperty

    @get:InputDirectory
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val descriptions: DirectoryProperty

    @get:OutputDirectory
    abstract val outputDirectory: DirectoryProperty

    private class Device(val vendorId: Int, val productId: Int, val backend: String, val models: MutableList<String>) {
        /** The models' names, joined and escaped for an XML attribute or comment. */
        val model get() = escape(models.joinToString(" / "))
    }

    @TaskAction
    fun generate() {
        val devices = linkedMapOf<Pair<Int, Int>, Device>()
        val names = backends.get().asFile.readLines().map { it.trim() }.filter { it.isNotEmpty() && !it.startsWith("#") }
        for (backend in names) {
            val desc = descriptions.get().file("$backend.desc").asFile
            if (!desc.exists()) throw GradleException("backends.txt lists $backend, which has no $desc")
            for ((id, model) in usbModels(desc)) {
                val listed = devices.getOrPut(id) { Device(id.first, id.second, backend, mutableListOf()) }
                if (listed.backend != backend) {
                    throw GradleException(
                        "USB id %04x:%04x is listed by both %s and %s".format(id.first, id.second, listed.backend, backend),
                    )
                }
                if (model !in listed.models) listed.models += model
            }
        }

        val xml = outputDirectory.get().dir("xml").asFile
        xml.mkdirs()
        xml.resolve("sane_usb_devices.xml").writeText(
            buildString {
                appendLine("""<?xml version="1.0" encoding="utf-8"?>""")
                appendLine("<!-- Generated from the .desc files of the backends in backends.txt. -->")
                appendLine("<sane-usb-devices>")
                for (device in devices.values) {
                    appendLine(
                        """    <device vendor-id="0x%04x" product-id="0x%04x" backend="%s" model="%s" />"""
                            .format(device.vendorId, device.productId, device.backend, device.model),
                    )
                }
                appendLine("</sane-usb-devices>")
            },
        )
        xml.resolve("sane_usb_device_filter.xml").writeText(
            buildString {
                appendLine("""<?xml version="1.0" encoding="utf-8"?>""")
                appendLine("<!-- Generated from the .desc files of the backends in backends.txt. -->")
                appendLine("<resources>")
                for (device in devices.values) {
                    appendLine("    <!-- %s (%s), %04x:%04x -->".format(device.model, device.backend, device.vendorId, device.productId))
                    appendLine("""    <usb-device vendor-id="${device.vendorId}" product-id="${device.productId}" />""")
                }
                appendLine("</resources>")
            },
        )
    }

    /** The (USB id, model name) of each model in [desc] that has a USB id and is not unsupported. */
    private fun usbModels(desc: File): List<Pair<Pair<Int, Int>, String>> {
        val models = mutableListOf<Pair<Pair<Int, Int>, String>>()
        var manufacturer = ""
        var model: String? = null
        var status: String? = null
        var usbId: List<String> = emptyList()
        fun finish() {
            val name = model ?: return
            model = null
            if (status == ":unsupported" || usbId.size != 2 || !usbId.all { it.startsWith("0x") }) return
            val id = usbId[0].substring(2).toInt(16) to usbId[1].substring(2).toInt(16)
            models += id to if (name.startsWith(manufacturer)) name else "$manufacturer $name"
        }
        for (line in desc.readLines()) {
            val tokens = tokens(line)
            when (tokens.firstOrNull()) {
                ":mfg" -> {
                    finish()
                    manufacturer = tokens[1]
                }
                ":model" -> {
                    finish()
                    model = tokens[1]
                    status = null
                    usbId = emptyList()
                }
                ":status" -> status = tokens[1]
                ":usbid" -> usbId = tokens.drop(1).map { it.lowercase() }
            }
        }
        finish()
        return models
    }

    /** The words of a .desc line, a quoted string as one word without its quotes; `;` starts a comment. */
    private fun tokens(line: String): List<String> =
        Regex(""""((?:[^"\\]|\\.)*)"|(;.*)|(\S+)""").findAll(line)
            .takeWhile { it.groups[2] == null }
            .map { it.groups[1]?.value ?: it.value }
            .toList()

    private companion object {
        fun escape(text: String) =
            text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;")
    }
}
