/* This file is part of my-small-c-projects

  This program is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 *  file: codeM_py.c
 *  created on: 19 May 2024
 *
 *  Python C API extension for the codeM.c library
 *
 *  compilation:
 *    cc -Wall -Wextra -shared -fPIC codeM_py.c -o codeM.so
 *       -I /path/to/Python.h
 *    you can find the path of `Python.h` by running
 *    `sysconfig.get_paths()` in a python shell,
 *    normally it's located in `/usr/include/python3.xx`
 *
 *  Example:
 *    ```
 *      import codeM
 *      # help(codeM)
 *
 *      print( codeM.mkrand().decode() )
 *      print( codeM.mkvalid('6665554443').decode() )
 *
 *      # change the default random number generator
 *      def rnd(dt)->int:
 *          return int( dt*0x42 + 0x666 )
 *
 *      if codeM.set_srand(lambda: rnd(666)) != True:
 *          print("Error!")
 *
 *      # set to default
 *      codeM.set_srand(None)
 *    ```
 **/
#include <time.h>
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define CODEM_IMPLEMENTATION
#define CODEM_FUZZY_SEARCH_CITYNAME
#include "codeM.c"

/* internal macros */
#define py_mkbuf_H(res, len, inp) \
  PyByteArray_AS_STRING ((res = PyByteArray_FromStringAndSize(inp, len)))
/* to run Py_DECREF and set to NULL for ptr */
#define py_decref(ptr) do {                     \
    if (NULL != (ptr))                          \
      Py_DECREF (ptr);                          \
  } while (0)

#ifndef PYCODEMDEF
#  define PYCODEMDEF static PyObject *
#endif

/* noise for random number generator function */
static size_t noise = 0;

/* to change the default random number generator function */
static PyObject *srand_fun = NULL;

/* internal function definitions */
static size_t default_srand (void);
/* external PyMethod definitions */
PYCODEMDEF py_rand2(PyObject *self, PyObject *args);
PYCODEMDEF py_rand(PyObject *self, PyObject *args);
PYCODEMDEF py_rand_suffix (PyObject *self, PyObject *args);
PYCODEMDEF py_validate (PyObject *self, PyObject *args);
PYCODEMDEF py_mkvalid (PyObject *self, PyObject *args);
PYCODEMDEF py_rand_ccode (PyObject *self, PyObject *args);
PYCODEMDEF py_cname_by_codem (PyObject *self, PyObject *args);
PYCODEMDEF py_cname_by_code (PyObject *self, PyObject *args);
PYCODEMDEF py_ccode_by_cname (PyObject *self, PyObject *args);
PYCODEMDEF py_search_cname (PyObject *self, PyObject *args);
PYCODEMDEF py_set_srand (PyObject *self, PyObject *arg);

static struct PyMethodDef funs[] = {
  {
    "mkrand", py_rand2,
    METH_VARARGS,
    "create random codem"
  },{
    "rand", py_rand,
    METH_VARARGS,
    "like mkrand, but city code might be invalid"
  },{
    "rand_suffix", py_rand_suffix,
    METH_VARARGS,
    "random codem with suffix"
  },{
    "rand_ccode", py_rand_ccode,
    METH_VARARGS,
    "make random city code"
  },{
    "validate", py_validate,
    METH_VARARGS,
    "validate the input (not the city code)"
  },{
    "mkvalid", py_mkvalid,
    METH_VARARGS,
    "make the input valid"
  },{
    "cname_by_ccode", py_cname_by_code,
    METH_VARARGS,
    "get city name by city code"
  },{
    "cname_by_codem", py_cname_by_codem,
    METH_VARARGS,
    "get city name of a codem"
  },{
    "ccode_by_cname", py_ccode_by_cname,
    METH_VARARGS,
    "get city code by city name"
  },{
    "search_cname", py_search_cname,
    METH_VARARGS,
    "search the city name"
  },{
    "set_srand", py_set_srand,
    METH_VARARGS,
    "set the random number generator function\n"
    "pass it None to use the default function\n"
    "expected signature:  def rand()->int: ..."
  },
  {NULL, NULL, 0, NULL}
};

/* the final python module definition */
struct PyModuleDef codeM_def = {
  .m_base = PyModuleDef_HEAD_INIT,
  .m_name = "codeM",
  .m_doc = "codeM.c library extension for python",
  .m_size = -1,
  .m_methods = funs,
  .m_traverse = NULL,
  .m_clear = NULL,
  .m_free = NULL
};


PYCODEMDEF
py_rand2 (PyObject *self, PyObject *args)
{
  UNUSED (self);
  UNUSED (args);

  PyObject *result;
  char *result_ptr = py_mkbuf_H (result, CODEM_LEN, NULL);

  codem_rand2 (result_ptr);

  return result;
}

PYCODEMDEF
py_rand (PyObject *self, PyObject *args)
{
  UNUSED (self);
  UNUSED (args);

  PyObject *result;
  char *result_ptr = py_mkbuf_H (result, CODEM_LEN, NULL);

  codem_rand (result_ptr);

  return result;
}

PYCODEMDEF
py_rand_suffix (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *suffix;
  size_t offset;

  if (!PyArg_ParseTuple(args, "s#", &suffix, &offset))
    Py_RETURN_NONE;
  if (offset > CODEM_LEN)
    offset = CODEM_LEN;

  PyObject *result;
  char *result_ptr = py_mkbuf_H (result, CODEM_LEN, suffix);

  codem_rands (result_ptr, offset);

  return result;
}

