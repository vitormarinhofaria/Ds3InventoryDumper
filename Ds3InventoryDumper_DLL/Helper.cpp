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
		Py_XDECREF(module_name);
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
	Py_XDECREF(pArgs);

	PyObject* pHead = PyTuple_GetItem(val, 0);
	*e_head = ItemFromPyDict(pHead);
	Py_XDECREF(pHead);

	PyObject* pChest = PyTuple_GetItem(val, 1);
	*e_chest = ItemFromPyDict(pChest);
	Py_XDECREF(pChest);

	PyObject* pHands = PyTuple_GetItem(val, 2);
	*e_gloves = ItemFromPyDict(pHands);
	Py_XDECREF(pHands);

	PyObject* pLegs = PyTuple_GetItem(val, 3);
	*e_legs = ItemFromPyDict(pLegs);
	Py_XDECREF(pLegs);
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

void PyData::UpdateNative() {
	auto start = std::chrono::system_clock::now();
	Item melhor_elmo = { .gid = 0, .name = "N/A", .defense = 1, .weight = 1 };
	Item melhor_peitoral = { .gid = 0, .name = "N/A", .defense = 1, .weight = 1 };
	Item melhor_luva = { .gid = 0, .name = "N/A", .defense = 1, .weight = 1 };
	Item melhor_calca = { .gid = 0, .name = "N/A", .defense = 1, .weight = 1 };

	float soma_def = 0.0f;
	float soma_peso = 0.0f;
	float capacidade = 60.0f;

	for (size_t ie = 0; ie < e_head_vec->size(); ie++) {
		Item elmo = (*e_head_vec)[ie];
		for (size_t ip = 0; ip < e_chest_vec->size(); ip++) {
			Item peitoral = (*e_chest_vec)[ip];
			for (size_t ic = 0; ic < e_legs_vec->size(); ic++) {
				Item calca = (*e_legs_vec)[ic];
				for (size_t il = 0; il < e_gloves_vec->size(); il++) {
					Item luva = (*e_gloves_vec)[il];
					float new_soma_defesa = elmo.defense + peitoral.defense + calca.defense + luva.defense;
					float peso = elmo.weight + peitoral.weight + calca.weight + luva.weight;
					if (new_soma_defesa >= soma_def && peso <= capacidade) {
						soma_def = new_soma_defesa;
						soma_peso = peso;
						melhor_elmo = elmo;
						melhor_peitoral = peitoral;
						melhor_calca = calca;
						melhor_luva = luva;
					}
				}
			}
		}
	}

	*e_head = melhor_elmo;
	*e_chest = melhor_peitoral;
	*e_legs = melhor_calca;
	*e_gloves = melhor_luva;
	auto end = std::chrono::system_clock::now();
	auto duration = end - start;
	std::cout << "C version took: " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << "ms -- begin " << start << "end " << end << "\n";
}

void PyData::Update() {
	if (m_should_reload.load()) {
		LoadModule();
		m_should_reload.store(false);
	}
	PyObject* heads_list = PyList_New(e_head_vec->size());
	for (size_t i = 0; i < e_head_vec->size(); i++) {
		PyObject* item = ItemToPyDict((*e_head_vec)[i]);
		if (PyList_SetItem(heads_list, i, item) == -1) {
			std::cout << "Failed to append to list\n";
			PyErr_Print();
			std::cin.get();
		}
	}

	PyObject* chest_list = PyList_New(e_chest_vec->size());
	for (size_t i = 0; i < e_chest_vec->size(); i++) {
		PyObject* item = ItemToPyDict((*e_chest_vec)[i]);
		if (PyList_SetItem(chest_list, i, item) == -1) {
			std::cout << "Failed to append to list\n";
			PyErr_Print();
			std::cin.get();
		}
	}

	PyObject* gloves_list = PyList_New(e_gloves_vec->size());
	for (size_t i = 0; i < e_gloves_vec->size(); i++) {
		PyObject* item = ItemToPyDict((*e_gloves_vec)[i]);
		if (PyList_SetItem(gloves_list, i, item) == -1) {
			std::cout << "Failed to append to list\n";
			PyErr_Print();
			std::cin.get();
		}
	}

	PyObject* legs_list = PyList_New(e_legs_vec->size());
	for (size_t i = 0; i < e_legs_vec->size(); i++) {
		PyObject* item = ItemToPyDict((*e_legs_vec)[i]);
		if (PyList_SetItem(legs_list, i, item) == -1) {
			std::cout << "Failed to append to list\n";
			PyErr_Print();
			std::cin.get();
		}
	}
	CallPyFunction(heads_list, chest_list, gloves_list, legs_list);
	Py_XDECREF(heads_list);
	Py_XDECREF(chest_list);
	Py_XDECREF(gloves_list);
	Py_XDECREF(legs_list);
	//UpdateNative();
}