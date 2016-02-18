#pragma once

#include <Python.h>

#include <dynd/array.hpp>
#include <dynd/callable.hpp>
#include <dynd/type.hpp>

#include "visibility.hpp"

namespace pydynd {

PYDYND_API dynd::ndt::type &type_to_cpp_ref(PyObject *);
PYDYND_API PyTypeObject *get_type_pytypeobject();
PYDYND_API PyObject *type_from_cpp(const dynd::ndt::type &);
PYDYND_API dynd::ndt::type dynd_ndt_as_cpp_type(PyObject *);
PYDYND_API dynd::ndt::type dynd_ndt_cpp_type_for(PyObject *);

PYDYND_API dynd::nd::array &array_to_cpp_ref(PyObject *);
PYDYND_API PyTypeObject *get_array_pytypeobject();
PYDYND_API PyObject *array_from_cpp(const dynd::nd::array &);

PYDYND_API dynd::nd::callable &callable_to_cpp_ref(PyObject *);
PYDYND_API PyTypeObject *get_callable_pytypeobject();
PYDYND_API PyObject *callable_from_cpp(const dynd::nd::callable &);

} // namespace pydynd