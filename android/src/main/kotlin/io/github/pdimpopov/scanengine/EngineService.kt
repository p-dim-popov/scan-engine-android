/* scan-engine-android
   Copyright (C) 2026 Petar Popov

   The Engine's bound Service, in its own process.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.  */

package io.github.pdimpopov.scanengine

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.os.ParcelFileDescriptor
import android.os.Process

/**
 * The Engine: runs SANE backends for the app over [IEngine], in its own process (the manifest's
 * android:process). The process lives only while the app binds the Service, so a hung backend or a
 * session left open by an app process that died goes with it.
 */
class EngineService : Service() {
    /** Open sessions by the number the app holds. */
    private val sessions = HashMap<Int, Long>()
    private var nextSession = 1

    private val binder = object : IEngine.Stub() {
        override fun open(backend: String, device: String, session: IntArray): Int {
            val pointer = LongArray(1)
            val status = NativeEngine.open(backend, device, pointer)
            if (status == SANE_STATUS_GOOD) {
                session[0] = synchronized(sessions) { nextSession++.also { sessions[it] = pointer[0] } }
            }
            return status
        }

        override fun getOptionDescriptors(session: Int): Array<SaneOptionDescriptor> {
            val pointer = pointer(session)
            return Array(NativeEngine.optionCount(pointer)) { option ->
                SaneOptionDescriptor().also { NativeEngine.optionDescriptor(pointer, option, it) }
            }
        }

        override fun setWordOption(session: Int, option: Int, value: IntArray, info: IntArray): Int =
            NativeEngine.setWordOption(pointer(session), option, value, info)

        override fun setStringOption(session: Int, option: Int, value: String, info: IntArray): Int =
            NativeEngine.setStringOption(pointer(session), option, value, info)

        override fun streamPage(session: Int, page: ParcelFileDescriptor): Int =
            page.use { NativeEngine.streamPage(pointer(session), it.fd) }

        // Under the table's lock, so that a close cannot free the session in between.
        override fun cancel(session: Int) = synchronized(sessions) {
            sessions[session]?.let(NativeEngine::cancel) ?: Unit
        }

        override fun close(session: Int) = synchronized(sessions) {
            sessions.remove(session)?.let(NativeEngine::close) ?: Unit
        }
    }

    private fun pointer(session: Int): Long =
        synchronized(sessions) { sessions[session] } ?: throw IllegalArgumentException("No session $session")

    override fun onBind(intent: Intent): IBinder = binder

    override fun onDestroy() {
        super.onDestroy()
        Process.killProcess(Process.myPid())
    }

    private companion object {
        const val SANE_STATUS_GOOD = 0
    }
}
