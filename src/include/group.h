#pragma once
#include <map>
#include <vector>
#include "cache.h"

namespace Group {
	struct ImageGroup {
		std::vector<int> ids;
		int savedCount;
		int skippedCount;
	};

	enum GroupParam {
		GroupParam_Time = 0,
	};

	struct GroupParameters {
		// Min number of seconds between images to be considered different groups
		bool timeEnabled = false;
		int timeSeconds = 10;
	};

	void generateInitialGroup(std::vector<ImageGroup>& groups, const std::map<int, Image>& images);
	void generateGroups(std::vector<ImageGroup>& groups, GroupParameters& parameters, const std::map<int, Image>& images);
}
