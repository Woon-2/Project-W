#include "pch.hpp"
#include "timer.hpp"

// 마지막 tick과 현재 tick과의 간격으로
// Delta Time과 fps, 각종 보조 정보들을 계산한다.
// * 타이머가 정지된 동안의 시간은 tick의 정보 갱신에 영향을 주지 않는다.
void Timer::tick() {
	// 타이머가 멈췄을 시 아무것도 갱신되지 않음
	if (stopped_) {
		return;
	}

	const auto tp = HighResolutionClock::now();

	// delta time 계산
	lastDeltaTime_ = stopAccDeltaTime_ + (tp - lastTp_);
	stopAccDeltaTime_ = 0ms;

	// delta time stamp 추가
	// maxStamps_는 상수이므로, dtStamps_.size()가 maxStamps_보다 클 일은 없다.
	if (dtStamps_.size() == maxStamps_) {
		sumDeltaTime_ -= dtStamps_[tickIdx_ % maxStamps_];
		dtStamps_[tickIdx_ % maxStamps_] = lastDeltaTime_;
		sumDeltaTime_ += lastDeltaTime_;
	}
	else {
		dtStamps_.push_back(lastDeltaTime_);
		sumDeltaTime_ += lastDeltaTime_;
	}

	// FPS 계산
	const auto avgFrameTime = sumDeltaTime_ / dtStamps_.size();
	fps_ = static_cast<float>(1.0 / std::chrono::duration_cast<Seconds>(avgFrameTime).count());

	// 틱 인덱스 및 타임 스탬프 갱신
	++tickIdx_;
	lastTp_ = tp;
}

// 타이머의 Tick을 비활성화 한다.
// 타이머가 정지되어 있는 동안의 시간은 deltaTime에 가산되지 않는다.
void Timer::stop() {
	stopped_ = true;
	const auto tp = HighResolutionClock::now();
	stopAccDeltaTime_ = tp - lastTp_;
}

// 타이머의 Tick을 활성화 한다.
// 타이머가 정지되어 있는 동안의 시간은 deltaTime에 가산되지 않는다.
void Timer::resume() {
	if (stopped_) {
		lastTp_ = HighResolutionClock::now();
	}
	stopped_ = false;
}
