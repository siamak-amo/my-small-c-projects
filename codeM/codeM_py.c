/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>

  codeM is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  codeM is distributed in the hope that it will be useful,
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
 *  Compilation:
 *    replace xx with your python version
 *    cc -ggdb -Wall -Wextra -Werror \
 *       -shared -fPIC codeM_py.c -o codeM.so \
 *       $(pkg-config --cflags python-3.xx)
 *
 *    options:
 *      define `-D PY_CODEM_DEBUG` to print some debug
 *      information about reference count of globally
 *      handled python objects
 *
 *  Usage Example:
 *    ```python
 *    import codeM
 *    # help(codeM)
 *
 *    print( codeM.mkrand().decode() )
 *    print( codeM.mkvalid('6665554443').decode() )
 *
 *    # change the default random number generator
 *    def rnd(dt)->int:
 *        return int( dt*0x42 + 0x666 )
 *
 *    if codeM.set_srand(lambda: rnd(666)) == None:
 *        print("Error!")
 *
 *    # set to default
 *    codeM.set_srand(None)
 *    ```
 **/
#include <time.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define CODEM_IMPLEMENTATION
#define CODEM_FUZZY_SEARCH_CITYNAME
#include "codeM.c"

/* internal macros */
/**
 *  Allocates string buffer in the @res from the source @inp[.@len]
 *  @res:  the result PyObject pointer
 *  @inp:  should be `const char *`
 *         pass it NULL to initialize an empty buffer of length @len
 *  if you are handling the @res locally, use `py_DECREF` to free it
 **/
#define py_mkstrbuf_H(res, len, inp) \
  PyByteArray_AS_STRING ((res = PyByteArray_FromStringAndSize (inp, len)))

