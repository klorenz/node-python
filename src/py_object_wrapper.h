
#ifndef PY_OBJECT_WRAPPER_H
#define PY_OBJECT_WRAPPER_H

#include <string>

#include <node.h>
#include <Python.h>

#include "utils.h"

using namespace v8;
using std::string;

class PyObjectWrapper : public node::ObjectWrap {
    PyObject* mPyObject;
    public:
        static Persistent<FunctionTemplate> py_function_template;
        PyObjectWrapper(PyObject* obj) : node::ObjectWrap(), mPyObject(obj) {};
        virtual ~PyObjectWrapper() {
            Py_XDECREF(mPyObject);
            mPyObject = NULL;
        }

        static void Initialize();

        static Handle<Value> New(PyObject* obj);
        static Handle<Value> New(const Arguments& args);

        static Handle<Value>
        Get(Local<String> key, const AccessorInfo& info) {
            // returning an empty Handle<Value> object signals V8 that we didn't
            // find the property here, and we should check the "NamedAccessor" functions
            HandleScope scope;
            PyObjectWrapper* wrapper = ObjectWrap::Unwrap<PyObjectWrapper>(info.Holder());
            String::Utf8Value utf8_key(key);
            string value(*utf8_key);
            PyObject* result = wrapper->InstanceGet(value);
            if(result) {
                return PyObjectWrapper::New(result);
            }
            return Handle<Value>();
        }

        static Handle<Value>
        Set(Local<String> key, Local<Value> value, const AccessorInfo& info) {
            // we don't know what to do.
            return Undefined();
        };

        static Handle<Value>
        CallAccessor(Local<String> property, const AccessorInfo& info) {
            HandleScope scope;
            Local<FunctionTemplate> func = FunctionTemplate::New(Call);
            return scope.Close(func->GetFunction());
        };

        static Handle<Value>
        ToStringAccessor(Local<String> property, const AccessorInfo& info) {
            HandleScope scope;
            Local<FunctionTemplate> func = FunctionTemplate::New(ToString);
            return scope.Close(func->GetFunction());
        };

        static Handle<Value>
        ValueOfAccessor(Local<String> property, const AccessorInfo& info) {
            HandleScope scope;
            Local<FunctionTemplate> func = FunctionTemplate::New(ValueOf);
            return scope.Close(func->GetFunction());
        }

        static Handle<Value>
        Call(const Arguments& args) {
            HandleScope scope;
            //Local<Object> this_object = args.This(); 
            PyObjectWrapper* pyobjwrap = ObjectWrap::Unwrap<PyObjectWrapper>(args.This());
            Handle<Value> result = pyobjwrap->InstanceCall(args);
            return scope.Close(result);
        }

        static Handle<Value>
        ToString(const Arguments& args) {
            HandleScope scope;
            //Local<Object> this_object = args.This(); 
            PyObjectWrapper* pyobjwrap = ObjectWrap::Unwrap<PyObjectWrapper>(args.This());
            Local<String> result = String::New(pyobjwrap->InstanceToString(args).c_str());
            return scope.Close(result);
        }

