/* scan-engine-android
   Copyright (C) 2026 Petar Popov

   Host test of the Engine core.

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

/* Host test of the Engine core: scans with the SANE test backend through
   engine_stream_page into a pipe and reads the Page stream back the way the
   app does.  Run one test by name, or all with no argument.  */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include "engine.h"
#include "sane/saneopts.h"
#include "sane/sanei_thread.h"

static int failures;

#define CHECK(cond)                                                        \
  do                                                                       \
    {                                                                      \
      if (!(cond))                                                         \
        {                                                                  \
          fprintf (stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
                   #cond);                                                 \
          failures++;                                                      \
        }                                                                  \
    }                                                                      \
  while (0)

#define CHECK_EQ(actual, expected)                                         \
  do                                                                       \
    {                                                                      \
      long long a_ = (long long) (actual), e_ = (long long) (expected);    \
      if (a_ != e_)                                                        \
        {                                                                  \
          fprintf (stderr, "%s:%d: %s is %lld, expected %lld\n", __FILE__, \
                   __LINE__, #actual, a_, e_);                             \
          failures++;                                                      \
        }                                                                  \
    }                                                                      \
  while (0)

/* ---- Page stream reader ---------------------------------------------------- */

#define MAX_FRAMES 4

typedef struct
{
  int format, last_frame, bytes_per_line, pixels_per_line, lines, depth;
  long long bytes;
  uint64_t hash; /* FNV-1a over the frame's bytes, in order */
} frame;

typedef struct
{
  int fd;
  long long close_after_bytes; /* < 0: read to the end; 0: close after HEAD */
  frame frames[MAX_FRAMES];
  int frame_count;
  int end_seen;
  int end_status;
  int malformed;
  long long bytes_after_end;
} page_reader;

#define FNV1A_START 0xcbf29ce484222325ULL

static uint64_t
fnv1a (uint64_t hash, unsigned char byte)
{
  return (hash ^ byte) * 0x100000001b3ULL;
}

static int
read_exact (int fd, void *buf, size_t n)
{
  unsigned char *p = buf;
  while (n > 0)
    {
      ssize_t r = read (fd, p, n);
      if (r < 0 && errno == EINTR)
        continue;
      if (r <= 0)
        return 0;
      p += r;
      n -= (size_t) r;
    }
  return 1;
}

static int
read_i32 (int fd, int32_t *v)
{
  unsigned char b[4];
  if (!read_exact (fd, b, 4))
    return 0;
  *v = (int32_t) ((uint32_t) b[0] | (uint32_t) b[1] << 8
                  | (uint32_t) b[2] << 16 | (uint32_t) b[3] << 24);
  return 1;
}

static void *
read_page (void *arg)
{
  page_reader *r = arg;
  static unsigned char buf[64 * 1024];
  long long total = 0;
  char tag[4];

  while (read_exact (r->fd, tag, 4))
    {
      if (r->end_seen)
        {
          r->bytes_after_end += 4;
          continue;
        }
      if (memcmp (tag, ENGINE_RECORD_HEAD, 4) == 0)
        {
          int32_t v[6];
          int i;
          for (i = 0; i < 6; i++)
            if (!read_i32 (r->fd, &v[i]))
              {
                r->malformed = 1;
                return NULL;
              }
          if (r->frame_count == MAX_FRAMES)
            {
              r->malformed = 1;
              return NULL;
            }
          frame *p = &r->frames[r->frame_count++];
          memset (p, 0, sizeof *p);
          p->hash = FNV1A_START;
          p->format = v[0];
          p->last_frame = v[1];
          p->bytes_per_line = v[2];
          p->pixels_per_line = v[3];
          p->lines = v[4];
          p->depth = v[5];
          if (r->close_after_bytes == 0)
            {
              close (r->fd);
              return NULL;
            }
        }
      else if (memcmp (tag, ENGINE_RECORD_DATA, 4) == 0)
        {
          int32_t len;
          if (r->frame_count == 0 || !read_i32 (r->fd, &len) || len <= 0)
            {
              r->malformed = 1;
              return NULL;
            }
          frame *p = &r->frames[r->frame_count - 1];
          while (len > 0)
            {
              size_t n = (size_t) len < sizeof buf ? (size_t) len : sizeof buf;
              size_t i;
              if (!read_exact (r->fd, buf, n))
                {
                  r->malformed = 1;
                  return NULL;
                }
              for (i = 0; i < n; i++)
                p->hash = fnv1a (p->hash, buf[i]);
              p->bytes += (long long) n;
              total += (long long) n;
              len -= (int32_t) n;
            }
          if (r->close_after_bytes >= 0 && total >= r->close_after_bytes)
            {
              close (r->fd);
              return NULL;
            }
        }
      else if (memcmp (tag, ENGINE_RECORD_END, 4) == 0)
        {
          int32_t status;
          if (!read_i32 (r->fd, &status))
            {
              r->malformed = 1;
              return NULL;
            }
          r->end_seen = 1;
          r->end_status = status;
        }
      else
        {
          r->malformed = 1;
          return NULL;
        }
    }
  close (r->fd);
  return NULL;
}

/* ---- test backend session -------------------------------------------------- */

static const engine_backend *backend;
static SANE_Handle handle;

static void
open_test_scanner (void)
{
  SANE_Int version;
  backend = engine_find_backend ("test");
  if (!backend)
    {
      fprintf (stderr, "the test backend is not linked into the Engine\n");
      exit (1);
    }
  if (backend->init (&version, NULL) != SANE_STATUS_GOOD
      || backend->open ("0", &handle) != SANE_STATUS_GOOD)
    {
      fprintf (stderr, "cannot open the test backend's device 0\n");
      exit (1);
    }
}

static void
close_test_scanner (void)
{
  backend->cancel (handle);
  backend->close (handle);
  backend->exit ();
}

static int
find_option (const char *name)
{
  SANE_Int count = 0, i;
  backend->control_option (handle, 0, SANE_ACTION_GET_VALUE, &count, NULL);
  for (i = 1; i < count; i++)
    {
      const SANE_Option_Descriptor *d =
        backend->get_option_descriptor (handle, i);
      if (d && d->name && strcmp (d->name, name) == 0)
        return i;
    }
  fprintf (stderr, "test backend has no option %s\n", name);
  exit (1);
}

static void
set_string (const char *name, const char *value)
{
  char buf[256];
  snprintf (buf, sizeof buf, "%s", value);
  CHECK_EQ (backend->control_option (handle, find_option (name),
                                     SANE_ACTION_SET_VALUE, buf, NULL),
            SANE_STATUS_GOOD);
}

static void
set_number (const char *name, double value)
{
  int option = find_option (name);
  const SANE_Option_Descriptor *d =
    backend->get_option_descriptor (handle, option);
  SANE_Word w = d->type == SANE_TYPE_FIXED ? SANE_FIX (value)
                                           : (SANE_Word) value;
  CHECK_EQ (backend->control_option (handle, option, SANE_ACTION_SET_VALUE,
                                     &w, NULL),
            SANE_STATUS_GOOD);
}

static void
set_bool (const char *name, SANE_Bool value)
{
  CHECK_EQ (backend->control_option (handle, find_option (name),
                                     SANE_ACTION_SET_VALUE, &value, NULL),
            SANE_STATUS_GOOD);
}

/* A 10 mm square at 254 dpi: 100 x 100 pixels.  The test backend rounds
   geometry to whole millimetres.  */
static void
set_100_pixel_square (void)
{
  set_number (SANE_NAME_SCAN_RESOLUTION, 254);
  set_number (SANE_NAME_SCAN_TL_X, 0);
  set_number (SANE_NAME_SCAN_TL_Y, 0);
  set_number (SANE_NAME_SCAN_BR_X, 10);
  set_number (SANE_NAME_SCAN_BR_Y, 10);
}

/* Streams one Page into a pipe while a reader thread consumes it.  */
static SANE_Status
stream_page (page_reader *reader, long long close_after_bytes)
{
  int fds[2];
  pthread_t thread;
  SANE_Status status;

  memset (reader, 0, sizeof *reader);
  reader->close_after_bytes = close_after_bytes;
  if (pipe (fds) != 0)
    {
      perror ("pipe");
      exit (1);
    }
  reader->fd = fds[0];
  pthread_create (&thread, NULL, read_page, reader);
  status = engine_stream_page (backend, handle, fds[1]);
  close (fds[1]);
  pthread_join (thread, NULL);
  return status;
}

/* Reads one frame straight from the backend, without the Engine, and
   hashes it the way the Page reader does.  */
static uint64_t
direct_frame_hash (void)
{
  static SANE_Byte buf[64 * 1024];
  uint64_t hash = FNV1A_START;
  SANE_Int length, i;
  SANE_Status status = backend->start (handle);
  while (status == SANE_STATUS_GOOD)
    {
      status = backend->read (handle, buf, sizeof buf, &length);
      if (status == SANE_STATUS_GOOD)
        for (i = 0; i < length; i++)
          hash = fnv1a (hash, buf[i]);
    }
  CHECK_EQ (status, SANE_STATUS_EOF);
  backend->cancel (handle);
  return hash;
}

/* ---- tests ------------------------------------------------------------------ */

static void
gray_page_is_one_header_its_rows_and_a_good_end (void)
{
  page_reader r;
  open_test_scanner ();
  set_string (SANE_NAME_SCAN_MODE, SANE_VALUE_SCAN_MODE_GRAY);
  set_number (SANE_NAME_BIT_DEPTH, 8);
  set_100_pixel_square ();

  CHECK_EQ (stream_page (&r, -1), SANE_STATUS_GOOD);

  CHECK (!r.malformed);
  CHECK_EQ (r.frame_count, 1);
  CHECK_EQ (r.frames[0].format, SANE_FRAME_GRAY);
  CHECK_EQ (r.frames[0].last_frame, 1);
  CHECK_EQ (r.frames[0].pixels_per_line, 100);
  CHECK_EQ (r.frames[0].bytes_per_line, 100);
  CHECK_EQ (r.frames[0].lines, 100);
  CHECK_EQ (r.frames[0].depth, 8);
  CHECK_EQ (r.frames[0].bytes, 100 * 100);
  CHECK (r.end_seen);
  CHECK_EQ (r.end_status, SANE_STATUS_GOOD);
  CHECK_EQ (r.bytes_after_end, 0);
  close_test_scanner ();
}

static void
colour_16_bit_rows_pass_through_unchanged (void)
{
  page_reader r;
  uint64_t expected;
  open_test_scanner ();
  set_string (SANE_NAME_SCAN_MODE, SANE_VALUE_SCAN_MODE_COLOR);
  set_number (SANE_NAME_BIT_DEPTH, 16);
  set_string ("test-picture", "Color pattern");
  set_100_pixel_square ();
  expected = direct_frame_hash ();

  CHECK_EQ (stream_page (&r, -1), SANE_STATUS_GOOD);

  CHECK (!r.malformed);
  CHECK_EQ (r.frame_count, 1);
  CHECK_EQ (r.frames[0].format, SANE_FRAME_RGB);
  CHECK_EQ (r.frames[0].depth, 16);
  CHECK_EQ (r.frames[0].bytes_per_line, 100 * 3 * 2);
  CHECK_EQ (r.frames[0].bytes, 100 * 100 * 3 * 2);
  CHECK (r.frames[0].hash == expected);
  CHECK_EQ (r.end_status, SANE_STATUS_GOOD);
  close_test_scanner ();
}

static void
three_pass_page_has_a_header_per_colour_frame (void)
{
  page_reader r;
  int i;
  open_test_scanner ();
  set_string (SANE_NAME_SCAN_MODE, SANE_VALUE_SCAN_MODE_COLOR);
  set_number (SANE_NAME_BIT_DEPTH, 8);
  set_bool ("three-pass", SANE_TRUE);
  set_100_pixel_square ();

  CHECK_EQ (stream_page (&r, -1), SANE_STATUS_GOOD);

  CHECK (!r.malformed);
  CHECK_EQ (r.frame_count, 3);
  CHECK_EQ (r.frames[0].format, SANE_FRAME_RED);
  CHECK_EQ (r.frames[1].format, SANE_FRAME_GREEN);
  CHECK_EQ (r.frames[2].format, SANE_FRAME_BLUE);
  CHECK_EQ (r.frames[0].last_frame, 0);
  CHECK_EQ (r.frames[1].last_frame, 0);
  CHECK_EQ (r.frames[2].last_frame, 1);
  for (i = 0; i < 3; i++)
    CHECK_EQ (r.frames[i].bytes, 100 * 100);
  CHECK (r.end_seen);
  CHECK_EQ (r.end_status, SANE_STATUS_GOOD);
  close_test_scanner ();
}

static void
page_of_unknown_height_still_ends_cleanly (void)
{
  page_reader r;
  open_test_scanner ();
  set_string (SANE_NAME_SCAN_MODE, SANE_VALUE_SCAN_MODE_GRAY);
  set_number (SANE_NAME_BIT_DEPTH, 8);
  set_100_pixel_square ();
  set_bool ("hand-scanner", SANE_TRUE);

  CHECK_EQ (stream_page (&r, -1), SANE_STATUS_GOOD);

  CHECK (!r.malformed);
  CHECK_EQ (r.frame_count, 1);
  CHECK_EQ (r.frames[0].lines, -1);
  CHECK (r.frames[0].bytes > 0);
  CHECK_EQ (r.frames[0].bytes % r.frames[0].bytes_per_line, 0);
  CHECK (r.end_seen);
  CHECK_EQ (r.end_status, SANE_STATUS_GOOD);
  close_test_scanner ();
}

static void
backend_error_ends_the_page_with_its_status (void)
{
  page_reader r;
  open_test_scanner ();
  set_100_pixel_square ();
  set_string ("read-return-value", "SANE_STATUS_JAMMED");

  CHECK_EQ (stream_page (&r, -1), SANE_STATUS_JAMMED);

  CHECK (!r.malformed);
  CHECK (r.end_seen);
  CHECK_EQ (r.end_status, SANE_STATUS_JAMMED);

  /* The failed Page leaves the scanner ready for the next one.  */
  set_string ("read-return-value", "Default");
  CHECK_EQ (stream_page (&r, -1), SANE_STATUS_GOOD);
  CHECK_EQ (r.frames[0].bytes, 100 * 100);
  close_test_scanner ();
}

/* After a backend error the Engine cancels the Page at once, while the
   backend's reader thread may still be starting.  A cancel in that window
   used to hang in about 1 newly started process in 10, and far less often
   later in a process, so this runs the backend error test 300 times, each in
   a newly executed copy of this program that SIGALRM ends if it hangs.  */
static void
backend_error_right_after_start_never_hangs (void)
{
  int i, wstatus = 0;
  fflush (NULL);
  for (i = 0; i < 300; i++)
    {
      pid_t child = fork ();
      if (child == 0)
        {
          int null = open ("/dev/null", O_WRONLY);
          dup2 (null, STDOUT_FILENO);
          alarm (2);
          execl ("/proc/self/exe", "engine_host_test",
                 "backend_error_ends_the_page_with_its_status", (char *) NULL);
          _exit (127);
        }
      if (child < 0 || waitpid (child, &wstatus, 0) != child
          || !WIFEXITED (wstatus) || WEXITSTATUS (wstatus) != 0)
        break;
    }
  CHECK (!WIFSIGNALED (wstatus));
  CHECK_EQ (i, 300);
}

static void
app_closing_the_pipe_cancels_the_page (void)
{
  page_reader r;
  open_test_scanner ();
  set_string (SANE_NAME_SCAN_MODE, SANE_VALUE_SCAN_MODE_COLOR);
  set_number (SANE_NAME_BIT_DEPTH, 8);
  /* 2000 x 2000 pixels, 12 MB: far more than a pipe buffers.  */
  set_number (SANE_NAME_SCAN_RESOLUTION, 254);
  set_number (SANE_NAME_SCAN_BR_X, 200);
  set_number (SANE_NAME_SCAN_BR_Y, 200);

  CHECK_EQ (stream_page (&r, 1), SANE_STATUS_IO_ERROR);
  CHECK (!r.end_seen);

  /* The Engine survives and the scanner takes the next Page.  */
  set_100_pixel_square ();
  CHECK_EQ (stream_page (&r, -1), SANE_STATUS_GOOD);
  CHECK_EQ (r.end_status, SANE_STATUS_GOOD);
  close_test_scanner ();
}

static int
sigpipe_is_ignored (void)
{
  struct sigaction act;
  return sigaction (SIGPIPE, NULL, &act) == 0 && act.sa_handler == SIG_IGN;
}

/* The Engine ignores SIGPIPE once for the whole process; a backend joining
   its reader thread must not undo that.  */
static void
sigpipe_stays_ignored_after_every_kind_of_page_end (void)
{
  page_reader r;
  open_test_scanner ();
  set_100_pixel_square ();

  CHECK_EQ (stream_page (&r, -1), SANE_STATUS_GOOD);
  CHECK (sigpipe_is_ignored ());

  set_string ("read-return-value", "SANE_STATUS_JAMMED");
  CHECK_EQ (stream_page (&r, -1), SANE_STATUS_JAMMED);
  CHECK (sigpipe_is_ignored ());
  set_string ("read-return-value", "Default");

  /* 2000 x 2000 pixels: the app closes the pipe while the reader runs.  */
  set_number (SANE_NAME_SCAN_BR_X, 200);
  set_number (SANE_NAME_SCAN_BR_Y, 200);
  CHECK_EQ (stream_page (&r, 1), SANE_STATUS_IO_ERROR);
  CHECK (sigpipe_is_ignored ());

  /* A Scan Job cancelled in the middle of a Page, while the reader runs.  */
  {
    static SANE_Byte buf[1024];
    SANE_Int length;
    CHECK_EQ (backend->start (handle), SANE_STATUS_GOOD);
    CHECK_EQ (backend->read (handle, buf, sizeof buf, &length),
              SANE_STATUS_GOOD);
    backend->cancel (handle);
    CHECK (sigpipe_is_ignored ());
  }
  close_test_scanner ();
}

static int
pixma_style_reader (void *stop)
{
  while (!*(volatile SANE_Bool *) stop)
    usleep (1000);
  return SANE_STATUS_GOOD;
}

/* pixma starts its reader with sanei_thread_begin, stops it with a flag and
   joins it with sanei_thread_waitpid.  No scanner is needed for that path.  */
static void
sigpipe_stays_ignored_after_a_pixma_style_reader_is_joined (void)
{
  SANE_Bool stop = SANE_FALSE;
  SANE_Pid pid;
  int status = -1;

  sanei_thread_init ();
  pid = sanei_thread_begin (pixma_style_reader, &stop);
  CHECK (sanei_thread_is_valid (pid));
  stop = SANE_TRUE;
  CHECK (sanei_thread_is_valid (sanei_thread_waitpid (pid, &status)));
  CHECK_EQ (status, SANE_STATUS_GOOD);
  CHECK (sigpipe_is_ignored ());
}

/* The backend joins its reader when it hands over the last rows, before the
   Engine writes them and END.  An app that closes the pipe then must give a
   write error on any Page, not only the first.  The run is in a child
   process, so a SIGPIPE death shows as a failure instead of ending the
   test program.  */
static void
app_closing_the_pipe_on_a_later_page_is_a_write_error (void)
{
  int wstatus = 0;
  pid_t child;
  fflush (NULL);
  child = fork ();
  if (child == 0)
    {
      page_reader r;
      open_test_scanner ();
      set_100_pixel_square ();
      CHECK_EQ (stream_page (&r, -1), SANE_STATUS_GOOD);

      /* The reader holds back the rows for 200 ms, so the app has closed
         the pipe after HEAD before the Engine writes them.  */
      set_bool ("read-delay", SANE_TRUE);
      set_number ("read-delay-duration", 200000);
      CHECK_EQ (stream_page (&r, 0), SANE_STATUS_IO_ERROR);
      CHECK (!r.end_seen);
      close_test_scanner ();
      _exit (failures ? 1 : 0);
    }
  CHECK (child > 0 && waitpid (child, &wstatus, 0) == child);
  if (WIFSIGNALED (wstatus))
    fprintf (stderr, "the Engine died of signal %d\n", WTERMSIG (wstatus));
  CHECK (WIFEXITED (wstatus) && WEXITSTATUS (wstatus) == 0);
}

static long
max_rss_kb (void)
{
  struct rusage usage;
  getrusage (RUSAGE_SELF, &usage);
  return usage.ru_maxrss;
}

static void
large_page_streams_in_bounded_memory (void)
{
  page_reader r;
  long before;
  open_test_scanner ();
  set_string (SANE_NAME_SCAN_MODE, SANE_VALUE_SCAN_MODE_COLOR);
  set_number (SANE_NAME_BIT_DEPTH, 16);
  /* 4724 x 4724 pixels of 16-bit colour: 134 MB.  */
  set_number (SANE_NAME_SCAN_RESOLUTION, 600);
  set_number (SANE_NAME_SCAN_BR_X, 200);
  set_number (SANE_NAME_SCAN_BR_Y, 200);
  before = max_rss_kb ();

  CHECK_EQ (stream_page (&r, -1), SANE_STATUS_GOOD);

  CHECK_EQ (r.frames[0].bytes, 4724LL * 4724 * 3 * 2);
  CHECK_EQ (r.end_status, SANE_STATUS_GOOD);
  /* Neither the Engine nor the backend held the Page.  */
  CHECK (max_rss_kb () - before < 16 * 1024);
  close_test_scanner ();
}

static void
linked_backends_are_found_by_name (void)
{
  const engine_backend *pixma = engine_find_backend ("pixma");
  const SANE_Device **devices;
  SANE_Int version;

  CHECK (engine_find_backend ("test") != NULL);
  CHECK (engine_find_backend ("genesys") == NULL);
  CHECK (pixma != NULL);
  if (!pixma)
    return;

  /* pixma runs through its own entry points: no device is attached here.  */
  CHECK_EQ (pixma->init (&version, NULL), SANE_STATUS_GOOD);
  CHECK_EQ (pixma->get_devices (&devices, SANE_TRUE), SANE_STATUS_GOOD);
  CHECK (devices != NULL && devices[0] == NULL);
  pixma->exit ();
}

static const struct
{
  const char *name;
  void (*run) (void);
} tests[] = {
  { "gray_page_is_one_header_its_rows_and_a_good_end",
    gray_page_is_one_header_its_rows_and_a_good_end },
  { "colour_16_bit_rows_pass_through_unchanged",
    colour_16_bit_rows_pass_through_unchanged },
  { "three_pass_page_has_a_header_per_colour_frame",
    three_pass_page_has_a_header_per_colour_frame },
  { "page_of_unknown_height_still_ends_cleanly",
    page_of_unknown_height_still_ends_cleanly },
  { "backend_error_ends_the_page_with_its_status",
    backend_error_ends_the_page_with_its_status },
  { "backend_error_right_after_start_never_hangs",
    backend_error_right_after_start_never_hangs },
  { "app_closing_the_pipe_cancels_the_page",
    app_closing_the_pipe_cancels_the_page },
  { "sigpipe_stays_ignored_after_every_kind_of_page_end",
    sigpipe_stays_ignored_after_every_kind_of_page_end },
  { "sigpipe_stays_ignored_after_a_pixma_style_reader_is_joined",
    sigpipe_stays_ignored_after_a_pixma_style_reader_is_joined },
  { "app_closing_the_pipe_on_a_later_page_is_a_write_error",
    app_closing_the_pipe_on_a_later_page_is_a_write_error },
  { "large_page_streams_in_bounded_memory",
    large_page_streams_in_bounded_memory },
  { "linked_backends_are_found_by_name",
    linked_backends_are_found_by_name },
};

int
main (int argc, char **argv)
{
  size_t i;
  int ran = 0;

  /* The app may close its end of the pipe at any time; that must surface
     as a write error, not kill the Engine.  */
  signal (SIGPIPE, SIG_IGN);

  for (i = 0; i < sizeof tests / sizeof tests[0]; i++)
    if (argc < 2 || strcmp (argv[1], tests[i].name) == 0)
      {
        int before = failures;
        tests[i].run ();
        printf ("%s %s\n", failures == before ? "PASS" : "FAIL",
                tests[i].name);
        ran++;
      }
  if (ran == 0)
    {
      fprintf (stderr, "no test called %s\n", argv[1]);
      return 2;
    }
  return failures ? 1 : 0;
}
