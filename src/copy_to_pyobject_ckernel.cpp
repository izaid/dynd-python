//
// Copyright (C) 2011-14 Mark Wiebe, DyND Developers
// BSD 2-Clause License, see LICENSE.txt
//

#include <Python.h>
#include <datetime.h>

#include "copy_to_pyobject_ckernel.hpp"
#include "utility_functions.hpp"
#include "type_functions.hpp"

#include <dynd/array.hpp>
#include <dynd/types/bytes_type.hpp>
#include <dynd/types/fixedbytes_type.hpp>
#include <dynd/types/string_type.hpp>
#include <dynd/types/date_type.hpp>
#include <dynd/types/time_type.hpp>
#include <dynd/types/datetime_type.hpp>
#include <dynd/types/option_type.hpp>
#include <dynd/kernels/assignment_kernels.hpp>

using namespace std;
using namespace dynd;
using namespace pydynd;

namespace {
// Initialize the pydatetime API
struct init_pydatetime {
    init_pydatetime() {
        PyDateTime_IMPORT;
    }
};
init_pydatetime pdt;

struct bool_ck : public kernels::unary_ck<bool_ck> {
  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = (*src != 0) ? Py_True : Py_False;
    Py_INCREF(*dst_obj);
  }
};

PyObject *pyint_from_int(int8_t v) {
#if PY_VERSION_HEX >= 0x03000000
  return PyLong_FromLong(v);
#else
  return PyInt_FromLong(v);
#endif
}

PyObject *pyint_from_int(uint8_t v) {
#if PY_VERSION_HEX >= 0x03000000
  return PyLong_FromLong(v);
#else
  return PyInt_FromLong(v);
#endif
}

PyObject *pyint_from_int(int16_t v) {
#if PY_VERSION_HEX >= 0x03000000
  return PyLong_FromLong(v);
#else
  return PyInt_FromLong(v);
#endif
}

PyObject *pyint_from_int(uint16_t v) {
#if PY_VERSION_HEX >= 0x03000000
  return PyLong_FromLong(v);
#else
  return PyInt_FromLong(v);
#endif
}

PyObject *pyint_from_int(int32_t v) {
#if PY_VERSION_HEX >= 0x03000000
  return PyLong_FromLong(v);
#else
  return PyInt_FromLong(v);
#endif
}

PyObject *pyint_from_int(uint32_t v) {
#if PY_VERSION_HEX >= 0x03000000
  return PyLong_FromUnsignedLong(v);
#else
  return PyInt_FromUnsignedLong(v);
#endif
}

#if SIZEOF_LONG == 8
PyObject *pyint_from_int(int64_t v) {
#if PY_VERSION_HEX >= 0x03000000
  return PyLong_FromLong(v);
#else
  return PyInt_FromLong(v);
#endif
}

PyObject *pyint_from_int(uint64_t v) {
#if PY_VERSION_HEX >= 0x03000000
  return PyLong_FromUnsignedLong(v);
#else
  return PyInt_FromUnsignedLong(v);
#endif
}
#else
PyObject *pyint_from_int(int64_t v) {
  return PyLong_FromLongLong(v);
}

PyObject *pyint_from_int(uint64_t v) {
  return PyLong_FromUnsignedLongLong(v);
}
#endif

PyObject *pyint_from_int(const dynd_uint128& val)
{
  if (val.m_hi == 0ULL) {
    return PyLong_FromUnsignedLongLong(val.m_lo);
  }
  // Use the pynumber methods to shift and or together the 64 bit parts
  pyobject_ownref hi(PyLong_FromUnsignedLongLong(val.m_hi));
  pyobject_ownref sixtyfour(PyLong_FromLong(64));
  pyobject_ownref hi_shifted(PyNumber_Lshift(hi.get(), sixtyfour));
  pyobject_ownref lo(PyLong_FromUnsignedLongLong(val.m_lo));
  return PyNumber_Or(hi_shifted.get(), lo.get());
}

PyObject *pyint_from_int(const dynd_int128& val)
{
  if (val.is_negative()) {
    if (val.m_hi == 0xffffffffffffffffULL &&
        (val.m_hi & 0x8000000000000000ULL) != 0) {
      return PyLong_FromLongLong(static_cast<int64_t>(val.m_lo));
    }
    pyobject_ownref absval(pyint_from_int(static_cast<dynd_uint128>(-val)));
    return PyNumber_Negative(absval.get());
  } else {
    return pyint_from_int(static_cast<dynd_uint128>(val));
  }
}



