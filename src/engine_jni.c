/* scan-engine-android
   Copyright (C) 2026 Petar Popov

   JNI glue between the Engine's Service and the Engine core.

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

#include <jni.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"

#define JNI(name) Java_io_github_pdimpopov_scanengine_NativeEngine_##name

JNIEXPORT jint
JNI_OnLoad (JavaVM *vm, void *reserved)
{
  (void) vm;
  (void) reserved;
  /* The app may close its end of a Page stream at any time (engine.h).  */
  signal (SIGPIPE, SIG_IGN);
  return JNI_VERSION_1_6;
}

static engine_handle *
handle_of (jlong pointer)
{
  return (engine_handle *) (intptr_t) pointer;
}

JNIEXPORT jint JNICALL
JNI (open) (JNIEnv *env, jclass cls, jstring backend, jstring device,
            jlongArray handle)
{
  const char *b = (*env)->GetStringUTFChars (env, backend, NULL);
  const char *d = (*env)->GetStringUTFChars (env, device, NULL);
  engine_handle *h;
  SANE_Status status = engine_open (b, d, &h);
  (void) cls;
  if (status == SANE_STATUS_GOOD)
    {
      jlong pointer = (jlong) (intptr_t) h;
      (*env)->SetLongArrayRegion (env, handle, 0, 1, &pointer);
    }
  (*env)->ReleaseStringUTFChars (env, device, d);
  (*env)->ReleaseStringUTFChars (env, backend, b);
  return status;
}

JNIEXPORT jint JNICALL
JNI (openUsb) (JNIEnv *env, jclass cls, jstring backend, jint fd,
               jlongArray handle)
{
  const char *b = (*env)->GetStringUTFChars (env, backend, NULL);
  engine_handle *h;
  SANE_Status status = engine_open_usb (b, fd, &h);
  (void) cls;
  if (status == SANE_STATUS_GOOD)
    {
      jlong pointer = (jlong) (intptr_t) h;
      (*env)->SetLongArrayRegion (env, handle, 0, 1, &pointer);
    }
  (*env)->ReleaseStringUTFChars (env, backend, b);
  return status;
}

JNIEXPORT jint JNICALL
JNI (optionCount) (JNIEnv *env, jclass cls, jlong handle)
{
  SANE_Int count = 0;
  (void) env;
  (void) cls;
  if (engine_control_option (handle_of (handle), 0, SANE_ACTION_GET_VALUE,
                             &count, NULL)
      != SANE_STATUS_GOOD)
    return 0;
  return count;
}

static void
set_string_field (JNIEnv *env, jobject object, jclass c, const char *field,
                  const char *value)
{
  jstring s = value ? (*env)->NewStringUTF (env, value) : NULL;
  (*env)->SetObjectField (
    env, object, (*env)->GetFieldID (env, c, field, "Ljava/lang/String;"), s);
  if (s)
    (*env)->DeleteLocalRef (env, s);
}

static void
set_int_field (JNIEnv *env, jobject object, jclass c, const char *field,
               jint value)
{
  (*env)->SetIntField (env, object, (*env)->GetFieldID (env, c, field, "I"),
                       value);
}

