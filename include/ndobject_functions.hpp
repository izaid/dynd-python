//
// Copyright (C) 2011-12, Dynamic NDArray Developers
// BSD 2-Clause License, see LICENSE.txt
//
// This header defines some wrapping functions to
// access various ndobject parameters
//

#ifndef _DYND__NDARRAY_FUNCTIONS_HPP_
#define _DYND__NDARRAY_FUNCTIONS_HPP_

#include "Python.h"

#include <sstream>

#include <dynd/ndobject.hpp>

#include "ndobject_from_py.hpp"
#include "ndobject_as_py.hpp"

namespace pydynd {

/**
 * This is the typeobject and struct of w_ndobject from Cython.
 */
extern PyTypeObject *WNDArray_Type;
inline bool WNDArray_CheckExact(PyObject *obj) {
    return Py_TYPE(obj) == WNDArray_Type;
}
inline bool WNDArray_Check(PyObject *obj) {
    return PyObject_TypeCheck(obj, WNDArray_Type);
}
struct WNDArray {
  PyObject_HEAD;
  // This is ndobject_placement_wrapper in Cython-land
  dynd::ndobject v;
};
void init_w_ndobject_typeobject(PyObject *type);

inline void ndobject_init_from_pyobject(dynd::ndobject& n, PyObject* obj)
{
    n = ndobject_from_py(obj);
}
dynd::ndobject ndobject_vals(const dynd::ndobject& n);
dynd::ndobject ndobject_eval_copy(const dynd::ndobject& n, PyObject* access_flags, const dynd::eval::eval_context *ectx = &dynd::eval::default_eval_context);

inline dynd::ndobject ndobject_add(const dynd::ndobject& lhs, const dynd::ndobject& rhs)
{
    throw std::runtime_error("TODO: ndobject_add");
//    return lhs + rhs;
}

inline dynd::ndobject ndobject_subtract(const dynd::ndobject& lhs, const dynd::ndobject& rhs)
{
    throw std::runtime_error("TODO: ndobject_subtract");
//    return lhs - rhs;
}

inline dynd::ndobject ndobject_multiply(const dynd::ndobject& lhs, const dynd::ndobject& rhs)
{
    throw std::runtime_error("TODO: ndobject_multiply");
//    return lhs * rhs;
}

inline dynd::ndobject ndobject_divide(const dynd::ndobject& lhs, const dynd::ndobject& rhs)
{
    throw std::runtime_error("TODO: ndobject_divide");
//    return lhs / rhs;
}

inline std::string ndobject_str(const dynd::ndobject& n)
{
    std::stringstream ss;
    ss << n;
    return ss.str();
}

inline std::string ndobject_repr(const dynd::ndobject& n)
{
    std::stringstream ss;
    ss << "nd." << n;
    return ss.str();
}

inline std::string ndobject_debug_dump(const dynd::ndobject& n)
{
    std::stringstream ss;
    n.debug_dump(ss);
    return ss.str();
}

dynd::ndobject ndobject_cast_scalars(const dynd::ndobject& n, const dynd::dtype& dt, PyObject *assign_error_obj);

PyObject *ndobject_get_shape(const dynd::ndobject& n);

PyObject *ndobject_get_strides(const dynd::ndobject& n);

/**
 * Implementation of __getitem__ for the wrapped ndobject object.
 */
dynd::ndobject ndobject_getitem(const dynd::ndobject& n, PyObject *subscript);

/**
 * Implementation of nd.arange().
 */
dynd::ndobject ndobject_arange(PyObject *start, PyObject *stop, PyObject *step);

/**
 * Implementation of nd.linspace().
 */
dynd::ndobject ndobject_linspace(PyObject *start, PyObject *stop, PyObject *count);

/**
 * Implementation of nd.groupby().
 */
dynd::ndobject ndobject_groupby(const dynd::ndobject& data, const dynd::ndobject& by, const dynd::dtype& groups);

} // namespace pydynd

#endif // _DYND__NDARRAY_FUNCTIONS_HPP_