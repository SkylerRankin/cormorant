#pragma once
#include <set>
#include <deque>

enum MonitorStat {
	Monitor_FrameTime = 0
};

class BlockTimer;

class Monitor {
public:
	// Frame rate info
	std::deque<double> frameTimes;
	double frameTimeAverage = 0.0;

	void handleTimerValue(MonitorStat stat, double value);
	void pauseTimers();
	void resumeTimers();
	void addTimer(BlockTimer* timer);
	void removeTimer(BlockTimer* timer);

private:
	std::set<BlockTimer*> activeTimers;
	double pauseStart;
	double pausedTime;

	// Frame rate info
	// Number of frame data points to keep
	const int frameTimePlotSize = 100;
	const float frameTimeAverageAlpha = 0.9f;
	// Number of frames to average for a single data point
	const float frameTimeInterval = 10;
	double frameTimeIntervalRemaining = frameTimeInterval;
	double frameTimeIntervalSum = 0;
};

class BlockTimer {
public:
	BlockTimer(Monitor* monitor, MonitorStat statistic);
	~BlockTimer();
	void pause();
	void resume();
private:
	Monitor* monitor;
	MonitorStat statistic;
	double time = 0.0;
	double startTime;
	bool paused = false;
};
