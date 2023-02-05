#include "pch.h"
#include "MemoryItems.hpp"

const bool InventoryHeader::validate(const size_t baseAddr)
{
	if (this->endAddr <= this->beginAddr)
	{
		return false;
	}
	size_t lenght = (this->endAddr - this->beginAddr) / sizeof(MemItem);
	if (this->size != 1920) return false;
	return (baseAddr + sizeof(size_t) == this->beginAddr && lenght >> 2 == this->size >> 2);
}

void LoadItemsDB(const std::string_view filePath)
{
	std::ifstream inFile(filePath.data());
	std::cout << filePath << "is_open? " << inFile.is_open() << '\n';
	std::map<uint32_t, Item> db;

	std::stringstream parser;

	std::string line;
	while (std::getline(inFile, line))
	{
		std::stringstream ss;
		ss << line;

		std::vector<std::string> splits;
		splits.reserve(4);
		std::string split;
		while (std::getline(ss, split, '\t'))
		{
			splits.push_back(split);
		}

		Item item;

		parser << std::hex << splits[0];
		parser >> item.gid;
		parser.clear();

		//item.name = splits[1];
		strcpy_s(item.name, splits[1].c_str());

		if (splits.size() > 2)
		{
			parser << splits[2];
			parser >> item.defense;
			parser.clear();

			if (splits.size() > 3)
			{
				parser << splits[3];
				parser >> item.weight;
				parser.clear();
			}
		}

		db.insert_or_assign(item.gid, item);
	}
	inFile.close();
	ItemsDb = db;
}

const std::map<uint32_t, Item>& GetItemsDB()
{
	return ItemsDb;
}

std::string SaveAsJson(const std::vector<Item>& items)
{
	using namespace nlohmann;
	json j = json::array();

	for (auto& item : items)
	{
		json i;
		i["gid"] = item.gid;
		i["name"] = item.name;
		i["defense"] = item.defense;
		i["weight"] = item.weight;

		j.push_back(i);
	}
	std::stringstream returnString;
	returnString << j;
	return returnString.str();
}

InventoryHeader ScanBasic(char* buffer, intptr_t bytesRead, bool* valid, size_t baseAddrs) {
	InventoryHeader header;
	std::cout << "Scanning at: " << std::hex << (size_t)baseAddrs << std::dec << "\n";
	for (auto i = 0ui64; i < bytesRead; i++)
	{
		auto memAddr = buffer + (i * 8ui64);
		InventoryHeader tempHeader;
		std::memcpy(&tempHeader, memAddr, sizeof(InventoryHeader));
		if (tempHeader.validate((size_t)baseAddrs + (i * 8ui64)))
		{
			*valid = true;
			header = tempHeader;
			return header;
		}
	}
	return header;
}
InventoryHeader ScanEx(HANDLE hProc)
{
	InventoryHeader header;
	char* match{ nullptr };
	SIZE_T bytesRead;
	DWORD oldprotect;
	char* buffer{ nullptr };
	MEMORY_BASIC_INFORMATION mbi{};
	char* begin = (char*)0x7FF3A0000000;
	uint64_t size = 0x100000000000;

	if (hProc == nullptr) {
		VirtualQuery((LPCVOID)begin, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
	}
	else {
		VirtualQueryEx(hProc, (LPCVOID)begin, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
	}

	for (char* curr = begin; curr < begin + size; curr += mbi.RegionSize)
	{
		if (hProc == nullptr) {
			if (!VirtualQuery(curr, &mbi, sizeof(mbi))) continue;
		}
		else {
			if (!VirtualQueryEx(hProc, curr, &mbi, sizeof(mbi))) continue;
		}
		if (mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS ) continue;

		delete[] buffer;
		buffer = new char[mbi.RegionSize];

		bool valid = false;
		if (hProc == nullptr) {
			if (VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &oldprotect))
			{
				header = ScanBasic((char*)mbi.BaseAddress, mbi.RegionSize, &valid, (size_t)mbi.BaseAddress);
				VirtualProtect(mbi.BaseAddress, mbi.RegionSize, oldprotect, &oldprotect);
			}
		}
		else {
			if (VirtualProtectEx(hProc, mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &oldprotect))
			{
				ReadProcessMemory(hProc, mbi.BaseAddress, buffer, mbi.RegionSize, &bytesRead);
				VirtualProtectEx(hProc, mbi.BaseAddress, mbi.RegionSize, oldprotect, &oldprotect);
				header = ScanBasic(buffer, (intptr_t)bytesRead, &valid, (size_t)mbi.BaseAddress);
			}
		}
		if (valid) {
			return header;
		}
	}
	delete[] buffer;
	std::cout << std::endl;
	return header;
}

size_t DefaultInventorySize() { return InventorySize; }