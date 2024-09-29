/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>

  unescape.h is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  unescape.h is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 *  file: buffered_io_py.c
 *  created on: 28 Sep 2024
 *
 *  Buffered_io library python C API extension
 *
 *  Compilation:
 *    replace xx with your python version:
 *      cc -Wall -Wextra -Werror -shared -fPIC \
 *        $(pkg-config --cflags python-3.xx) \
 *        buffered_io_py.c -o buffered_io.so
 *
 *    compilation options:
 *      `-D_DEBUG`:  to print some extra debug information
 *      `-D BMAX=`:  to change the default buffer length (1kb)
 *
 *  Usage:
 *  ```{py}
 *    import buffered_io
 *
 *    # initialization
 *    # optionally pass buffer length in bytes
 *    b = buffered_io.new()
 *
 *    # use library functions here
 *    # see `help(buffered_io)`
 *    b.putc("!")
 *    b.puts("hi")
 *
 *    # flush the buffer at the end
 *    # it gets flushed automatically at the end of your program
 *    # and when you deference `b` variable
 *    b.flush()
 *  ```
 **/
#include <stdlib.h>
#include <fcntl.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define BIO_IMPLEMENTATION
#include "buffered_io.h"

#define MAX_ALLOC 1024*1024*1024 // 1G
#ifndef BMAX
#  define BMAX 1024 // 1k
#endif

#ifndef PYBIODEFF
#  define PYBIODEFF static PyObject *
#endif

