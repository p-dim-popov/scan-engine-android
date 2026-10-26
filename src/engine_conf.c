/* scan-engine-android
   Copyright (C) 2026 Petar Popov

   The backends' configuration files, built in: an Android app has no
   configuration directory, so sanei_config_open falls back to the upstream
   <name>.conf.in of each linked backend, compiled into the Engine.

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

#include <stdio.h>
#include <string.h>

#include "sane/sanei_config.h"

typedef struct
{
  const char *name;
  const char *text;
} engine_conf;

/* CMake writes one ENGINE_CONF("<name>.conf", "<text>") line per linked
   backend that has a conf.  */
static const engine_conf confs[] = {
#define ENGINE_CONF(name, text) { name, text },
#include "engine_conf_list.h"
#undef ENGINE_CONF
  { NULL, NULL }
};

FILE *
sanei_config_open_builtin (const char *name)
{
  const engine_conf *conf;

  for (conf = confs; conf->name; conf++)
    if (strcmp (conf->name, name) == 0)
      return fmemopen ((void *) conf->text, strlen (conf->text), "r");
  return NULL;
}