template <class T>
struct int_ck : public kernels::unary_ck<int_ck<T> > {
  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    *dst_obj = pyint_from_int(*reinterpret_cast<const T *>(src));
  }
};

template<class T>
struct float_ck : public kernels::unary_ck<float_ck<T> > {
  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    *dst_obj = PyFloat_FromDouble(*reinterpret_cast<const T *>(src));
  }
};

template<class T>
struct complex_float_ck : public kernels::unary_ck<complex_float_ck<T> > {
  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    const dynd_complex<T> *val = reinterpret_cast<const dynd_complex<T> *>(src);
    *dst_obj = PyComplex_FromDoubles(val->real(), val->imag());
  }
};

struct bytes_ck : public kernels::unary_ck<bytes_ck> {
  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    const bytes_type_data *bd = reinterpret_cast<const bytes_type_data *>(src);
    *dst_obj = PyBytes_FromStringAndSize(bd->begin, bd->end - bd->begin);
  }
};

struct fixedbytes_ck : public kernels::unary_ck<fixedbytes_ck> {
  intptr_t m_data_size;

  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    *dst_obj = PyBytes_FromStringAndSize(src, m_data_size);
  }
};

struct char_ck : public kernels::unary_ck<char_ck> {
  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    *dst_obj = PyUnicode_DecodeUTF32(src, 4, NULL, NULL);
  }
};

struct string_ascii_ck : public kernels::unary_ck<string_ascii_ck> {
  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    const string_type_data *sd =
        reinterpret_cast<const string_type_data *>(src);
    *dst_obj = PyUnicode_DecodeASCII(sd->begin, sd->end - sd->begin, NULL);
  }
};

struct string_utf8_ck : public kernels::unary_ck<string_utf8_ck> {
  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    const string_type_data *sd =
        reinterpret_cast<const string_type_data *>(src);
    *dst_obj = PyUnicode_DecodeUTF8(sd->begin, sd->end - sd->begin, NULL);
  }
};

struct string_utf16_ck : public kernels::unary_ck<string_utf16_ck> {
  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    const string_type_data *sd =
        reinterpret_cast<const string_type_data *>(src);
    *dst_obj =
        PyUnicode_DecodeUTF16(sd->begin, sd->end - sd->begin, NULL, NULL);
  }
};

struct string_utf32_ck : public kernels::unary_ck<string_utf32_ck> {
  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    const string_type_data *sd =
        reinterpret_cast<const string_type_data *>(src);
    *dst_obj =
        PyUnicode_DecodeUTF32(sd->begin, sd->end - sd->begin, NULL, NULL);
  }
};

struct fixedstring_ascii_ck : public kernels::unary_ck<fixedstring_ascii_ck> {
  intptr_t m_data_size;

  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    intptr_t size = find(src, src + m_data_size, 0) - src;
    *dst_obj = PyUnicode_DecodeASCII(src, size, NULL);
  }
};

struct fixedstring_utf8_ck : public kernels::unary_ck<fixedstring_utf8_ck> {
  intptr_t m_data_size;

  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    intptr_t size = find(src, src + m_data_size, 0) - src;
    *dst_obj = PyUnicode_DecodeUTF8(src, size, NULL);
  }
};

struct fixedstring_utf16_ck : public kernels::unary_ck<fixedstring_utf16_ck> {
  intptr_t m_data_size;

  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    const uint16_t *char16_src = reinterpret_cast<const uint16_t *>(src);
    intptr_t size =
        find(char16_src, char16_src + (m_data_size >> 1), 0) - char16_src;
    *dst_obj = PyUnicode_DecodeUTF16(src, size * 2, NULL, NULL);
  }
};

struct fixedstring_utf32_ck : public kernels::unary_ck<fixedstring_utf32_ck> {
  intptr_t m_data_size;

  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    const uint32_t *char32_src = reinterpret_cast<const uint32_t *>(src);
    intptr_t size =
        find(char32_src, char32_src + (m_data_size >> 2), 0) - char32_src;
    *dst_obj = PyUnicode_DecodeUTF32(src, size * 4, NULL, NULL);
  }
};

