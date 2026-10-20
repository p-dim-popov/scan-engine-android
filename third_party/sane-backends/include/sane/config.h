/* sane - Scanner Access Now Easy.
   Copyright (C) 2026 Petar Popov

   Hand-written configuration header for the scan-engine-android CMake build.
   It replaces the header that autoheader and configure generate in an
   upstream build, and serves both Android (bionic) and the Linux host build
   (glibc).

   This file is part of the SANE package.

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

#ifndef SANE_CONFIG_H
#define SANE_CONFIG_H

#define PACKAGE "sane-backends"
#define PACKAGE_NAME "sane-backends"
#define PACKAGE_VERSION "1.4.0"
#define PACKAGE_STRING "sane-backends 1.4.0"
#define VERSION "1.4.0"

#define SANE_DLL_V_MAJOR 1
#define SANE_DLL_V_MINOR 4
#define SANE_DLL_V_BUILD 0

/* No .conf files ship with the Engine; backends fall back to their
   built-in defaults when sanei_config_open finds nothing here.  */
#define PATH_SANE_CONFIG_DIR /nonexistent/sane.d
#define PATH_SANE_DATA_DIR /nonexistent/share
#define PATH_SANE_LOCK_DIR /nonexistent/lock

/* Headers and functions present in both bionic (API 26+) and glibc.  */
#define HAVE_ALLOCA_H 1
#define HAVE_FCNTL_H 1
#define HAVE_IFADDRS_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_PWD_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_SIGPROCMASK 1
#define HAVE_VSYSLOG 1
#define HAVE_GETENV 1
#define HAVE_GETPAGESIZE 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_INET_NTOP 1
#define HAVE_INET_PTON 1
#define HAVE_ISFDTYPE 1
#define HAVE_MKDIR 1
#define HAVE_NANOSLEEP 1
#define HAVE_POLL_H 1
#define HAVE_SIGACTION 1
#define HAVE_SLEEP 1
#define HAVE_SNPRINTF 1
#define HAVE_STRCASESTR 1
#define HAVE_STRDUP 1
#define HAVE_STRNDUP 1
#define HAVE_STRSEP 1
#define HAVE_STRTOD 1
#define HAVE_USLEEP 1
#define HAVE_VSNPRINTF 1
#define HAVE_SOCKLEN_T 1
#define HAVE_U_CHAR 1
#define HAVE_U_INT 1
#define HAVE_U_LONG 1
#define HAVE_U_SHORT 1

/* libusb-1.0 and libjpeg-turbo are vendored and always linked.  */
#define HAVE_LIBUSB 1
#define HAVE_LIBJPEG 1

/* Reader tasks run as threads: an Android app process must not fork.  */
#define USE_PTHREAD 1
#define HAVE_PTHREAD_H 1

#endif /* SANE_CONFIG_H */