PYCODEMDEF
py_validate (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *code;
  size_t len;

  if (!PyArg_ParseTuple(args, "s#", &code, &len))
    Py_RETURN_NONE;

  if (len != CODEM_LEN)
    Py_RETURN_FALSE;

  return (codem_isvalidn (code)) ? Py_NewRef(Py_True)
    : Py_NewRef(Py_False);
}

PYCODEMDEF
py_mkvalid (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *code;
  size_t len;

  if (!PyArg_ParseTuple(args, "s#", &code, &len))
    Py_RETURN_NONE;

  PyObject *result;
  char *result_ptr = py_mkbuf_H (result, CODEM_LEN, code);

  codem_norm (result_ptr);
  codem_set_ctrl_digit (result_ptr);

  return result;
}

PYCODEMDEF
py_rand_ccode (PyObject *self, PyObject *args)
{
  UNUSED (self);
  UNUSED (args);

  PyObject *result;
  char *result_ptr = py_mkbuf_H (result, CC_LEN, NULL);

  codem_rand_ccode (result_ptr);
  return result;
}

PYCODEMDEF
py_cname_by_codem (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *code;
  size_t len;

  if (!PyArg_ParseTuple(args, "s#", &code, &len))
    Py_RETURN_NONE;

  int idx = codem_ccode_idx (code);
  if (idx < 0)
    Py_RETURN_NONE;

  const char *p = codem_get_cname (idx);
  return PyByteArray_FromStringAndSize (p, strlen (p));
}

PYCODEMDEF
py_cname_by_code (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *code;
  size_t len;

  if (!PyArg_ParseTuple(args, "s#", &code, &len))
    Py_RETURN_NONE;

  if (len != CC_LEN)
    Py_RETURN_NONE;

  int idx = codem_ccode_idx (code);
  if (idx < 0)
    Py_RETURN_NONE;

  const char *p = codem_get_cname (idx);
  return PyByteArray_FromStringAndSize (p, strlen (p));
}

PYCODEMDEF
py_ccode_by_cname (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *name;
  size_t len;

  if (!PyArg_ParseTuple(args, "s#", &name, &len))
    Py_RETURN_NONE;

  int res = codem_cname_search (name);
  if (res < 0)
    Py_RETURN_NONE;

  const char *res_codes = codem_ccode (res);
  Py_ssize_t res_len = strlen (res_codes) / CC_LEN; 

  PyObject *result = PyList_New (res_len);
  if (result == NULL)
    Py_RETURN_NONE;

  for (; *res_codes != '\0'; res_codes += CC_LEN)
    {
      if (0 != PyList_SetItem (result, --res_len,
           PyByteArray_FromStringAndSize (res_codes, CC_LEN)))
        {
          Py_DECREF (result);
          Py_RETURN_NONE;
        }
    }

  return result;
}

PYCODEMDEF
py_search_cname (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *name;
  size_t len;

  if (!PyArg_ParseTuple(args, "s#", &name, &len))
    Py_RETURN_NONE;

  int res = codem_cname_search (name);
  if (res < 0)
    Py_RETURN_NONE;

  const char *tmp = codem_cname_byidx (res);
  if (tmp == NULL)
    Py_RETURN_NONE;

  return PyByteArray_FromStringAndSize (tmp, strlen (tmp));
}

/**
 *  @return:
 *    on error                          -> None
 *    on setting a valid function       -> True
 *    on setting to default             -> False
 **/
PYCODEMDEF
py_set_srand (PyObject *self, PyObject *arg)
{
  UNUSED (self);

  if (!PyArg_ParseTuple(arg, "O", &srand_fun))
    {
      /* error, unset the srand_fun */
      py_decref (srand_fun);
      srand_fun = NULL;
      Py_RETURN_NONE;
    }

  if (!PyFunction_Check (srand_fun))
    {
      /* unset the srand_fun */
      py_decref (srand_fun);
      srand_fun = NULL;
      Py_RETURN_FALSE;
    }
  else
    {
      /**
       *  setting the srand_fun
       *  we are keeping a reference of `srand_fun` object locally
       *  it's important to increase it's reference count,
       *  specially when the @arg is a lambda function
       *  otherwise calling `srand_fun` will cause SEGFAULT
       *  to unset `srand_fun`, first release it by `py_decref`
       **/
      Py_INCREF (srand_fun);
      Py_RETURN_TRUE;
    }
}

/**
 *  @return:
 *    on error:  max size_t value
 *    otherwise: srand_fun()
 **/
static inline size_t
user_srand ()
{
  PyObject* rando = PyObject_CallNoArgs (srand_fun);
  if (NULL == rando)
    return -1;

  if (!PyLong_Check (rando))
    return -1;

  /* use `Mask`ed functions to ignore python `int` overflow */
  return PyLong_AsUnsignedLongMask (rando);
}

static size_t
default_srand ()
{
  size_t r = time (NULL) + noise++;

  for (int i=7; i>0; --i)
    {
      r *= 0x42;
      r += 0x666;
    }

  return r;
}

static size_t
ssrand ()
{
  if (NULL != srand_fun)
    return user_srand ();
  else
    return default_srand ();
}

PyMODINIT_FUNC
PyInit_codeM()
{
  /* internal codeM initialization */
  codem_rand_init (ssrand);

  return PyModule_Create(&codeM_def);
}
