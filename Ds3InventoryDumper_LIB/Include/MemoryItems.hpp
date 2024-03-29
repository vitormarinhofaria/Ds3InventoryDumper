#pragma once
#include <map>
#include <string>

struct InventoryHeader {
	size_t endAddr = 0;
	size_t beginAddr = 0;
	size_t size = 0;

	const bool validate(const size_t baseAddr);
};
struct Item {
	uint32_t gid = 0;
	char name[60] = "N/A\0";
	float defense = 0.0f;
	float weight = 0.0f;
};
struct MemItem {
	uint32_t begin = 0;
	uint32_t gid = 0;
	uint32_t cout = 0;
	uint32_t end = 0;
};

static std::map<uint32_t, Item> ItemsDb;
void LoadItemsDB(const std::string_view filePath);
const std::map<uint32_t, Item>& GetItemsDB();

std::string SaveAsJson(const std::vector<Item>& items);

const size_t HEADER_SEARCH = 0x7FF440102288;
constexpr size_t InventorySize = 1920;
size_t DefaultInventorySize();

InventoryHeader ScanBasic(char* buffer, intptr_t bytesRead, bool* valid, size_t baseAddrs);
InventoryHeader ScanEx(HANDLE hProc);