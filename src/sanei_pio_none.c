/* scan-engine-android
   Copyright (C) 2026 Petar Popov

   sanei_pio without a parallel port: a phone has none.  hp opens a
   parallel-port Scanner through sanei_pio; here every open fails, so it
   reaches its USB Scanners only.

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

#include <errno.h>

#include "sane/sane.h"
#include "sane/sanei_pio.h"

SANE_Status
sanei_pio_open (const char *dev, int *fd)
{
  (void) dev;
  *fd = -1;
  return SANE_STATUS_INVAL;
}

void
sanei_pio_close (int fd)
{
  (void) fd;
}

int
sanei_pio_read (int fd, u_char *buf, int n)
{
  (void) fd;
  (void) buf;
  (void) n;
  errno = EBADF;
  return -1;
}

int
sanei_pio_write (int fd, const u_char *buf, int n)
{
  (void) fd;
  (void) buf;
  (void) n;
  errno = EBADF;
  return -1;
}
