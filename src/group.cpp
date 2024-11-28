#include <functional>
#include <iostream>
#include <time.h>
#include "group.h"

// Forward declarations
void applyTimeSplits(std::vector<std::vector<int>>& groups, int seconds, const std::map<int, Image>& images);

void generateInitialGroup(std::vector<std::vector<int>>& groups, const std::map<int, Image>& images) {
	groups.clear();

	std::vector<int> initialGroup;
	for (const auto& [id, image] : images) {
		initialGroup.push_back(id);
	}

	// TODO: handle other baseline sorting options
	std::sort(initialGroup.begin(), initialGroup.end(), [&images](int a, int b) {
		return images.at(a).filename.compare(images.at(b).filename) < 0;
	});

	groups.push_back(initialGroup);
}

void generateGroups(std::vector<std::vector<int>>& groups, GroupParameters& parameters, const std::map<int, Image>& images) {
	generateInitialGroup(groups, images);

	if (parameters.timeEnabled) {
		applyTimeSplits(groups, parameters.timeSeconds, images);
	} else {
		generateInitialGroup(groups, images);
	}
}

void applySplits(std::vector<std::vector<int>>& groups, const std::map<int, Image>& images, std::function<bool(int, int)> comparator) {
	for (int currentGroup = 0; currentGroup < groups.size(); currentGroup++) {
		// In the current group, find the next image that should be the last in its group.
		for (int i = 0; i < groups[currentGroup].size() - 1; i++) {
			int currentID = groups[currentGroup][i];
			int nextID = groups[currentGroup][i + 1];

			// If image doesn't have time metadata, just include it in current group?
			// TODO: maybe it should be in a new group
			if (!images.at(currentID).metadata.timestamp.has_value()) {
				continue;
			}

			const ImageTimestamp& currentTimestamp = images.at(currentID).metadata.timestamp.value();
			const ImageTimestamp& nextTimestamp = images.at(nextID).metadata.timestamp.value();
			if (comparator(currentID, nextID)) {
				// Split current group into 0:i and i+1:end
				std::vector<int> newGroup{ groups[currentGroup].begin() + i + 1, groups[currentGroup].end() };
				groups[currentGroup].erase(groups[currentGroup].begin() + i + 1, groups[currentGroup].end());
				groups.insert(groups.begin() + currentGroup + 1, newGroup);
				break;
			}
		}
	}
}

void applyTimeSplits(std::vector<std::vector<int>>& groups, int seconds, const std::map<int, Image>& images) {
	applySplits(groups, images, [&images, seconds](int currentID, int nextID) {
		if (!images.at(currentID).metadata.timestamp.has_value() || !images.at(nextID).metadata.timestamp.has_value()) {
			return false;
		}

		const ImageTimestamp& currentTimestamp = images.at(currentID).metadata.timestamp.value();
		const ImageTimestamp& nextTimestamp = images.at(nextID).metadata.timestamp.value();
		return nextTimestamp.secondsSinceEpoch - currentTimestamp.secondsSinceEpoch >= seconds;
	});
}
