/* scan-engine-android
   Copyright (C) 2026 Petar Popov

   Engine core: backend table, sessions and Page stream.

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

/* ---- sessions --------------------------------------------------------------- */

/* Which backends sane_init has run for, under init_lock.  */
static int initialised[sizeof backends / sizeof backends[0]];
static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;

SANE_Status
engine_open (const char *backend_name, SANE_String_Const device,
             engine_session **session)
{
  const engine_backend *backend = engine_find_backend (backend_name);
  engine_session *s;
  SANE_Handle handle;
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int version;

  if (!backend)
    return SANE_STATUS_INVAL;
  pthread_mutex_lock (&init_lock);
  if (!initialised[backend - backends])
    {
      status = backend->init (&version, NULL);
      initialised[backend - backends] = status == SANE_STATUS_GOOD;
    }
  pthread_mutex_unlock (&init_lock);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = backend->open (device, &handle);
  if (status != SANE_STATUS_GOOD)
    return status;
  s = calloc (1, sizeof *s);
  if (!s)
    {
      backend->close (handle);
      return SANE_STATUS_NO_MEM;
    }
  s->backend = backend;
  s->handle = handle;
  pthread_mutex_init (&s->lock, NULL);
  atomic_init (&s->cancelled, 0);
  *session = s;
  return SANE_STATUS_GOOD;
}

const SANE_Option_Descriptor *
engine_get_option_descriptor (engine_session *s, SANE_Int option)
{
  const SANE_Option_Descriptor *descriptor;
  pthread_mutex_lock (&s->lock);
  descriptor = s->backend->get_option_descriptor (s->handle, option);
  pthread_mutex_unlock (&s->lock);
  return descriptor;
}

SANE_Status
engine_control_option (engine_session *s, SANE_Int option, SANE_Action action,
                       void *value, SANE_Int *info)
{
  SANE_Status status;
  pthread_mutex_lock (&s->lock);
  status = s->backend->control_option (s->handle, option, action, value, info);
  pthread_mutex_unlock (&s->lock);
  return status;
}

void
engine_cancel (engine_session *s)
{
  /* Set first: the streaming thread then makes no further backend call
     once it releases the lock.  */
  atomic_store (&s->cancelled, 1);
  pthread_mutex_lock (&s->lock);
  s->backend->cancel (s->handle);
  pthread_mutex_unlock (&s->lock);
}

void
engine_close (engine_session *s)
{
  pthread_mutex_lock (&s->lock);
  s->backend->close (s->handle);
  pthread_mutex_unlock (&s->lock);
  pthread_mutex_destroy (&s->lock);
  free (s);
}

/* ---- Page stream ------------------------------------------------------------ */

/* One sane_read buffer; the only Page data the Engine ever holds.  */
#define READ_BUFFER_SIZE (256 * 1024)

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

/* Sets STATUS to the result of CALL, made under the session lock, or to
   SANE_STATUS_CANCELLED without making it once the Scan Job is cancelled.  */
#define LOCKED_CALL(s, status, call)                                        \
  do                                                                        \
    {                                                                       \
      pthread_mutex_lock (&(s)->lock);                                      \
      status = atomic_load (&(s)->cancelled) ? SANE_STATUS_CANCELLED       \
                                             : (call);                      \
      pthread_mutex_unlock (&(s)->lock);                                    \
    }                                                                       \
  while (0)

SANE_Status
engine_stream_page (engine_session *s, int fd)
{
  static SANE_Byte buffer[READ_BUFFER_SIZE];
  const engine_backend *backend = s->backend;
  SANE_Parameters params;
  SANE_Status status;
  SANE_Int length;
  int writable = 1;

  do
    {
      LOCKED_CALL (s, status, backend->start (s->handle));
      if (status == SANE_STATUS_GOOD)
        LOCKED_CALL (s, status, backend->get_parameters (s->handle, &params));
      if (status == SANE_STATUS_GOOD)
        writable = write_head (fd, &params);
      while (writable && status == SANE_STATUS_GOOD)
        {
          LOCKED_CALL (s, status, backend->read (s->handle, buffer,
                                                 sizeof buffer, &length));
          if (status == SANE_STATUS_GOOD && length > 0)
            writable = write_data (fd, buffer, length);
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
      pthread_mutex_lock (&s->lock);
      backend->cancel (s->handle);
      pthread_mutex_unlock (&s->lock);
    }
  return status;
}
