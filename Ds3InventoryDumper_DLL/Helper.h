#pragma once
#include "pch.h"

#include "MemoryItems.hpp"

struct InitPythonData {
	std::string module_path;
	Item* head = nullptr;
	Item* chest = nullptr;
	Item* gloves = nullptr;
	Item* legs = nullptr;
	std::vector<Item>* head_vec = nullptr;
	std::vector<Item>* chest_vec = nullptr;
	std::vector<Item>* gloves_vec = nullptr;
	std::vector<Item>* legs_vec = nullptr;
};

struct PyData {
	PyObject* m_module = nullptr;
	PyObject* m_calc_function = nullptr;

	Item* e_head = nullptr;
	Item* e_chest = nullptr;
	Item* e_gloves = nullptr;
	Item* e_legs = nullptr;
	std::vector<Item>* e_head_vec = nullptr;
	std::vector<Item>* e_chest_vec = nullptr;
	std::vector<Item>* e_gloves_vec = nullptr;
	std::vector<Item>* e_legs_vec = nullptr;
	std::atomic_bool m_should_reload = false;

	void InitPython(InitPythonData init);
	void Update();
	void UpdateNative();
private:
	PyObject* m_heads_list, *m_chest_list, *m_gloves_list, *m_legs_list = nullptr;
	void LoadModule();
	void CallPyFunction(PyObject* head, PyObject* chest, PyObject* hands, PyObject* legs);
};
static std::shared_mutex g_writing;
static std::atomic_bool g_should_reload = false;

static PyObject* ItemToPyDict(const Item& item)
{
	PyObject* dict = PyDict_New();

	PyObject* name = PyUnicode_FromString(item.name);
	PyDict_SetItemString(dict, "name", name);
	Py_DECREF(name);

	PyObject* defense = PyFloat_FromDouble(item.defense);
	PyDict_SetItemString(dict, "defense", defense);
	Py_DECREF(defense);

	PyObject* weight = PyFloat_FromDouble(item.weight);
	PyDict_SetItemString(dict, "weight", weight);
	Py_DECREF(weight);

	return dict;
}

static Item ItemFromPyDict(PyObject* pItem)
{
	auto pName = PyDict_GetItemString(pItem, "name");
	auto pDefense = PyDict_GetItemString(pItem, "defense");
	auto pWeight = PyDict_GetItemString(pItem, "weight");

	std::string name = PyUnicode_AsUTF8(pName);
	float defense = static_cast<float>(PyFloat_AsDouble(pDefense));
	float weight = static_cast<float>(PyFloat_AsDouble(pWeight));

	Py_DECREF(pItem);

	Item item = { .gid = 0, .defense = defense, .weight = weight };
	strcpy_s(item.name, name.c_str());
	return item;
}

static void SetShouldReloadPython(bool val) {
	g_should_reload.store(val);
}


static std::shared_mutex& get_py_mutex() {
	return g_writing;
}