// Ds3InventoryDumper.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include <fstream>
#include <format>

#include <Windows.h>

#include "MemoryItems.hpp"

const void waitForQuit()
{
	std::cout << "Press ENTER to quit...\n";
	std::cin.get();
}

//const size_t HEADER_SEARCH = 0x7FF49E871080;

int main(int argc, char** argv)
{
	HWND dsHwnd = FindWindow(nullptr, L"DARK SOULS III");
	if (!dsHwnd)
	{
		std::cout << "Error: Failed to find DARK SOULS III window\n";
		waitForQuit();
		return -1;
	}

	DWORD dsPid = 0;
	GetWindowThreadProcessId(dsHwnd, &dsPid);
	HANDLE dsHandle = OpenProcess(PROCESS_ALL_ACCESS, 0, dsPid);
	if (!dsHandle)
	{
		std::cout << "Error: Failed to get DARK SOULS III process, try running as Admin\n";
		waitForQuit();
		return -1;
	}

	InventoryHeader header;
	for (auto i = 0ui64; i < 4096; i++)
	{
		auto memAddr = HEADER_SEARCH + (i * 8ui64);
		InventoryHeader tempHeader;

		auto result = ReadProcessMemory(dsHandle, (LPCVOID)memAddr, &tempHeader, sizeof(InventoryHeader), 0);
		if (result)
		{
			if (tempHeader.validate(memAddr))
			{
				header = tempHeader;
				break;
			}
		}
	}
	if (header.size == 0)
	{
		std::cout << "Error: Failed to find player's inventory.\n";
		waitForQuit();
		return -1;
	}

	auto workDir = std::filesystem::current_path();
	auto itemsFilePath = workDir.string() + "\\items.txt";
	LoadItemsDB(itemsFilePath);

	std::vector<MemItem> memItems(header.size);
	std::vector<Item> items;

	auto result = ReadProcessMemory(dsHandle, (LPCVOID)(header.beginAddr + 16), &memItems[0], sizeof(MemItem) * header.size, nullptr);
	if (result)
	{
		for (auto& memItem : memItems)
		{
			bool isInDB = GetItemsDB().contains(memItem.gid);
			if (isInDB)
			{
				Item item = GetItemsDB().at(memItem.gid);
				items.push_back(GetItemsDB().at(memItem.gid));
				/*std::cout << "ID: " << std::hex << item.gid << "\t" << "Name: " << std::setw(30) << item.name << ' ' << "Defense: " << item.defense << ' ' << "Weight: " << item.weight << '\n';*/
				std::cout << std::format("ID: {:08X}\t{:30} Defense: {:3.1f}\tWeight: {:3.1f}\n", item.gid, item.name, item.defense, item.weight);
			}
		}
	}
	std::vector<Item> head;
	std::vector<Item> armor;
	std::vector<Item> gloves;
	std::vector<Item> pants;
	std::vector<Item> allItems;

	for (auto i = 0; i < header.size; i++)
	{
		bool isInDB = GetItemsDB().contains(memItems[i].gid);
		if (isInDB)
		{
			Item item = GetItemsDB().at(memItems[i].gid);
			allItems.push_back(item);

			if (item.name.find("Helm") != std::string::npos ||
				item.name.find("Hood") != std::string::npos ||
				item.name.find("Veil") != std::string::npos ||
				item.name.find("Hat") != std::string::npos)
			{
				head.push_back(item);
			}
			if (item.name.find("Armor") != std::string::npos || item.name.find("Robe") != std::string::npos)
			{
				armor.push_back(item);
			}
			if (item.name.find("Gauntlet") != std::string::npos || item.name.find("Gloves") != std::string::npos)
			{
				gloves.push_back(item);
			}
			if (item.name.find("Trousers") != std::string::npos || item.name.find("Leggings") != std::string::npos)
			{
				pants.push_back(item);
			}
		}
	}
	if (head.size() != 0)
		std::cout << std::format("Head: {}\n", head[0].name);
	
	if(armor.size() != 0)
		std::cout << std::format("Chest: {}\n", armor[0].name);

	if (gloves.size() != 0)
		std::cout << std::format("Hands: {}\n", gloves[0].name);

	if (pants.size() != 0)
		std::cout << std::format("Legs: {}\n", pants[0].name);

	auto json = SaveAsJson(items);
	std::ofstream of("items.json");
	of << json; of.flush(); of.close();

	std::string dllPath = "Ds3InventoryDumper_DLL.dll";
	void* pDllPath = VirtualAllocEx(dsHandle, 0, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	WriteProcessMemory(dsHandle, pDllPath, dllPath.data(), dllPath.size() + 1, nullptr);

	HANDLE loadLibT = CreateRemoteThread(dsHandle, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, pDllPath, 0, 0);
	if (loadLibT)
	{
		std::cout << "Done!!\n";
		CloseHandle(loadLibT);
	}
	return 0;
}