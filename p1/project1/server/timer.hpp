#ifndef __timer_HPP
#define __timer_HPP

class Timer {
public:
	// 마지막 tick과 현재 tick과의 간격으로
	// Delta Time과 fps, 각종 보조 정보들을 계산한다.
	// * 타이머가 정지된 동안의 시간은 tick의 정보 갱신에 영향을 주지 않는다.
	void tick();

	// 타이머의 Tick을 비활성화 한다.
	// 타이머가 정지되어 있는 동안의 시간은 deltaTime에 가산되지 않는다.
	void stop();
	// 타이머의 Tick을 활성화 한다.
	// 타이머가 정지되어 있는 동안의 시간은 deltaTime에 가산되지 않는다.
	void resume();

	// Delta Time을 원하는 타입(MilliSeconds, Seconds, etc.)으로 반환한다.
	template <class TDuration>
		requires requires(TDuration& d){
			std::chrono::duration_cast<Milliseconds>(d);
		}
	TDuration deltaTime() const {
		return std::chrono::duration_cast<TDuration>(lastDeltaTime_);
	}

	// 지난 maxStamps_ 프레임 동안의 평균 fps를 반환한다.
	float fps() const { return fps_; }

private:
	std::vector<Milliseconds> dtStamps_{};
	HighResolutionClock::time_point lastTp_{ HighResolutionClock::now() };
	const std::size_t maxStamps_{60u};
	std::size_t tickIdx_{0u};
	Milliseconds lastDeltaTime_{0ms};
	Milliseconds sumDeltaTime_{0ms};
	Milliseconds stopAccDeltaTime_{0ms};
	float fps_ = 0.f;
	bool stopped_ = false;
};

#endif	// __timer_HPP