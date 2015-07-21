from ..config cimport translate_exception
from .array cimport array

cdef extern from "arrfunc_functions.hpp" namespace "pydynd":
    void init_w_arrfunc_typeobject(object)
    object arrfunc_call(object, object, object, object) except +translate_exception

cdef class arrfunc(array):
    pass