struct date_ck : public kernels::unary_ck<date_ck> {
  ndt::type m_src_tp;
  const char *m_src_arrmeta;

  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    const date_type *dd = m_src_tp.tcast<date_type>();
    date_ymd ymd = dd->get_ymd(m_src_arrmeta, src);
    *dst_obj = PyDate_FromDate(ymd.year, ymd.month, ymd.day);
  }
};

struct time_ck : public kernels::unary_ck<time_ck> {
  ndt::type m_src_tp;
  const char *m_src_arrmeta;

  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    const time_type *tt = m_src_tp.tcast<time_type>();
    time_hmst hmst = tt->get_time(m_src_arrmeta, src);
    *dst_obj = PyTime_FromTime(hmst.hour, hmst.minute, hmst.second,
                               hmst.tick / DYND_TICKS_PER_MICROSECOND);
  }
};

struct datetime_ck : public kernels::unary_ck<datetime_ck> {
  ndt::type m_src_tp;
  const char *m_src_arrmeta;

  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    const datetime_type *dd = m_src_tp.tcast<datetime_type>();
    int32_t year, month, day, hour, minute, second, tick;
    dd->get_cal(m_src_arrmeta, src, year, month, day, hour, minute, second,
                tick);
    int32_t usecond = tick / 10;
    *dst_obj = PyDateTime_FromDateAndTime(year, month, day, hour, minute,
                                          second, usecond);
  }
};

struct type_ck : public kernels::unary_ck<type_ck> {
  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    ndt::type tp(reinterpret_cast<const type_type_data *>(src)->tp, true);
    *dst_obj = wrap_ndt_type(DYND_MOVE(tp));
  }
};

// TODO: Should make a more efficient strided kernel function
struct option_ck : public kernels::unary_ck<option_ck> {
  intptr_t m_copy_value_offset;

  inline void single(char *dst, const char *src)
  {
    PyObject **dst_obj = reinterpret_cast<PyObject **>(dst);
    Py_XDECREF(*dst_obj);
    *dst_obj = NULL;
    ckernel_prefix *is_avail = get_child_ckernel();
    expr_single_t is_avail_fn = is_avail->get_function<expr_single_t>();
    ckernel_prefix *copy_value = get_child_ckernel(m_copy_value_offset);
    expr_single_t copy_value_fn = is_avail->get_function<expr_single_t>();
    char value_is_avail = 0;
    is_avail_fn(&value_is_avail, &src, is_avail);
    if (value_is_avail != 0) {
      copy_value_fn(dst, &src, copy_value);
    } else {
      *dst_obj = Py_None;
      Py_INCREF(*dst_obj);
    }
  }

  inline void destruct_children()
  {
    get_child_ckernel()->destroy();
    base.destroy_child_ckernel(m_copy_value_offset);
  }
};

} // anonymous namespace

