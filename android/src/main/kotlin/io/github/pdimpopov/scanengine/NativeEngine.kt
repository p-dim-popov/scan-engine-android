/* scan-engine-android
   Copyright (C) 2026 Petar Popov

   The Engine core (src/engine.h) through JNI.

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

/** The Engine core through JNI. A handle is an engine_handle pointer; status results are SANE_Status. */
internal object NativeEngine {
    init {
        System.loadLibrary("engine")
    }

    /** engine_open; on SANE_STATUS_GOOD, handle[0] is the new handle. */
    @JvmStatic external fun open(backend: String, device: String, handle: LongArray): Int

    /** The number of options, option 0 included. */
    @JvmStatic external fun optionCount(handle: Long): Int

    /** Copies the descriptor of [option] into [descriptor]; false when the backend has none. */
    @JvmStatic external fun optionDescriptor(handle: Long, option: Int, descriptor: SaneOptionDescriptor): Boolean

    @JvmStatic external fun setWordOption(handle: Long, option: Int, value: IntArray, info: IntArray): Int

    @JvmStatic external fun setStringOption(handle: Long, option: Int, value: String, info: IntArray): Int

    /** engine_stream_page into [fd], which stays open. */
    @JvmStatic external fun streamPage(handle: Long, fd: Int): Int

    @JvmStatic external fun cancel(handle: Long)

    @JvmStatic external fun close(handle: Long)
}
