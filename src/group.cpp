#include "group.h"

void generateGroups(std::vector<Group>& groups, const std::vector<Image>& images) {
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
