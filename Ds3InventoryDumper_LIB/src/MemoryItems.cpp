#include "pch.h"
#include "MemoryItems.hpp"

const bool InventoryHeader::validate(const size_t baseAddr)
{
	if (this->endAddr <= this->beginAddr)
	{
		return false;
	}
	size_t lenght = (this->endAddr - this->beginAddr) / sizeof(MemItem);
	return (baseAddr + sizeof(size_t) == this->beginAddr && lenght >> 2 == this->size >> 2);
}

void LoadItemsDB(const std::string_view filePath)
{
	std::ifstream inFile(filePath.data());
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

		item.name = splits[1];

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
		i["gid"]	 = item.gid;
		i["name"]	 = item.name;
		i["defense"] = item.defense;
		i["weight"]	 = item.weight;

		j.push_back(i);
	}
	std::stringstream returnString;
	returnString << j;
	return returnString.str();
}

