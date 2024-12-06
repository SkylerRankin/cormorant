#include <iostream>
#include "glCommon.h"
#include "stats.h"

void Monitor::handleTimerValue(MonitorStat stat, double value) {
	switch (stat) {
	case Monitor_FrameTime:
		if (frameTimeIntervalRemaining == 0) {
			// Completed an interval of n frames
			frameTimes.push_back(frameTimeIntervalSum / frameTimeInterval);
			if (frameTimes.size() > frameTimePlotSize) {
				frameTimes.pop_front();
			}
			frameTimeIntervalSum = 0;
			frameTimeIntervalRemaining = frameTimeInterval;
		} else {
			frameTimeIntervalRemaining--;
			frameTimeIntervalSum += value;
		}

		if (frameTimes.size() == 0) {
			frameTimeAverage = value;
		} else {
			frameTimeAverage = frameTimeAverageAlpha * frameTimeAverage + (1 - frameTimeAverageAlpha) * value;
		}
		
		break;
	}
}

void Monitor::pauseTimers() {
	for (auto& timer : activeTimers) {
		timer->pause();
	}
}

void Monitor::resumeTimers() {
	for (auto& timer : activeTimers) {
		timer->resume();
	}
}

void Monitor::addTimer(BlockTimer* timer) {
	activeTimers.insert(timer);
}

void Monitor::removeTimer(BlockTimer* timer) {
	activeTimers.erase(timer);
}

BlockTimer::BlockTimer(Monitor* monitor, MonitorStat statistic) : monitor(monitor), statistic(statistic) {
	startTime = glfwGetTime();
	monitor->addTimer(this);
}

BlockTimer::~BlockTimer() {
	double totalTime = time;
	if (!paused) {
		totalTime += glfwGetTime() - startTime;;
	}
	monitor->handleTimerValue(statistic, totalTime);
	monitor->removeTimer(this);
}

void BlockTimer::pause() {
	time += glfwGetTime() - startTime;
	paused = true;
}

void BlockTimer::resume() {
	paused = false;
	startTime = glfwGetTime();
}