#ifdef PY_CODEM_DEBUG
/* debug macro, to run fun(obj) and print refcnt of @obj */
#  define pyd_fun(fun, obj) do {                                        \
  if (NULL == obj) {                                                    \
    printf ("[debug %s:%d] %s is NULL!\n", __func__, __LINE__, #obj);   \
  } else {                                                              \
    fun (obj);                                                          \
    printf ("[debug %s:%d] %s->ob_refcnt = %ld, after running %s\n",    \
            __func__, __LINE__, #obj, Py_REFCNT(obj), #fun);            \
  }} while (0)
/* debug macro, to print refcnt of @obj */
#  define pyd_refcnt(obj) if (NULL != obj) {                    \
    printf ("[debug %s:%d] %s->ob_refcnt = %ld\n",              \
            __func__, __LINE__, #obj, Py_REFCNT (obj)); }
#else
#  define pyd_fun(fun, obj) fun (obj)
#  define pyd_refcnt(obj)
#endif

/* wrappers for Py_xxxREF functions */
#define py_INCREF(obj) pyd_fun (Py_INCREF, obj)
#define py_DECREF(obj) pyd_fun (Py_DECREF, obj)

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
PYCODEMDEF py_rand2 (PyObject *self, PyObject *args);
PYCODEMDEF py_rand (PyObject *self, PyObject *args);
PYCODEMDEF py_rand_suffix (PyObject *self, PyObject *args);
PYCODEMDEF py_isvalid (PyObject *self, PyObject *args);
PYCODEMDEF py_validate (PyObject *self, PyObject *args);
PYCODEMDEF py_validate2 (PyObject *self, PyObject *args);
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
    "isvalid", py_isvalid,
    METH_VARARGS,
    "validate the input"
  },{
    "validate", py_validate,
    METH_VARARGS,
    "validate the input\n"
    "this wont check the city code part of the input"
  },{
    "validate2", py_validate2,
    METH_VARARGS,
    "validate the input after normalizing\n"
    "this wont check the city code part of the input"
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
  char *result_ptr = py_mkstrbuf_H (result, CODEM_LEN, NULL);

  codem_rand2 (result_ptr);

  return result;
}

PYCODEMDEF
py_rand (PyObject *self, PyObject *args)
{
  UNUSED (self);
  UNUSED (args);

  PyObject *result;
  char *result_ptr = py_mkstrbuf_H (result, CODEM_LEN, NULL);

  codem_rand (result_ptr);

  return result;
}

PYCODEMDEF
py_rand_suffix (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *suffix;
  size_t offset;

  if (!PyArg_ParseTuple (args, "s#", &suffix, &offset))
    Py_RETURN_NONE;
  if (offset > CODEM_LEN)
    offset = CODEM_LEN;

  PyObject *result;
  char *result_ptr = py_mkstrbuf_H (result, CODEM_LEN, suffix);

  codem_memnum (result_ptr, CODEM_LEN);
  codem_randp (result_ptr, offset);

  return result;
}

/**
 *  @return:
 *    on error   -> None
 *    otherwise  -> True for valid and False for invalid
 */
PYCODEMDEF
py_validate (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *code;
  size_t len;

  if (!PyArg_ParseTuple (args, "s#", &code, &len))
    Py_RETURN_NONE;

  if (len != CODEM_LEN)
    Py_RETURN_FALSE;

  if (codem_isvalidn (code))
    Py_RETURN_TRUE;
  else
    Py_RETURN_FALSE;
}

// @return:  same as py_validate
PYCODEMDEF
py_validate2 (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *code;
  char tmp[CODEM_BUF_LEN];
  size_t len;

  if (!PyArg_ParseTuple (args, "s#", &code, &len))
    Py_RETURN_NONE;

  /* copy to a temprary buffer with write permission */
  memcpy (tmp, code, CODEM_BUF_LEN);
  /* normalizing */
  codem_norm (tmp);

  if (codem_isvalidn (tmp))
    Py_RETURN_TRUE;
  else
    Py_RETURN_FALSE;
}

// @return:  same as py_validate
PYCODEMDEF
py_isvalid (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *code;
  size_t len;

  if (!PyArg_ParseTuple (args, "s#", &code, &len))
    Py_RETURN_NONE;

  if (len != CODEM_LEN)
    Py_RETURN_FALSE;

  int idx = codem_ccode_idx (code);
  if (idx < 0)
    Py_RETURN_FALSE;

  if (codem_isvalidn (code))
    Py_RETURN_TRUE;
  else
    Py_RETURN_FALSE;
}

/**
 *  @return:
 *    on error    -> None
 *    otherwise   -> bytearray of a valid codeM
 **/
PYCODEMDEF
py_mkvalid (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *code;
  size_t len;

  if (!PyArg_ParseTuple (args, "s#", &code, &len))
    Py_RETURN_NONE;

  PyObject *result;
  char *result_ptr = py_mkstrbuf_H (result, CODEM_LEN, code);

  codem_norm (result_ptr);
  codem_set_ctrl_digit (result_ptr);

  return result;
}

// @return:  bytearray of a valid codeM
PYCODEMDEF
py_rand_ccode (PyObject *self, PyObject *args)
{
  UNUSED (self);
  UNUSED (args);

  PyObject *result;
  char *result_ptr = py_mkstrbuf_H (result, CC_LEN, NULL);

  codem_rand_ccode (result_ptr);
  return result;
}

/**
 *  @return:
 *    on error / on failure    -> None
 *    on success               -> bytearray of name of a city
 *                                [UTF8 - None-ASCII]
 **/
PYCODEMDEF
py_cname_by_codem (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *code;
  size_t len;

  if (!PyArg_ParseTuple (args, "s#", &code, &len))
    Py_RETURN_NONE;

  /* codem's internal, should be used by codem_get_cname */
  int idx = codem_ccode_idx (code);
  if (idx < 0)
    Py_RETURN_NONE;

  const char *p = codem_get_cname (idx);
  return PyByteArray_FromStringAndSize (p, strlen (p));
}

// @return:  the same as py_cname_by_codem
PYCODEMDEF
py_cname_by_code (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *code;
  size_t len;

  if (!PyArg_ParseTuple (args, "s#", &code, &len))
    Py_RETURN_NONE;

  if (len != CC_LEN)
    Py_RETURN_NONE;

  /* codem's internal, should be used by codem_get_cname */
  int idx = codem_ccode_idx (code);
  if (idx < 0)
    Py_RETURN_NONE;

  const char *p = codem_get_cname (idx);
  return PyByteArray_FromStringAndSize (p, strlen (p));
}

/**
 *  @return:
 *    on error      -> None
 *    on failure    -> empty list
 *    otherwise     -> list of byte arrays of length 3
 *                     [ASCII numbers]
 **/
PYCODEMDEF
py_ccode_by_cname (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *name;
  size_t len;

  if (!PyArg_ParseTuple (args, "s#", &name, &len))
    Py_RETURN_NONE;

  int res = codem_cname_search (name);
  if (res < 0)
    return PyList_New (0); /* not found */

  /* array of codes, each of length CC_LEN */
  const char *res_codes = codem_ccode (res);

  Py_ssize_t res_len = strlen (res_codes) / CC_LEN; /* nonzero */
  PyObject *result = PyList_New (res_len);

  if (result == NULL)
    Py_RETURN_NONE; // error

  for (Py_ssize_t i = 0; i < res_len; res_codes += CC_LEN, ++i)
    {
      if (0 != PyList_SetItem (result, i,
           PyByteArray_FromStringAndSize (res_codes, CC_LEN)))
        {
          py_DECREF (result);
          /* unexpected internal python error */
          PyErr_SetString (PyExc_MemoryError, "Append to list error");
          return NULL;
        }
    }

  return result;
}

// @return:  same as py_cname_by_code
PYCODEMDEF
py_search_cname (PyObject *self, PyObject *args)
{
  UNUSED (self);

  const char *name;
  size_t len;

  if (!PyArg_ParseTuple (args, "s#", &name, &len))
    Py_RETURN_NONE;

  int res = codem_cname_search (name);
  if (res < 0)
    Py_RETURN_NONE;

  /* namy of the city in UTF8 format */
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
  pyd_refcnt (srand_fun);

  /**
   *  we must call Py_DECREF before runnign the
   *  `PyArg_ParseTuple` function, as it may override
   *  the `srand_fun` to NULL (on error) so we will lose
   *  the pointer to an allocated object causing memory leak
   **/
  if (NULL != srand_fun)
    {
      py_DECREF (srand_fun);
      srand_fun = NULL;
    }
  
  if (!PyArg_ParseTuple (arg, "O", &srand_fun))
    Py_RETURN_NONE;
  
  if (!PyFunction_Check (srand_fun))
      Py_RETURN_FALSE;
  else
    {
      /**
       *  setting the srand_fun
       *  we keep a reference of the `srand_fun` object locally
       *  it's important to increase it's reference count
       *  specially when the @arg is a lambda function
       *  otherwise calling `srand_fun` will cause SEGFAULT
       *  to unset the `srand_fun`, first release it by `py_decref`
       **/
      py_INCREF (srand_fun);
      Py_RETURN_TRUE;
    }
}

/**
 *  @return:
 *    on error    -> max size_t value
 *    otherwise   -> srand_fun()
 **/
static inline size_t
user_srand ()
{
  PyObject* rando = PyObject_CallNoArgs (srand_fun);
  if (NULL == rando)
    return -1;

  if (!PyLong_Check (rando))
    return -1;

  /* use `Mask`ed functions to ignore python overflow exceptions */
  return PyLong_AsUnsignedLongMask (rando);
}

static inline size_t
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
PyInit_codeM ()
{
  /* internal codeM initialization */
  codem_rand_init (ssrand);
  /* internal python C API initialization */
  return PyModule_Create (&codeM_def);
}
