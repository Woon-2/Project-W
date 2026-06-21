#ifndef headless_stress_runner_hpp
#define headless_stress_runner_hpp

#include "Common.hpp"
#include "Metrics.hpp"
#include "StressConfig.hpp"
#include "BotSession.hpp"

#include <memory>
#include <vector>
#include <cstdio>

namespace hc {

// 전체 봇 수명·이벤트 루프·통계 출력을 주관한다.
class StressRunner {
public:
	explicit StressRunner(const StressConfig& cfg);
	~StressRunner();

	// 부하 테스트 실행. 0 = 정상 종료.
	int run();

private:
	void rampUp(TimePoint now);          // connectPerSecond 속도로 새 봇 접속 시작
	void pollOnce();                     // WSAPoll + IO 처리
	void driveSends(TimePoint now);      // InRoom 봇의 20Hz 송신
	void printStats(TimePoint now, bool finalLine);
	bool allDone() const;                // 모든 봇이 Closed 이고 더 만들 봇이 없음

	StressConfig cfg_;
	Metrics      m_;
	std::vector<std::unique_ptr<BotSession>> bots_;

	int       started_ = 0;
	TimePoint startTime_{};
	TimePoint nextConnect_{};
	TimePoint lastStat_{};
	Metrics::Snapshot prev_{};

	std::FILE* csv_ = nullptr;

	// poll 버퍼(재사용).
	std::vector<WSAPOLLFD>  pfds_;
	std::vector<BotSession*> pmap_;
};

} // namespace hc

#endif // headless_stress_runner_hpp
