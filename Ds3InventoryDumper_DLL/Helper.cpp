#include "pch.h"
#include "Helper.h"

void PyData::LoadModule()
{
	if (m_module)
	{
		PyImport_ReloadModule(m_module);
	}
	else
	{
		PyObject* module_name = PyUnicode_FromString("myds3code");
		m_module = PyImport_Import(module_name);
		Py_DECREF(module_name);
	}

	if (m_module != nullptr)
	{
		m_calc_function = PyObject_GetAttrString(m_module, "calcular");
		std::cout << __FUNCTION__ << ":" << __LINE__ << " Loaded calcular from myds3code.py\n";
	}
}

void PyData::CallPyFunction(PyObject* head, PyObject* chest, PyObject* hands, PyObject* legs)
{
	PyObject* pArgs = PyTuple_New(4);

	PyTuple_SetItem(pArgs, 0, head);
	PyTuple_SetItem(pArgs, 1, chest);
	PyTuple_SetItem(pArgs, 2, hands);
	PyTuple_SetItem(pArgs, 3, legs);

	PyObject* val = PyObject_CallObject(m_calc_function, pArgs);
	Py_DECREF(pArgs);

	PyObject* pHead = PyTuple_GetItem(val, 0);
	*e_head = ItemFromPyDict(pHead);
	Py_DECREF(pHead);

	PyObject* pChest = PyTuple_GetItem(val, 1);
	*e_chest = ItemFromPyDict(pChest);
	Py_DECREF(pChest);

	PyObject* pHands = PyTuple_GetItem(val, 2);
	*e_gloves = ItemFromPyDict(pHands);
	Py_DECREF(pHands);

	PyObject* pLegs = PyTuple_GetItem(val, 3);
	*e_legs = ItemFromPyDict(pLegs);
	Py_DECREF(pLegs);
}

void PyData::InitPython(InitPythonData init)
{
	e_head = init.head;
	e_chest = init.chest;
	e_gloves = init.gloves;
	e_legs = init.legs;
	e_head_vec = init.head_vec;
	e_chest_vec = init.chest_vec;
	e_gloves_vec = init.gloves_vec;
	e_legs_vec = init.legs_vec;

	PyPreConfig preConfig{};
	PyPreConfig_InitPythonConfig(&preConfig);
	Py_PreInitialize(&preConfig);

	PyConfig pyConfig{};
	PyConfig_InitPythonConfig(&pyConfig);

	std::string pythonHome = init.module_path + "/python3";
	wchar_t wPyHome[MAX_PATH] = { 0 };
	MultiByteToWideChar(CP_UTF8, MB_USEGLYPHCHARS, pythonHome.c_str(), -1, wPyHome, sizeof(wPyHome));
	pyConfig.home = wPyHome;
	PyWideStringList_Append(&pyConfig.module_search_paths, wPyHome);

	wchar_t wModulePath[MAX_PATH] = { 0 };
	MultiByteToWideChar(CP_UTF8, MB_USEGLYPHCHARS, init.module_path.c_str(), -1, wModulePath, sizeof(wModulePath));
	PyWideStringList_Append(&pyConfig.module_search_paths, wModulePath);

	std::wstring wLibsPath = std::wstring(wPyHome) + L"/Lib";
	PyWideStringList_Append(&pyConfig.module_search_paths, wLibsPath.data());
	pyConfig.module_search_paths_set = 1;

	pyConfig.optimization_level = 2;

	auto status = Py_InitializeFromConfig(&pyConfig);
	if (PyStatus_IsError(status)) {
		std::cout << status.err_msg << '\n';
	}
	else {
		std::cout << __FUNCTION__ << ":" << __LINE__ << " Python Initialized\n";
	}
	LoadModule();
	m_heads_list = PyList_New(0);
	m_chest_list = PyList_New(0);
	m_gloves_list = PyList_New(0);
	m_legs_list = PyList_New(0);
}


void PyData::Update() {
	if (g_should_reload.load()) {
		LoadModule();
		g_should_reload.store(false);
	}
	PyObject* heads_list = PyList_New(0);
	for (size_t i = 0; i < e_head_vec->size(); i++) {
		PyObject* item = ItemToPyDict((*e_head_vec)[i]);
		PyList_Insert(heads_list, 1, item);
		Py_DECREF(item);
	}

	PyObject* chest_list = PyList_New(0);
	for (size_t i = 0; i < e_chest_vec->size(); i++) {
		PyObject* item = ItemToPyDict((*e_chest_vec)[i]);
		PyList_Insert(chest_list, 1, item);
		Py_DECREF(item);
	}

	PyObject* gloves_list = PyList_New(0);
	for (size_t i = 0; i < e_gloves_vec->size(); i++) {
		PyObject* item = ItemToPyDict((*e_gloves_vec)[i]);
		PyList_Insert(gloves_list, 1, item);
		Py_DECREF(item);
	}

	PyObject* legs_list = PyList_New(0);
	for (size_t i = 0; i < e_legs_vec->size(); i++) {
		PyObject* item = ItemToPyDict((*e_legs_vec)[i]);
		PyList_Insert(legs_list, 1, item);
		Py_DECREF(item);
	}
	std::cout << "Calling python function\n";
	CallPyFunction(heads_list, chest_list, gloves_list, legs_list);
	Py_DECREF(heads_list);
	Py_DECREF(chest_list);
	Py_DECREF(gloves_list);
	Py_DECREF(legs_list);
}