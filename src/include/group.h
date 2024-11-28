#pragma once
#include <filesystem>
#include <string>
#include <map>
#include "cache.h"

enum GroupParam {
	GroupParam_Time = 0,
};

struct GroupParameters {
	// Min number of seconds between images to be considered different groups
	bool timeEnabled = false;
	int timeSeconds = 10;
};

void generateInitialGroup(std::vector<std::vector<int>>& groups, const std::map<int, Image>& images);
void generateGroups(std::vector<std::vector<int>>& groups, GroupParameters& parameters, const std::map<int, Image>& images);