JNIEXPORT jboolean JNICALL
JNI (optionDescriptor) (JNIEnv *env, jclass cls, jlong handle, jint option,
                        jobject descriptor)
{
  const SANE_Option_Descriptor *d =
    engine_get_option_descriptor (handle_of (handle), option);
  jclass c;
  (void) cls;
  if (!d)
    return JNI_FALSE;
  c = (*env)->GetObjectClass (env, descriptor);
  set_string_field (env, descriptor, c, "name", d->name);
  set_string_field (env, descriptor, c, "title", d->title);
  set_string_field (env, descriptor, c, "desc", d->desc);
  set_int_field (env, descriptor, c, "type", d->type);
  set_int_field (env, descriptor, c, "unit", d->unit);
  set_int_field (env, descriptor, c, "size", d->size);
  set_int_field (env, descriptor, c, "cap", d->cap);
  set_int_field (env, descriptor, c, "constraintType", d->constraint_type);
  switch (d->constraint_type)
    {
    case SANE_CONSTRAINT_RANGE:
      set_int_field (env, descriptor, c, "rangeMin", d->constraint.range->min);
      set_int_field (env, descriptor, c, "rangeMax", d->constraint.range->max);
      set_int_field (env, descriptor, c, "rangeQuant",
                     d->constraint.range->quant);
      break;
    case SANE_CONSTRAINT_WORD_LIST:
      {
        const SANE_Word *list = d->constraint.word_list;
        jintArray words = (*env)->NewIntArray (env, list[0]);
        (*env)->SetIntArrayRegion (env, words, 0, list[0],
                                   (const jint *) list + 1);
        (*env)->SetObjectField (
          env, descriptor, (*env)->GetFieldID (env, c, "wordList", "[I"),
          words);
        (*env)->DeleteLocalRef (env, words);
      }
      break;
    case SANE_CONSTRAINT_STRING_LIST:
      {
        const SANE_String_Const *list = d->constraint.string_list;
        jsize count = 0, i;
        jobjectArray strings;
        while (list[count])
          count++;
        strings = (*env)->NewObjectArray (
          env, count, (*env)->FindClass (env, "java/lang/String"), NULL);
        for (i = 0; i < count; i++)
          {
            jstring s = (*env)->NewStringUTF (env, list[i]);
            (*env)->SetObjectArrayElement (env, strings, i, s);
            (*env)->DeleteLocalRef (env, s);
          }
        (*env)->SetObjectField (
          env, descriptor,
          (*env)->GetFieldID (env, c, "stringList", "[Ljava/lang/String;"),
          strings);
        (*env)->DeleteLocalRef (env, strings);
      }
      break;
    default:
      break;
    }
  return JNI_TRUE;
}

static void
put_info (JNIEnv *env, jintArray info, SANE_Int value)
{
  jint v = value;
  (*env)->SetIntArrayRegion (env, info, 0, 1, &v);
}

JNIEXPORT jint JNICALL
JNI (setWordOption) (JNIEnv *env, jclass cls, jlong handle, jint option,
                     jintArray value, jintArray info)
{
  engine_handle *h = handle_of (handle);
  const SANE_Option_Descriptor *d = engine_get_option_descriptor (h, option);
  SANE_Int flags = 0;
  SANE_Status status;
  jint *words;
  (void) cls;
  if (!d
      || (size_t) (*env)->GetArrayLength (env, value) * sizeof (SANE_Word)
           < (size_t) d->size)
    return SANE_STATUS_INVAL;
  words = (*env)->GetIntArrayElements (env, value, NULL);
  status = engine_control_option (h, option, SANE_ACTION_SET_VALUE, words,
                                  &flags);
  (*env)->ReleaseIntArrayElements (env, value, words, 0);
  put_info (env, info, flags);
  return status;
}

JNIEXPORT jint JNICALL
JNI (setStringOption) (JNIEnv *env, jclass cls, jlong handle, jint option,
                       jstring value, jintArray info)
{
  engine_handle *h = handle_of (handle);
  const SANE_Option_Descriptor *d = engine_get_option_descriptor (h, option);
  SANE_Int flags = 0;
  SANE_Status status;
  const char *v;
  char *buffer;
  (void) cls;
  if (!d || d->size < 1)
    return SANE_STATUS_INVAL;
  /* sane_control_option reads and writes size bytes.  */
  buffer = calloc ((size_t) d->size, 1);
  if (!buffer)
    return SANE_STATUS_NO_MEM;
  v = (*env)->GetStringUTFChars (env, value, NULL);
  strncpy (buffer, v, (size_t) d->size - 1);
  (*env)->ReleaseStringUTFChars (env, value, v);
  status = engine_control_option (h, option, SANE_ACTION_SET_VALUE, buffer,
                                  &flags);
  free (buffer);
  put_info (env, info, flags);
  return status;
}

JNIEXPORT jint JNICALL
JNI (streamPage) (JNIEnv *env, jclass cls, jlong handle, jint fd)
{
  (void) env;
  (void) cls;
  return engine_stream_page (handle_of (handle), fd);
}

JNIEXPORT void JNICALL
JNI (cancel) (JNIEnv *env, jclass cls, jlong handle)
{
  (void) env;
  (void) cls;
  engine_cancel (handle_of (handle));
}

JNIEXPORT void JNICALL
JNI (close) (JNIEnv *env, jclass cls, jlong handle)
{
  (void) env;
  (void) cls;
  engine_close (handle_of (handle));
}
