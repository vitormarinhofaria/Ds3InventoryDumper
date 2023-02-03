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
//const size_t HEADER_SEARCH =   0x7FF4401B2288;

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
	auto dllPath = std::filesystem::current_path().concat("/Ds3InventoryDumper_DLL.dll").string();
	//std::string dllPath = "Ds3InventoryDumper_DLL.dll";
	void* pDllPath = VirtualAllocEx(dsHandle, 0, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!pDllPath)
	{
		std::cout << "Failed to allocate memory\n";
		waitForQuit();
		CloseHandle(dsHandle);
		return 0;
	}
	auto result = WriteProcessMemory(dsHandle, pDllPath, dllPath.data(), dllPath.size() + 1, nullptr);

	HANDLE loadLibT = CreateRemoteThread(dsHandle, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, pDllPath, 0, 0);
	if (loadLibT)
	{
		std::cout << "Done!!\n";
		CloseHandle(loadLibT);
	}
	CloseHandle(dsHandle);
	return 0;
}