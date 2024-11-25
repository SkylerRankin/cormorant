#pragma once
#include <filesystem>
#include <string>
#include <map>
#include "cache.h"

struct Group {
	int startIndex = -1;
	int endIndex = -1;
};

void generateInitialGroup(std::vector<Group>& groups, const std::map<int, Image>& images);
void generateGroups(std::vector<Group>& groups, const std::map<int, Image>& images);
