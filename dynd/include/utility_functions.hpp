//
// Copyright (C) 2011-15 DyND Developers
// BSD 2-Clause License, see LICENSE.txt
//

#pragma once

#include <Python.h>

#include <sstream>
#include <stdexcept>
#include <string>

#include <dynd/type.hpp>

#include "visibility.hpp"

namespace dynd {
// Forward declaration
struct callable_type_data;
} // namespace dynd

namespace pydynd {

/**
 * A container class for managing the local lifetime of
 * PyObject *.
 *
 * Throws an exception if the object passed into the constructor
 * is NULL.
 */
class pyobject_ownref {
  PyObject *m_obj;

  // Non-copyable
  pyobject_ownref(const pyobject_ownref &);
  pyobject_ownref &operator=(const pyobject_ownref &);

public:
  inline pyobject_ownref() : m_obj(NULL) {}
  inline explicit pyobject_ownref(PyObject *obj) : m_obj(obj)
  {
    if (obj == NULL) {
      throw std::runtime_error("propagating a Python exception...");
    }
  }

  inline pyobject_ownref(PyObject *obj, bool inc_ref) : m_obj(obj)
  {
    if (obj == NULL) {
      throw std::runtime_error("propagating a Python exception...");
    }
    if (inc_ref) {
      Py_INCREF(m_obj);
    }
  }

  inline ~pyobject_ownref() { Py_XDECREF(m_obj); }

  inline PyObject **obj_addr() { return &m_obj; }

  /**
   * Resets the reference owned by this object to the one provided.
   * This steals a reference to the input parameter, 'obj'.
   *
   * \param obj  The reference to replace the current one in this object.
   */
  inline void reset(PyObject *obj)
  {
    if (obj == NULL) {
      throw std::runtime_error("propagating a Python exception...");
    }
    Py_XDECREF(m_obj);
    m_obj = obj;
  }

  /**
   * Clears the owned reference to NULL.
   */
  inline void clear()
  {
    Py_XDECREF(m_obj);
    m_obj = NULL;
  }

  /** Returns a borrowed reference. */
  inline PyObject *get() const { return m_obj; }

  /** Returns a borrowed reference. */
  inline operator PyObject *() { return m_obj; }

  /**
   * Returns the reference owned by this object,
   * use it like "return obj.release()". After the
   * call, this object contains NULL.
   */
  inline PyObject *release()
  {
    PyObject *result = m_obj;
    m_obj = NULL;
    return result;
  }
};

class PyGILState_RAII {
  PyGILState_STATE m_gstate;

  PyGILState_RAII(const PyGILState_RAII &);
  PyGILState_RAII &operator=(const PyGILState_RAII &);

public:
  inline PyGILState_RAII() { m_gstate = PyGILState_Ensure(); }

