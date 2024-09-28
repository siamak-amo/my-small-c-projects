#include <stdlib.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define BIO_IMPLEMENTATION
#include "buffered_io.h"

#ifndef BMAX
#  define BMAX 1024 // 1k
#  define MAX_ALLOC 1024*1024*1024 // 1G
#endif

#ifndef PYBIODEFF
#  define PYBIODEFF static PyObject *
#endif

#ifdef _DEBUG
#  include <stdio.h>
#  define __printf(format, ...) printf (format, ##__VA_ARGS__)
#  define printd(format, ...) \
  fprintf (stderr, "[debug %s:%-4d] "format, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#  define __printf(format, ...)
#  define printd(format, ...)
#endif

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
PYBIODEFF pybio_new(PyObject *self, PyObject *args);
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
static PyMethodDef funs[] = {
  {"new", pybio_new, METH_VARARGS, ""},
  {NULL}
};

/* bio object */
static PyMethodDef bio_funs[] = {
    {"putc", (PyCFunction)pybio_putc, METH_VARARGS, ""},
    {"puts", (PyCFunction)pybio_puts, METH_VARARGS, ""},
    {"fputs", (PyCFunction)pybio_fputs, METH_VARARGS, ""},
    {"put", (PyCFunction)pybio_put, METH_VARARGS, ""},
    {"putln", (PyCFunction)pybio_putln, METH_VARARGS, ""},
    {"flush", (PyCFunction)pybio_flush, METH_NOARGS, ""},
    {"flushln", (PyCFunction)pybio_flushln, METH_NOARGS, ""},
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
  bio_flush (self->bio);
  printd ("bio flush\n");
  Py_RETURN_NONE;
}

PYBIODEFF
pybio_flushln (BIO_Object *self, PyObject *args)
{
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
      // Parse the input tuple to get a string
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
      // Parse the input tuple to get a string
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
      // Parse the input tuple to get a string
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
      const char *ptr;
      long len;
      // Parse the input tuple to get a string
      if (!PyArg_ParseTuple (args, "sl", &ptr, &len))
        Py_RETURN_NONE;

      bio_put (self->bio, ptr, len);
    }
  Py_RETURN_NONE;
}

PYBIODEFF
pybio_putln (BIO_Object *self, PyObject *args)
{
  if (self && self->bio)
    {
      const char *ptr;
      long len;
      // Parse the input tuple to get a string
      if (!PyArg_ParseTuple (args, "sl", &ptr, &len))
        Py_RETURN_NONE;

      bio_putln (self->bio, ptr, len);
    }
  Py_RETURN_NONE;
}

PYBIODEFF
BIO_Object_alloc (PyTypeObject *type, PyObject *args, PyObject *kwds) {
    BIO_Object *self;
    long cap;
    if (PyTuple_Size(args) == 0 || /* no arg */
        !PyArg_ParseTuple(args, "l", &cap) || /* invalid arg */
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
  return BIO_Object_alloc (&BIO_Type, args, NULL);
}

static void
BIO_Object_free (BIO_Object *self)
{
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
  if (PyType_Ready(&BIO_Type) < 0)
    return NULL;

  Py_INCREF(&BIO_Type);
  PyModule_AddObject(module, "bio", (PyObject *)&BIO_Type);

  return module;
}