        static Handle<Value>
        ValueOf(const Arguments& args) {
            HandleScope scope;
            //Local<Object> this_object = args.This(); 
            PyObjectWrapper* pyobjwrap = ObjectWrap::Unwrap<PyObjectWrapper>(args.This());
            PyObject* py_obj = pyobjwrap->InstanceGetPyObject();
            if(PyCallable_Check(py_obj)) {
                Local<FunctionTemplate> call = FunctionTemplate::New(Call);
                return scope.Close(call->GetFunction());
            } else if (PyNumber_Check(py_obj)) {
                long long_result = PyLong_AsLong(py_obj);
                return scope.Close(Integer::New(long_result));
            } else if (PySequence_Check(py_obj)) {
                int len = PySequence_Length(py_obj);
                Local<Array> array = Array::New(len);
                for(int i = 0; i < len; ++i) {
                    Handle<Object> jsobj = PyObjectWrapper::py_function_template->GetFunction()->NewInstance();
                    PyObject* py_obj_out = PySequence_GetItem(py_obj, i);
                    PyObjectWrapper* obj_out = new PyObjectWrapper(py_obj_out);
                    obj_out->Wrap(jsobj);
                    array->Set(i, jsobj);
                }
                return scope.Close(array);
            } else if (PyMapping_Check(py_obj)) {
                int len = PyMapping_Length(py_obj);
                Local<Object> object = Object::New();
                PyObject* keys = PyMapping_Keys(py_obj);
                PyObject* values = PyMapping_Values(py_obj);
                for(int i = 0; i < len; ++i) {
                    PyObject *key = PySequence_GetItem(keys, i),
                        *value = PySequence_GetItem(values, i),
                        *key_as_string = PyObject_Str(key);
                    char* cstr = PyString_AsString(key_as_string);

                    Local<Object> jsobj = PyObjectWrapper::py_function_template->GetFunction()->NewInstance();
                    PyObjectWrapper* obj_out = new PyObjectWrapper(value);
                    obj_out->Wrap(jsobj);
                    Py_XDECREF(key);
                    Py_XDECREF(key_as_string);
                }
                Py_XDECREF(keys);
                Py_XDECREF(values);
                return scope.Close(object);
            }
            return Undefined();
        }

        static PyObject*
        ConvertToPython(const Handle<Value>& value) {
            int len;
            HandleScope scope;
            if(value->IsString()) {
                return PyString_FromString(*String::Utf8Value(value->ToString()));
            } else if(value->IsNumber()) {
                return PyFloat_FromDouble(value->NumberValue());
            } else if(value->IsObject()) {
                Local<Object> obj = value->ToObject();
                if(!obj->FindInstanceInPrototypeChain(PyObjectWrapper::py_function_template).IsEmpty()) {
                    PyObjectWrapper* python_object = ObjectWrap::Unwrap<PyObjectWrapper>(value->ToObject());
                    PyObject* pyobj = python_object->InstanceGetPyObject();
                    return pyobj;
                } else {
                    Local<Array> property_names = obj->GetPropertyNames();
                    len = property_names->Length();
                    PyObject* py_dict = PyDict_New();
                    for(int i = 0; i < len; ++i) {
                        Local<String> str = property_names->Get(i)->ToString();
                        Local<Value> js_val = obj->Get(str);
                        PyDict_SetItemString(py_dict, *String::Utf8Value(str), ConvertToPython(js_val));
                    }
                    return py_dict;
                }
                return NULL;
            } else if(value->IsArray()) {
                Local<Array> array = Array::Cast(*value);
                len = array->Length();
                PyObject* py_list = PyList_New(len);
                for(int i = 0; i < len; ++i) {
                    Local<Value> js_val = array->Get(i);
                    PyList_SET_ITEM(py_list, i, ConvertToPython(js_val));
                }
                return py_list;
            } else if(value->IsUndefined()) {
                Py_INCREF(Py_None);
                return Py_None;
            }
            return NULL;
        }

        PyObject*
        InstanceGetPyObject() {
            return mPyObject;
        }

        Handle<Value> InstanceCall(const Arguments& args) {
            // for now, we don't do anything.
            HandleScope scope;
            int len = args.Length();
            PyObject* args_tuple = PyTuple_New(len);
            for(int i = 0; i < len; ++i) {
                PyObject* py_arg = ConvertToPython(args[i]);
                PyTuple_SET_ITEM(args_tuple, i, py_arg);
            }
            PyObject* result = PyObject_CallObject(mPyObject, args_tuple);
            Py_XDECREF(args_tuple);
            if(result) {
                return scope.Close(PyObjectWrapper::New(result));
            } else {
                return ThrowPythonException();
            }
        }

        string InstanceToString(const Arguments& args) {
            PyObject* as_string = PyObject_Str(mPyObject);
            string native_string(PyString_AsString(as_string));
            Py_XDECREF(as_string);
            return native_string;
        }

        PyObject* InstanceGet(const string& key) {
            if(PyObject_HasAttrString(mPyObject, key.c_str())) {
                PyObject* attribute = PyObject_GetAttrString(mPyObject, key.c_str());
                return attribute;
            }
            return (PyObject*)NULL;
        }
};


#endif
