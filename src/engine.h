/* scan-engine-android
   Copyright (C) 2026 Petar Popov

   Engine core: the SANE backends linked into the Engine and the Page
   stream that carries scanned rows to the app through a file descriptor.

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


#ifndef ENGINE_H
#define ENGINE_H

#include "sane/sane.h"

/* One SANE backend linked into the Engine, reached through its
   sane_<name>_* entry points (there is no dll loader).  */
typedef struct engine_backend
{
  const char *name;
  SANE_Status (*init) (SANE_Int *version_code, SANE_Auth_Callback authorize);
  void (*exit) (void);
  SANE_Status (*get_devices) (const SANE_Device ***device_list,
                              SANE_Bool local_only);
  SANE_Status (*open) (SANE_String_Const name, SANE_Handle *handle);
  void (*close) (SANE_Handle handle);
  const SANE_Option_Descriptor *(*get_option_descriptor) (SANE_Handle handle,
                                                          SANE_Int option);
  SANE_Status (*control_option) (SANE_Handle handle, SANE_Int option,
                                 SANE_Action action, void *value,
                                 SANE_Int *info);
  SANE_Status (*get_parameters) (SANE_Handle handle, SANE_Parameters *params);
  SANE_Status (*start) (SANE_Handle handle);
  SANE_Status (*read) (SANE_Handle handle, SANE_Byte *data,
                       SANE_Int max_length, SANE_Int *length);
  void (*cancel) (SANE_Handle handle);
} engine_backend;

/* Returns the backend called NAME, or NULL when the build does not link it.  */
const engine_backend *engine_find_backend (const char *name);

/* The Page stream.

   engine_stream_page scans one Page from an open handle and writes it to FD
   as a sequence of records.  Every record starts with a four-byte ASCII tag;
   integers are 32 bits, little-endian.

     HEAD  format, last_frame, bytes_per_line, pixels_per_line, lines, depth
           (six int32, the SANE_Parameters of the frame that follows;
           lines is -1 when the backend does not know the Page height)
     DATA  uint32 length (never 0), then that many bytes of raw rows
     END   int32 SANE_Status: SANE_STATUS_GOOD when the Page is complete

   A single-pass Page is HEAD DATA* END.  A three-pass Page repeats HEAD DATA*
   for the red, green and blue frames before END.  Rows pass through
   unchanged, 16-bit samples included.  The Engine holds one read buffer, never
   the Page.

   Returns the status written in END.  On any status other than GOOD the
   backend's scan is cancelled; when FD can no longer be written the END
   record is not written and the result is SANE_STATUS_IO_ERROR.  After a
   GOOD Page the scan stays open, so a feeder can deliver the next Page with
   another call; the caller cancels when the Scan Job ends.

   The caller ignores SIGPIPE, so that a reader closing the pipe surfaces as
   SANE_STATUS_IO_ERROR instead of killing the process.  One Page streams at
   a time: the read buffer is shared.  */
#define ENGINE_RECORD_HEAD "HEAD"
#define ENGINE_RECORD_DATA "DATA"
#define ENGINE_RECORD_END "END "

SANE_Status engine_stream_page (const engine_backend *backend,
                                SANE_Handle handle, int fd);

#endif /* ENGINE_H */
