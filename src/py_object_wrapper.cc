
#include "py_object_wrapper.h"
#include "utils.h"

Persistent<FunctionTemplate> PyObjectWrapper::py_function_template;

void PyObjectWrapper::Initialize() {
    HandleScope scope;
    Local<FunctionTemplate> fn_tpl = FunctionTemplate::New();
    Local<ObjectTemplate> proto = fn_tpl->PrototypeTemplate();
    Local<ObjectTemplate> obj_tpl = fn_tpl->InstanceTemplate();

    obj_tpl->SetInternalFieldCount(1);

    // this has first priority. see if the properties already exist on the python object
    obj_tpl->SetNamedPropertyHandler(Get, Set);

    // If we're calling `toString`, delegate to our version of ToString
    proto->SetAccessor(String::NewSymbol("toString"), ToStringAccessor); 

    // likewise for valueOf
    obj_tpl->SetAccessor(String::NewSymbol("valueOf"), ValueOfAccessor); 

    // Python objects can be called as functions.
    obj_tpl->SetCallAsFunctionHandler(Call, Handle<Value>());

    py_function_template = Persistent<FunctionTemplate>::New(fn_tpl);
}

Handle<Value> PyObjectWrapper::New(PyObject* obj) {
    HandleScope scope;
    Local<Value> jsVal;

    // undefined
    if(obj == Py_None) {
        jsVal = Local<Value>::New(Undefined());
    }
    // double
    else if(PyFloat_CheckExact(obj)) {
        double d = PyFloat_AsDouble(obj);
        jsVal = Local<Value>::New(Number::New(d));
    }
    // integer (can be 64b)
    else if(PyInt_CheckExact(obj)) {
        long i = PyInt_AsLong(obj);
        jsVal = Local<Value>::New(Number::New((double) i));
    }
    // string
    else if(PyString_CheckExact(obj)) {
        char *str = PyString_AsString(obj);
        if(str) {
            jsVal = Local<Value>::New(String::New(str));
        }
    }
    else if(PyBool_Check(obj)) {
        int b = PyObject_IsTrue(obj);
        if(b != -1) {
            jsVal = Local<Value>::New(Boolean::New(b));
        }
    }

    if(PyErr_Occurred()) {
        Py_XDECREF(obj);
        return ThrowPythonException();
    }
    
    if(jsVal.IsEmpty()) {
        Local<Object> jsObj = py_function_template->GetFunction()->NewInstance();
        PyObjectWrapper* wrapper = new PyObjectWrapper(obj);
        wrapper->Wrap(jsObj);
        jsVal = Local<Value>::New(jsObj);
    }
    else {
        Py_XDECREF(obj);
    }

    return scope.Close(jsVal);
}

