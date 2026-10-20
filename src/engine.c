/* scan-engine-android
   Copyright (C) 2026 Petar Popov

   Engine core: backend table, handles and Page stream.

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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "engine.h"
#include "sane/sanei_usb.h"

/* Declare every linked backend's entry points ...  */
#define ENGINE_BACKEND(name)                                                \
  SANE_Status sane_##name##_init (SANE_Int *, SANE_Auth_Callback);         \
  void sane_##name##_exit (void);                                          \
  SANE_Status sane_##name##_get_devices (const SANE_Device ***, SANE_Bool); \
  SANE_Status sane_##name##_open (SANE_String_Const, SANE_Handle *);       \
  void sane_##name##_close (SANE_Handle);                                  \
  const SANE_Option_Descriptor *sane_##name##_get_option_descriptor (      \
    SANE_Handle, SANE_Int);                                                \
  SANE_Status sane_##name##_control_option (SANE_Handle, SANE_Int,         \
                                            SANE_Action, void *,           \
                                            SANE_Int *);                   \
  SANE_Status sane_##name##_get_parameters (SANE_Handle, SANE_Parameters *); \
  SANE_Status sane_##name##_start (SANE_Handle);                           \
  SANE_Status sane_##name##_read (SANE_Handle, SANE_Byte *, SANE_Int,      \
                                  SANE_Int *);                             \
  void sane_##name##_cancel (SANE_Handle);
#include "engine_backend_list.h"
#undef ENGINE_BACKEND

/* ... and list them.  */
#define ENGINE_BACKEND(name)                                                \
  { #name,                                                                 \
    sane_##name##_init,                                                    \
    sane_##name##_exit,                                                    \
    sane_##name##_get_devices,                                             \
    sane_##name##_open,                                                    \
    sane_##name##_close,                                                   \
    sane_##name##_get_option_descriptor,                                   \
    sane_##name##_control_option,                                          \
    sane_##name##_get_parameters,                                          \
    sane_##name##_start,                                                   \
    sane_##name##_read,                                                    \
    sane_##name##_cancel },
static const engine_backend backends[] = {
#include "engine_backend_list.h"
};
#undef ENGINE_BACKEND

const engine_backend *
engine_find_backend (const char *name)
{
  size_t i;
  for (i = 0; i < sizeof backends / sizeof backends[0]; i++)
    if (strcmp (backends[i].name, name) == 0)
      return &backends[i];
  return NULL;
}

/* ---- handles ---------------------------------------------------------------- */

/* Which backends sane_init has run for, and whether a USB handle is open,
   under backend_lock.  sanei_usb's Android fd, and the device list rescans
   that follow a change of it, change only under it too.  */
static int initialised[sizeof backends / sizeof backends[0]];
static int usb_open;
static pthread_mutex_t backend_lock = PTHREAD_MUTEX_INITIALIZER;

/* Initialises BACKEND on its first use, under backend_lock.  */
static SANE_Status
init_backend (const engine_backend *backend)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int version;
  if (!initialised[backend - backends])
    {
      status = backend->init (&version, NULL);
      initialised[backend - backends] = status == SANE_STATUS_GOOD;
    }
  return status;
}

/* Opens DEVICE of BACKEND, initialised, into a new handle on USB_FD.  */
static SANE_Status
open_handle (const engine_backend *backend, SANE_String_Const device,
             int usb_fd, engine_handle **handle)
{
  engine_handle *h;
  SANE_Handle sane;
  SANE_Status status = backend->open (device, &sane);
  if (status != SANE_STATUS_GOOD)
    return status;
  h = calloc (1, sizeof *h);
  if (!h)
    {
      backend->close (sane);
      return SANE_STATUS_NO_MEM;
    }
  h->backend = backend;
  h->sane = sane;
  h->usb_fd = usb_fd;
  pthread_mutex_init (&h->lock, NULL);
  atomic_init (&h->cancelled, 0);
  *handle = h;
  return SANE_STATUS_GOOD;
}

SANE_Status
engine_open (const char *backend_name, SANE_String_Const device,
             engine_handle **handle)
{
  const engine_backend *backend = engine_find_backend (backend_name);
  SANE_Status status;

  if (!backend)
    return SANE_STATUS_INVAL;
  pthread_mutex_lock (&backend_lock);
  status = init_backend (backend);
  pthread_mutex_unlock (&backend_lock);
  if (status != SANE_STATUS_GOOD)
    return status;
  return open_handle (backend, device, -1, handle);
}

SANE_Status
engine_open_usb (const char *backend_name, int fd, engine_handle **handle)
{
  const engine_backend *backend = engine_find_backend (backend_name);
  const SANE_Device **devices;
  SANE_Status status;

  if (fd < 0)
    return SANE_STATUS_INVAL;
  if (!backend)
    {
      close (fd);
      return SANE_STATUS_INVAL;
    }
  pthread_mutex_lock (&backend_lock);
  if (usb_open)
    status = SANE_STATUS_DEVICE_BUSY;
  else
    {
      /* A backend initialised before lists the new device after a
         rescan.  */
      sanei_usb_set_android_fd (fd);
      if (initialised[backend - backends])
        sanei_usb_scan_devices ();
      status = init_backend (backend);
      if (status == SANE_STATUS_GOOD)
        status = backend->get_devices (&devices, SANE_TRUE);
      if (status == SANE_STATUS_GOOD)
        status = devices && devices[0]
                   ? open_handle (backend, devices[0]->name, fd, handle)
                   : SANE_STATUS_INVAL;
      if (status == SANE_STATUS_GOOD)
        usb_open = 1;
      else
        sanei_usb_set_android_fd (-1);
    }
  pthread_mutex_unlock (&backend_lock);
  if (status != SANE_STATUS_GOOD)
    close (fd);
  return status;
}

