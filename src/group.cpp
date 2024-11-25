#include "group.h"

void generateInitialGroup(std::vector<Group>& groups, const std::map<int, Image>& images) {
	groups.clear();
	Group initialGroup;
	initialGroup.startIndex = 0;
	initialGroup.endIndex = images.size() - 1;
	groups.push_back(initialGroup);
}

void generateGroups(std::vector<Group>& groups, const std::map<int, Image>& images) {
	int groupSize = 4;

	groups.clear();
	Group currentGroup;
	for (int i = 0; i < images.size(); i++) {
		if (currentGroup.startIndex == -1) {
			currentGroup.startIndex = i;
		} else if (i - currentGroup.startIndex + 1 == groupSize) {
			currentGroup.endIndex = i;
			groups.push_back(currentGroup);
			currentGroup = Group();
			currentGroup.startIndex = -1;
		}
	}

	if (currentGroup.startIndex <= images.size()) {
		currentGroup.endIndex = images.size() - 1;
		groups.push_back(currentGroup);
	}
}