intptr_t pydynd::make_copy_to_pyobject_kernel(
    dynd::ckernel_builder *ckb, intptr_t ckb_offset,
    const dynd::ndt::type &src_tp, const char *src_arrmeta,
    dynd::kernel_request_t kernreq, const dynd::eval::eval_context *ectx)
{
  switch (src_tp.get_type_id()) {
  case bool_type_id:
    bool_ck::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case int8_type_id:
    int_ck<int8_t>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case int16_type_id:
    int_ck<int16_t>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case int32_type_id:
    int_ck<int32_t>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case int64_type_id:
    int_ck<int64_t>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case int128_type_id:
    int_ck<dynd_int128>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case uint8_type_id:
    int_ck<uint8_t>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case uint16_type_id:
    int_ck<uint16_t>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case uint32_type_id:
    int_ck<uint32_t>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case uint64_type_id:
    int_ck<uint64_t>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case uint128_type_id:
    int_ck<dynd_uint128>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case float16_type_id:
    float_ck<dynd_float16>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case float32_type_id:
    float_ck<float>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case float64_type_id:
    float_ck<double>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case complex_float32_type_id:
    complex_float_ck<float>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case complex_float64_type_id:
    complex_float_ck<double>::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case bytes_type_id:
    bytes_ck::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case fixedbytes_type_id: {
    fixedbytes_ck *self = fixedbytes_ck::create_leaf(ckb, kernreq, ckb_offset);
    self->m_data_size = src_tp.tcast<fixedbytes_type>()->get_data_size();
    return ckb_offset;
  }
  case char_type_id:
    char_ck::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  case string_type_id:
    switch (src_tp.tcast<base_string_type>()->get_encoding()) {
    case string_encoding_ascii:
      string_ascii_ck::create_leaf(ckb, kernreq, ckb_offset);
      return ckb_offset;
    case string_encoding_utf_8:
      string_utf8_ck::create_leaf(ckb, kernreq, ckb_offset);
      return ckb_offset;
    case string_encoding_ucs_2:
    case string_encoding_utf_16:
      string_utf16_ck::create_leaf(ckb, kernreq, ckb_offset);
      return ckb_offset;
    case string_encoding_utf_32:
      string_utf32_ck::create_leaf(ckb, kernreq, ckb_offset);
      return ckb_offset;
    default:
      break;
    }
    break;
  case fixedstring_type_id:
    switch (src_tp.tcast<base_string_type>()->get_encoding()) {
    case string_encoding_ascii: {
      fixedstring_ascii_ck *self =
          fixedstring_ascii_ck::create_leaf(ckb, kernreq, ckb_offset);
      self->m_data_size = src_tp.get_data_size();
      return ckb_offset;
    }
    case string_encoding_utf_8: {
      fixedstring_utf8_ck *self =
          fixedstring_utf8_ck::create_leaf(ckb, kernreq, ckb_offset);
      self->m_data_size = src_tp.get_data_size();
      return ckb_offset;
    }
    case string_encoding_ucs_2:
    case string_encoding_utf_16: {
      fixedstring_utf16_ck *self =
          fixedstring_utf16_ck::create_leaf(ckb, kernreq, ckb_offset);
      self->m_data_size = src_tp.get_data_size();
      return ckb_offset;
    }
    case string_encoding_utf_32: {
      fixedstring_utf32_ck *self =
          fixedstring_utf32_ck::create_leaf(ckb, kernreq, ckb_offset);
      self->m_data_size = src_tp.get_data_size();
      return ckb_offset;
    }
    default:
      break;
    }
    break;
  case date_type_id: {
    date_ck *self = date_ck::create_leaf(ckb, kernreq, ckb_offset);
    self->m_src_tp = src_tp;
    self->m_src_arrmeta = src_arrmeta;
    return ckb_offset;
  }
  case time_type_id: {
    time_ck *self = time_ck::create_leaf(ckb, kernreq, ckb_offset);
    self->m_src_tp = src_tp;
    self->m_src_arrmeta = src_arrmeta;
    return ckb_offset;
  }
  case datetime_type_id: {
    datetime_ck *self = datetime_ck::create_leaf(ckb, kernreq, ckb_offset);
    self->m_src_tp = src_tp;
    self->m_src_arrmeta = src_arrmeta;
    return ckb_offset;
  }
  case type_type_id: {
    type_ck::create_leaf(ckb, kernreq, ckb_offset);
    return ckb_offset;
  }
  case option_type_id: {
    intptr_t root_ckb_offset = ckb_offset;
    option_ck *self = option_ck::create(ckb, kernreq, ckb_offset);
    const arrfunc_type_data *is_avail_af =
        src_tp.tcast<option_type>()->get_is_avail_arrfunc();
    ckb_offset = is_avail_af->instantiate(
        is_avail_af, ckb, ckb_offset, ndt::make_type<dynd_bool>(), NULL,
        &src_tp, &src_arrmeta, kernel_request_single, ectx);
    ckb->ensure_capacity(ckb_offset);
    self = ckb->get_at<option_ck>(root_ckb_offset);
    self->m_copy_value_offset = ckb_offset - root_ckb_offset;
    ckb_offset = make_copy_to_pyobject_kernel(
        ckb, ckb_offset, src_tp.tcast<option_type>()->get_value_type(),
        src_arrmeta, kernel_request_single, ectx);
    return ckb_offset;
  }
  default:
    break;
  }

  stringstream ss;
  ss << "Unable to copy dynd value with type " << src_tp
     << " to a Python object";
  throw invalid_argument(ss.str());
}