  inline ~PyGILState_RAII() { PyGILState_Release(m_gstate); }
};

/**
 * Function which casts the parameter to
 * a PyObject pointer and calls Py_XDECREF on it.
 */
 inline void py_decref_function(void *obj)
 {
   // Because dynd in general is intended to do things multi-threaded
   // (eventually),
   // the decref function needs to be threadsafe. The way to do that is to ensure
   // we're holding the GIL. This is slower than normal DECREF, but because the
   // reference count isn't an atomic variable, this appears to be the best we
   // can do.
   if (obj != NULL) {
     PyGILState_STATE gstate;
     gstate = PyGILState_Ensure();

     Py_DECREF((PyObject *)obj);

     PyGILState_Release(gstate);
   }
 }

inline intptr_t pyobject_as_index(PyObject *index)
{
  pyobject_ownref start_obj(PyNumber_Index(index));
  intptr_t result;
  if (PyLong_Check(start_obj.get())) {
    result = PyLong_AsSsize_t(start_obj.get());
#if PY_VERSION_HEX < 0x03000000
  } else if (PyInt_Check(start_obj.get())) {
    result = PyInt_AS_LONG(start_obj.get());
#endif
  } else {
    throw std::runtime_error(
        "Value returned from PyNumber_Index is not an int or long");
  }
  if (result == -1 && PyErr_Occurred()) {
    throw std::exception();
  }
  return result;
}

inline int pyobject_as_int_index(PyObject *index)
{
  pyobject_ownref start_obj(PyNumber_Index(index));
#if PY_VERSION_HEX >= 0x03000000
  long result = PyLong_AsLong(start_obj);
#else
  long result = PyInt_AsLong(start_obj);
#endif
  if (result == -1 && PyErr_Occurred()) {
    throw std::exception();
  }
  if (((unsigned long)result & 0xffffffffu) != (unsigned long)result) {
    throw std::overflow_error("overflow converting Python integer to 32-bit int");
  }
  return (int)result;
}

inline dynd::irange pyobject_as_irange(PyObject *index)
{
  if (PySlice_Check(index)) {
    dynd::irange result;
    PySliceObject *slice = (PySliceObject *)index;
    if (slice->start != Py_None) {
      result.set_start(pyobject_as_index(slice->start));
    }
    if (slice->stop != Py_None) {
      result.set_finish(pyobject_as_index(slice->stop));
    }
    if (slice->step != Py_None) {
      result.set_step(pyobject_as_index(slice->step));
    }
    return result;
  } else {
    return dynd::irange(pyobject_as_index(index));
  }
}

inline std::string pystring_as_string(PyObject *str)
{
  char *data = NULL;
  Py_ssize_t len = 0;
  if (PyUnicode_Check(str)) {
    pyobject_ownref utf8(PyUnicode_AsUTF8String(str));

#if PY_VERSION_HEX >= 0x03000000
    if (PyBytes_AsStringAndSize(utf8.get(), &data, &len) < 0) {
#else
    if (PyString_AsStringAndSize(utf8.get(), &data, &len) < 0) {
#endif
      throw std::runtime_error("Error getting string data");
    }
    return std::string(data, len);
#if PY_VERSION_HEX < 0x03000000
  } else if (PyString_Check(str)) {
    if (PyString_AsStringAndSize(str, &data, &len) < 0) {
      throw std::runtime_error("Error getting string data");
    }
    return std::string(data, len);
#endif
  } else {
    throw dynd::type_error("Cannot convert pyobject to string");
  }
}

inline PyObject *pystring_from_string(const char *str)
{
#if PY_VERSION_HEX >= 0x03000000
  return PyUnicode_FromString(str);
#else
  return PyString_FromString(str);
#endif
}
inline PyObject *pystring_from_string(const std::string &str)
{
#if PY_VERSION_HEX >= 0x03000000
  return PyUnicode_FromStringAndSize(str.data(), str.size());
#else
  return PyString_FromStringAndSize(str.data(), str.size());
#endif
}
inline std::string pyobject_repr(PyObject *obj)
{
  pyobject_ownref src_repr(PyObject_Repr(obj));
  return pystring_as_string(src_repr.get());
}

// Forward declare this. Include type functions header at the bottom.
inline dynd::ndt::type make__type_from_pyobject(PyObject *obj);

inline void pyobject_as_vector__type(PyObject *list_of_types,
                                 std::vector<dynd::ndt::type> &vector_of__types)
{
  Py_ssize_t size = PySequence_Size(list_of_types);
  vector_of__types.resize(size);
  for (Py_ssize_t i = 0; i < size; ++i) {
    pyobject_ownref item(PySequence_GetItem(list_of_types, i));
    vector_of__types[i] = make__type_from_pyobject(item.get());
  }
}

inline void pyobject_as_vector_string(PyObject *list_string,
                                     std::vector<std::string> &vector_string)
{
Py_ssize_t size = PySequence_Size(list_string);
vector_string.resize(size);
for (Py_ssize_t i = 0; i < size; ++i) {
  pyobject_ownref item(PySequence_GetItem(list_string, i));
  vector_string[i] = pystring_as_string(item.get());
}
}

inline void pyobject_as_vector_intp(PyObject *list_index,
                                     std::vector<intptr_t> &vector_intp,
                                     bool allow_int)
{
  if (allow_int) {
    // If permitted, convert an int into a size-1 list
    if (PyLong_Check(list_index)) {
      intptr_t v = PyLong_AsSsize_t(list_index);
      if (v == -1 && PyErr_Occurred()) {
        throw std::runtime_error("error converting int");
      }
      vector_intp.resize(1);
      vector_intp[0] = v;
      return;
    }
#if PY_VERSION_HEX < 0x03000000
    if (PyInt_Check(list_index)) {
      vector_intp.resize(1);
      vector_intp[0] = PyInt_AS_LONG(list_index);
      return;
    }
#endif
    if (PyIndex_Check(list_index)) {
      PyObject *idx_obj = PyNumber_Index(list_index);
      if (idx_obj != NULL) {
        intptr_t v = PyLong_AsSsize_t(idx_obj);
        Py_DECREF(idx_obj);
        if (v == -1 && PyErr_Occurred()) {
          throw std::exception();
        }
        vector_intp.resize(1);
        vector_intp[0] = v;
        return;
      } else if (PyErr_ExceptionMatches(PyExc_TypeError)) {
        // Swallow a type error, fall through to the sequence code
        PyErr_Clear();
      } else {
        // Propagate the error
        throw std::exception();
      }
    }
  }
  Py_ssize_t size = PySequence_Size(list_index);
  vector_intp.resize(size);
  for (Py_ssize_t i = 0; i < size; ++i) {
    pyobject_ownref item(PySequence_GetItem(list_index, i));
    vector_intp[i] = pyobject_as_index(item.get());
  }
}

void pyobject_as_vector_int(PyObject *list_int, std::vector<int> &vector_int);

/**
 * Same as PySequence_Size, but throws a C++
 * exception on error.
 */
inline Py_ssize_t pysequence_size(PyObject *seq)
{
  Py_ssize_t s = PySequence_Size(seq);
  if (s == -1 && PyErr_Occurred()) {
    throw std::exception();
  }
  return s;
}

/**
 * Same as PyDict_GetItemString, but throws a
 * C++ exception on error.
 */
inline PyObject *pydict_getitemstring(PyObject *dp, const char *key)
{
  PyObject *result = PyDict_GetItemString(dp, key);
  if (result == NULL) {
    throw std::exception();
  }
  return result;
}

PYDYND_API PyObject *intptr_array_as_tuple(size_t size, const intptr_t *array);

/**
 * Parses the axis argument, which may be either a single index
 * or a tuple of indices. They are converted into a boolean array
 * which is set to true whereever a reduction axis is provided.
 *
 * Returns the number of axes which were set.
 */
int pyarg_axis_argument(PyObject *axis, int ndim, dynd::bool1 *reduce_axes);

/**
 * Parses the error_mode argument. If it is None, returns
 * assign_error_default.
 */
dynd::assign_error_mode pyarg_error_mode(PyObject *error_mode_obj);
dynd::assign_error_mode pyarg_error_mode_no_default(PyObject *error_mode_obj);

PyObject *pyarg_error_mode_to_pystring(dynd::assign_error_mode errmode);

/**
 * Matches the input object against one of several
 * strings, returning the corresponding integer.
 */
int pyarg_strings_to_int(PyObject *obj, const char *argname, int default_value,
                         const char *string0, int value0);

/**
 * Matches the input object against one of several
 * strings, returning the corresponding integer.
 */
int pyarg_strings_to_int(PyObject *obj, const char *argname, int default_value,
                         const char *string0, int value0, const char *string1,
                         int value1);

int pyarg_strings_to_int(PyObject *obj, const char *argname, int default_value,
                         const char *string0, int value0, const char *string1,
                         int value1, const char *string2, int value2);

int pyarg_strings_to_int(PyObject *obj, const char *argname, int default_value,
                         const char *string0, int value0, const char *string1,
                         int value1, const char *string2, int value2,
                         const char *string3, int value3);

int pyarg_strings_to_int(PyObject *obj, const char *argname, int default_value,
                         const char *string0, int value0, const char *string1,
                         int value1, const char *string2, int value2,
                         const char *string3, int value3, const char *string4,
                         int value4);

bool pyarg_bool(PyObject *obj, const char *argname, bool default_value);

/**
 * Accepts "readwrite", "readonly", and "immutable".
 */
uint32_t pyarg_access_flags(PyObject *obj);
/**
 * Accepts "readwrite" and "immutable".
 */
uint32_t pyarg_creation_access_flags(PyObject *obj);

const dynd::callable_type_data *pyarg_callable_ro(PyObject *af,
                                                  const char *paramname);
dynd::callable_type_data *pyarg_callable_rw(PyObject *af,
                                            const char *paramname);

} // namespace pydynd

#include "type_functions.hpp"
