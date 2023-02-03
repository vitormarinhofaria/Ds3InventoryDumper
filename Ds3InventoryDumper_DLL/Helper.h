#pragma once
#include "pch.h"

#include "MemoryItems.hpp"

PyObject* ItemToPyDict(const Item& item)
{
	PyObject* dict = PyDict_New();
	
	PyObject* name = PyUnicode_FromString(item.name.c_str());
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

Item ItemFromPyDict(PyObject* pItem)
{
	auto pName = PyDict_GetItemString(pItem, "name");
	auto pDefense = PyDict_GetItemString(pItem, "defense");
	auto pWeight = PyDict_GetItemString(pItem, "weight");
	
	std::string name = PyUnicode_AsUTF8(pName);
	float defense = PyFloat_AsDouble(pDefense);
	float weight = PyFloat_AsDouble(pWeight);

	Py_DECREF(pItem);

	Item item = { 0, name, defense, weight };
	return item;
}