#include <functional>
#include <iostream>
#include <time.h>
#include "group.h"

// Forward declarations
void applyTimeSplits(std::vector<ImageGroup>& groups, int seconds, const std::map<int, Image>& images);

void updateSavedSkippedCounts(std::vector<ImageGroup>& groups, const std::map<int, Image>& images) {
	for (ImageGroup& group : groups) {
		group.savedCount = 0;
		group.skippedCount = 0;
		for (int id : group.ids) {
			if (images.at(id).saved) {
				group.savedCount++;
			} else if (images.at(id).skipped) {
				group.skippedCount++;
			}
		}
	}
}

void generateInitialGroup(std::vector<ImageGroup>& groups, const std::map<int, Image>& images) {
	groups.clear();

	ImageGroup initialGroup = {
		.ids = {},
		.savedCount = 0,
		.skippedCount = 0
	};
	for (const auto& [id, image] : images) {
		initialGroup.ids.push_back(id);
	}

	// TODO: handle other baseline sorting options
	std::sort(initialGroup.ids.begin(), initialGroup.ids.end(), [&images](int a, int b) {
		return images.at(a).filename.compare(images.at(b).filename) < 0;
	});

	groups.push_back(initialGroup);
	updateSavedSkippedCounts(groups, images);
}

void generateGroups(std::vector<ImageGroup>& groups, GroupParameters& parameters, const std::map<int, Image>& images) {
	generateInitialGroup(groups, images);

	if (parameters.timeEnabled) {
		applyTimeSplits(groups, parameters.timeSeconds, images);
	} else {
		generateInitialGroup(groups, images);
	}

	updateSavedSkippedCounts(groups, images);
}

void applySplits(std::vector<ImageGroup>& groups, const std::map<int, Image>& images, std::function<bool(int, int)> comparator) {
	for (int currentGroup = 0; currentGroup < groups.size(); currentGroup++) {
		// In the current group, find the next image that should be the last in its group.
		for (int i = 0; i < groups[currentGroup].ids.size() - 1; i++) {
			int currentID = groups[currentGroup].ids[i];
			int nextID = groups[currentGroup].ids[i + 1];

			if (comparator(currentID, nextID)) {
				// Split current group into 0:i and i+1:end
				ImageGroup newGroup{
					.ids = {groups[currentGroup].ids.begin() + i + 1, groups[currentGroup].ids.end()},
					.savedCount = 0,
					.skippedCount = 0
				};
				groups[currentGroup].ids.erase(groups[currentGroup].ids.begin() + i + 1, groups[currentGroup].ids.end());
				groups.insert(groups.begin() + currentGroup + 1, newGroup);
				break;
			}
		}
	}
}

void applyTimeSplits(std::vector<ImageGroup>& groups, int seconds, const std::map<int, Image>& images) {
	applySplits(groups, images, [&images, seconds](int currentID, int nextID) {
		if (!images.at(currentID).metadata.timestamp.has_value() || !images.at(nextID).metadata.timestamp.has_value()) {
			return false;
		}

		const ImageTimestamp& currentTimestamp = images.at(currentID).metadata.timestamp.value();
		const ImageTimestamp& nextTimestamp = images.at(nextID).metadata.timestamp.value();
		return nextTimestamp.secondsSinceEpoch - currentTimestamp.secondsSinceEpoch >= seconds;
	});
}
