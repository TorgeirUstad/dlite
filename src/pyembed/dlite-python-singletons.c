#include "dlite-macros.h"
#include "dlite-python-singletons.h"


/*
  Returns a pointer to __main__.__dict__ of the embedded interpreater
  or NULL on error.
*/
PyObject *dlite_python_maindict(void)
{
  PyObject *main_module, *main_dict=NULL;

  if (!(main_module = PyImport_AddModule("__main__")))
    FAIL("cannot load the embedded Python __main__ module");
  if (!(main_dict = PyModule_GetDict(main_module)))
    FAIL("cannot access __dict__ of the embedded Python __main__ module");
 fail:
  return main_dict;
}


/*
  Creates a new class in __main__ and returns it.  If the class
  already exists, it is returned immediately.

  The class is created as executing the following python code in the
  __main__ scope `class <classname>: pass`.

  Returns NULL on error.
 */
PyObject *dlite_python_mainclass(const char *classname)
{
  PyObject *main_dict, *class;
  char initcode[96];

  if (!(main_dict = dlite_python_maindict())) goto fail;

  /* Return newclass if it is already in __main__.__dict__ */
  if ((class = PyDict_GetItemString(main_dict, classname)))
    return class;

  /* Create and inject new class into the __main__ module */
  if (snprintf(initcode, sizeof(initcode), "class %s: pass\n", classname) < 0)
    FAIL("failure to create initialisation code for embedded Python "
         "__main__ module");
  if (PyRun_SimpleString(initcode))
    FAIL("failure when running embedded Python __main__ module "
         "initialisation code");
  if ((class = PyDict_GetItemString(main_dict, classname)))
    return class;

 fail:
  return NULL;
}


/* Returns the base class for storage plugins. */
PyObject *dlite_python_storage_base(void)
{
  return dlite_python_mainclass("DLiteStorageBase");
}


/* Returns the base class for mapping plugins. */
PyObject *dlite_python_mapping_base(void)
{
  return dlite_python_mainclass("DLiteMappingBase");
}