const SANE_Option_Descriptor *
engine_get_option_descriptor (engine_handle *h, SANE_Int option)
{
  const SANE_Option_Descriptor *descriptor;
  pthread_mutex_lock (&h->lock);
  descriptor = h->backend->get_option_descriptor (h->sane, option);
  pthread_mutex_unlock (&h->lock);
  return descriptor;
}

SANE_Status
engine_control_option (engine_handle *h, SANE_Int option, SANE_Action action,
                       void *value, SANE_Int *info)
{
  SANE_Status status;
  pthread_mutex_lock (&h->lock);
  status = h->backend->control_option (h->sane, option, action, value, info);
  pthread_mutex_unlock (&h->lock);
  return status;
}

void
engine_cancel (engine_handle *h)
{
  /* Set first: the streaming thread then makes no further backend call
     once it releases the lock.  */
  atomic_store (&h->cancelled, 1);
  pthread_mutex_lock (&h->lock);
  h->backend->cancel (h->sane);
  pthread_mutex_unlock (&h->lock);
}

void
engine_close (engine_handle *h)
{
  pthread_mutex_lock (&h->lock);
  h->backend->close (h->sane);
  pthread_mutex_unlock (&h->lock);
  if (h->usb_fd >= 0)
    {
      pthread_mutex_lock (&backend_lock);
      sanei_usb_set_android_fd (-1);
      close (h->usb_fd);
      usb_open = 0;
      pthread_mutex_unlock (&backend_lock);
    }
  pthread_mutex_destroy (&h->lock);
  free (h);
}

/* ---- Page stream ------------------------------------------------------------ */

static int
write_all (int fd, const void *data, size_t length)
{
  const unsigned char *p = data;
  while (length > 0)
    {
      ssize_t n = write (fd, p, length);
      if (n < 0)
        {
          if (errno == EINTR)
            continue;
          return 0;
        }
      p += n;
      length -= (size_t) n;
    }
  return 1;
}

static void
put_i32 (unsigned char *p, int32_t value)
{
  uint32_t v = (uint32_t) value;
  p[0] = (unsigned char) v;
  p[1] = (unsigned char) (v >> 8);
  p[2] = (unsigned char) (v >> 16);
  p[3] = (unsigned char) (v >> 24);
}

static int
write_head (int fd, const SANE_Parameters *p)
{
  unsigned char record[4 + 6 * 4];
  memcpy (record, ENGINE_RECORD_HEAD, 4);
  put_i32 (record + 4, p->format);
  put_i32 (record + 8, p->last_frame);
  put_i32 (record + 12, p->bytes_per_line);
  put_i32 (record + 16, p->pixels_per_line);
  put_i32 (record + 20, p->lines);
  put_i32 (record + 24, p->depth);
  return write_all (fd, record, sizeof record);
}

static int
write_data (int fd, const SANE_Byte *data, SANE_Int length)
{
  unsigned char record[4 + 4];
  memcpy (record, ENGINE_RECORD_DATA, 4);
  put_i32 (record + 4, length);
  return write_all (fd, record, sizeof record)
         && write_all (fd, data, (size_t) length);
}

static int
write_end (int fd, SANE_Status status)
{
  unsigned char record[4 + 4];
  memcpy (record, ENGINE_RECORD_END, 4);
  put_i32 (record + 4, status);
  return write_all (fd, record, sizeof record);
}

/* Sets STATUS to the result of CALL, made under the handle's lock, or to
   SANE_STATUS_CANCELLED without making it once the Scan Job is cancelled.  */
#define LOCKED_CALL(h, status, call)                                        \
  do                                                                        \
    {                                                                       \
      pthread_mutex_lock (&(h)->lock);                                      \
      status = atomic_load (&(h)->cancelled) ? SANE_STATUS_CANCELLED       \
                                             : (call);                      \
      pthread_mutex_unlock (&(h)->lock);                                    \
    }                                                                       \
  while (0)

SANE_Status
engine_stream_page (engine_handle *h, int fd)
{
  const engine_backend *backend = h->backend;
  SANE_Parameters params;
  SANE_Status status;
  SANE_Int length;
  int writable = 1;

  do
    {
      LOCKED_CALL (h, status, backend->start (h->sane));
      if (status == SANE_STATUS_GOOD)
        LOCKED_CALL (h, status, backend->get_parameters (h->sane, &params));
      if (status == SANE_STATUS_GOOD)
        writable = write_head (fd, &params);
      while (writable && status == SANE_STATUS_GOOD)
        {
          LOCKED_CALL (h, status, backend->read (h->sane, h->buffer,
                                                 sizeof h->buffer, &length));
          if (status == SANE_STATUS_GOOD && length > 0)
            writable = write_data (fd, h->buffer, length);
        }
      if (status == SANE_STATUS_EOF)
        status = SANE_STATUS_GOOD;
    }
  while (writable && status == SANE_STATUS_GOOD && !params.last_frame);

  if (writable)
    writable = write_end (fd, status);
  if (!writable)
    status = SANE_STATUS_IO_ERROR;
  if (status != SANE_STATUS_GOOD)
    {
      pthread_mutex_lock (&h->lock);
      backend->cancel (h->sane);
      pthread_mutex_unlock (&h->lock);
    }
  return status;
}