#ifdef _DEBUG
#  include <stdio.h>
#  ifndef _DEBUG_FD
#    define _DEBUG_F stderr /* debug output file */
#  endif
#  define __printd(format, ...) fprintf (_DEBUG_F, format, ##__VA_ARGS__)
#  define printd(format, ...) \
  __printd ("[debug %s:%d]\t "format, __func__, __LINE__, ##__VA_ARGS__)
#else
#  define __printd(format, ...)
#  define printd(format, ...)
#endif

#undef UNUSED
#define UNUSED(x) (void)(x)

/* PyObject compatible */
typedef struct {
  PyObject_HEAD
  uchar *mem;
  BIO_t *bio;
} BIO_Object;

/* BIO_Object allocator and destructor */
PYBIODEFF BIO_Object_alloc (PyTypeObject *type, PyObject *args, PyObject *kwds);
static void BIO_Object_free (BIO_Object *self);
/* new BIO_Type */
PYBIODEFF pybio_new (PyObject *self, PyObject *args);
PYBIODEFF pybio_setofd (BIO_Object *self, PyObject *args);
/* flush */
PYBIODEFF pybio_flush (BIO_Object *self, PyObject *args);
PYBIODEFF pybio_flushln (BIO_Object *self, PyObject *args);
/* putc */
PYBIODEFF pybio_putc (BIO_Object *self, PyObject *args);
/* put requires buffer length */
PYBIODEFF pybio_put (BIO_Object *self, PyObject *args);
PYBIODEFF pybio_putln (BIO_Object *self, PyObject *args);
/* put str */
PYBIODEFF pybio_fputs (BIO_Object *self, PyObject *args); // no newline
PYBIODEFF pybio_puts (BIO_Object *self, PyObject *args); // extra trailing newline

/* main module */
static PyMethodDef funs[] =
  {
    {
      "new", pybio_new, METH_VARARGS,
      "Initialization\n"
      "  optionally pass buffer length"
    },
    {NULL}
  };

/* bio object */
static PyMethodDef bio_funs[] =
  {
    {
      "set_out_fd", (PyCFunction)pybio_setofd, METH_VARARGS,
      "set_out_fd(int fd)\n"
      "to change the output file descriptor\n"
      "\nParameters:  \n"
      "  fd (int): file descriptor\n"
    },{
      "putc", (PyCFunction)pybio_putc, METH_VARARGS,
      "putc(string str)\n"
      "to put a single character\n"
      "\nParameters:\n"
      "  str (string): it only uses it's first byte\n"
    },{
      "puts", (PyCFunction)pybio_puts, METH_VARARGS,
      "puts(string str)\n"
      "to put a string and a trailing newline at the end\n"
      "\nParameters:\n"
      "  str (string): input string"
    },{
      "fputs", (PyCFunction)pybio_fputs, METH_VARARGS,
      "fputs(string str)\n"
      "to put the given string\n"
      "\nParameters:\n"
      "  str (string): input string"
    },{
      "put", (PyCFunction)pybio_put, METH_VARARGS,
      "put(bytes b)\n"
      "to put the given bytes array\n"
      "\nParameters:\n"
      "  b (bytes): input bytes b'xxx'"
    },{
      "putln", (PyCFunction)pybio_putln, METH_VARARGS,
      "put(bytes b)\n"
      "like put function, also puts a newline\n"
      "\nParameters:\n"
      "  b (bytes): input bytes"
    },{
      "flush", (PyCFunction)pybio_flush, METH_NOARGS,
      "flush the buffer"
    },{
      "flushln", (PyCFunction)pybio_flushln, METH_NOARGS,
      "like flush, also puts a newline"
    },
    {NULL}
};

static PyTypeObject BIO_Type = {
    PyVarObject_HEAD_INIT (NULL, 0)
    .tp_name = "buffered_io.bio",
    .tp_basicsize = sizeof (BIO_Object),
    .tp_itemsize = 0,
    .tp_new = BIO_Object_alloc,
    .tp_dealloc = (destructor)BIO_Object_free,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_init = NULL,
    .tp_repr = NULL,
    .tp_methods = bio_funs,
};

static struct PyModuleDef bio_def = {
  .m_base = PyModuleDef_HEAD_INIT,
  .m_name = "buffered_io",
  .m_doc = "buffered_io python C API extension",
  .m_size = -1,
  .m_methods = funs,
  .m_traverse = NULL,
  .m_clear = NULL,
  .m_free = NULL
};

PYBIODEFF
pybio_flush (BIO_Object *self, PyObject *args)
{
  UNUSED (args);
  bio_flush (self->bio);
  printd ("bio flush\n");
  Py_RETURN_NONE;
}

PYBIODEFF
pybio_flushln (BIO_Object *self, PyObject *args)
{
  UNUSED (args);
  bio_flushln (self->bio);
  printd ("bio flush with newline\n");
  Py_RETURN_NONE;
}

PYBIODEFF
pybio_putc (BIO_Object *self, PyObject *args)
{
  if (self && self->bio)
    {
      const char* str;
      if (!PyArg_ParseTuple (args, "s", &str))
        Py_RETURN_NONE;

      bio_putc (self->bio, *str);
    }
  Py_RETURN_NONE;
}

PYBIODEFF
pybio_fputs (BIO_Object *self, PyObject *args)
{
  if (self && self->bio)
    {
      const char* str;
      if (!PyArg_ParseTuple (args, "s", &str))
        Py_RETURN_NONE;

      bio_fputs (self->bio, str);
    }
  Py_RETURN_NONE;
}

PYBIODEFF
pybio_puts (BIO_Object *self, PyObject *args)
{
  if (self && self->bio)
    {
      const char* str;
      if (!PyArg_ParseTuple (args, "s", &str))
        Py_RETURN_NONE;

      bio_puts (self->bio, str);
    }
  Py_RETURN_NONE;
}

PYBIODEFF
pybio_put (BIO_Object *self, PyObject *args)
{
  if (self && self->bio)
    {
      const char *bytes_ptr;
      PyObject *bytes_obj;
      Py_ssize_t bytes_len;
      if (!PyArg_ParseTuple (args, "O!", &PyBytes_Type, &bytes_obj))
        Py_RETURN_NONE;
      bytes_ptr = PyBytes_AsString (bytes_obj);
      if (!bytes_ptr)
        Py_RETURN_NONE;
      bytes_len = PyBytes_Size (bytes_obj);

      bio_put (self->bio, bytes_ptr, bytes_len);
    }
  Py_RETURN_NONE;
}

PYBIODEFF
pybio_putln (BIO_Object *self, PyObject *args)
{
  if (self && self->bio)
    {
      const char *bytes_ptr;
      PyObject *bytes_obj;
      Py_ssize_t bytes_len;
      if (!PyArg_ParseTuple (args, "O!", &PyBytes_Type, &bytes_obj))
        Py_RETURN_NONE;
      bytes_ptr = PyBytes_AsString (bytes_obj);
      if (!bytes_ptr)
        Py_RETURN_NONE;
      bytes_len = PyBytes_Size (bytes_obj);

      bio_putln (self->bio, bytes_ptr, bytes_len);
    }
  Py_RETURN_NONE;
}

PYBIODEFF
BIO_Object_alloc (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  BIO_Object *self;
  long cap;
  UNUSED (kwds);
  if (PyTuple_Size (args) == 0 || /* no arg */
      !PyArg_ParseTuple (args, "l", &cap) || /* invalid arg */
      cap >= MAX_ALLOC) /* too big */
    {
      cap = BMAX;
    }

  if ((self = (BIO_Object *)type->tp_alloc (type, 0)))
    {
      self->bio = malloc (sizeof (BIO_t));
      self->mem = malloc (cap);
      if (!self->bio || !self->mem)
        {
          Py_DECREF (self);
          return NULL; /* allocation failure */
        }
      *self->bio = bio_new (cap, self->mem, 1);
      printd ("bio @%p was allocated with mem[.%d] @%p\n",
              self->bio, self->bio->len, self->bio->buffer);
    }
  return (PyObject *)self;
}


PYBIODEFF
pybio_new (PyObject *self, PyObject *args)
{
  UNUSED (self);
  return BIO_Object_alloc (&BIO_Type, args, NULL);
}

PYBIODEFF
pybio_setofd (BIO_Object *self, PyObject *args)
{
  if (self && self->bio)
    {
      int fd;
      if (!PyArg_ParseTuple (args, "i", &fd))
        Py_RETURN_NONE;

      if (fcntl (fd, F_GETFD) == -1)
        {
          printd ("fd %d is not open\n", fd);
          Py_RETURN_NONE;
        }

      bio_out (self->bio, fd);
      printd ("bip output fd was changed to %d\n", fd);
    }
  Py_RETURN_NONE;
}

static void
BIO_Object_free (BIO_Object *self)
{
  if (bio_has_more (self->bio))
    {
      printd ("bio flush before destroying\n");
      bio_flush (self->bio);
    }
  printd ("bio @%p was destroyed\n", self->bio);
  free (self->bio);
  (Py_TYPE (self))->tp_free ((PyObject *) self);
}

PyMODINIT_FUNC
PyInit_buffered_io ()
{
  PyObject *module = PyModule_Create (&bio_def);
  if (!module)
    return NULL;
  if (PyType_Ready (&BIO_Type) < 0)
    return NULL;

  Py_INCREF (&BIO_Type);
  PyModule_AddObject (module, "bio", (PyObject *)&BIO_Type);

  return module;
}
