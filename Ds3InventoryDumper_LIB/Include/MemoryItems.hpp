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
	std::string name = "";
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

const size_t HEADER_SEARCH = 0x7FF49E871080;