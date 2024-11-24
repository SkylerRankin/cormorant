#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "cache.h"

struct Group {
	int startIndex = -1;
	int endIndex = -1;
};

void generateGroups(std::vector<Group>& groups, const std::vector<Image>& images);
