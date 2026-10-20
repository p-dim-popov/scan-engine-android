/* scan-engine-android
   Copyright (C) 2026 Petar Popov

   The Engine's binder contract.  It carries SANE shapes only: status codes,
   option descriptors and values, and the file descriptor of a Page stream
   (src/engine.h).

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

package io.github.pdimpopov.scanengine;

import io.github.pdimpopov.scanengine.SaneOptionDescriptor;

/** A handle is one open device for one Scan Job.  Every call but cancel
    comes from one thread at a time; close comes after streamPage returns.
    Methods that return int return a SANE_Status.  */
interface IEngine {
    /** Opens DEVICE of the backend called BACKEND.  On SANE_STATUS_GOOD,
        handle[0] is the new handle.  */
    int open(String backend, String device, out int[] handle);

    /** The descriptors of every option, option 0 (the option count) first.  */
    SaneOptionDescriptor[] getOptionDescriptors(int handle);

    /** Sets a bool, int or fixed option, or an array of them.  VALUE holds
        size / 4 words and receives what the backend stored; info[0]
        receives the SANE_INFO flags.  */
    int setWordOption(int handle, int option, inout int[] value, out int[] info);

    /** Sets a string option; info[0] receives the SANE_INFO flags.  */
    int setStringOption(int handle, int option, String value, out int[] info);

    /** Scans one Page into PAGE as a Page stream and closes PAGE.  Blocks
        until the Page ends and returns the status written in END.  */
    int streamPage(int handle, in ParcelFileDescriptor page);

    /** Cancels the Scan Job: the Page in progress ends with
        SANE_STATUS_CANCELLED, and so does any later one.  */
    void cancel(int handle);

    void close(int handle);
}
