#include "StressRunner.hpp"

#include <thread>
#include <string>
#include <cstdio>

namespace hc {

namespace {

std::string fmtBps(double bps) {
	char buf[32];
	if (bps >= 1024.0 * 1024.0) {
		std::snprintf(buf, sizeof(buf), "%.1fMB/s", bps / (1024.0 * 1024.0));
	} else if (bps >= 1024.0) {
		std::snprintf(buf, sizeof(buf), "%.0fKB/s", bps / 1024.0);
	} else {
		std::snprintf(buf, sizeof(buf), "%.0fB/s", bps);
	}
	return buf;
}

double secondsBetween(TimePoint a, TimePoint b) {
	return std::chrono::duration<double>(b - a).count();
}

} // namespace

StressRunner::StressRunner(const StressConfig& cfg)
	: cfg_(cfg) {
	bots_.reserve(static_cast<size_t>(cfg_.botCount));
	if (!cfg_.csvPath.empty()) {
		std::FILE* f = nullptr;
		if (::fopen_s(&f, cfg_.csvPath.c_str(), "w") == 0) {
			csv_ = f;
		}
		if (csv_) {
			std::fprintf(csv_,
				"time_s,bots,connected,inRoom,sendPps,recvPps,sendBps,recvBps,"
				"disconnect,parseError,connectFail,npcBatchRecv\n");
		}
	}
}

StressRunner::~StressRunner() {
	if (csv_) {
		std::fclose(csv_);
		csv_ = nullptr;
	}
}

int StressRunner::run() {
	const TimePoint now0 = SteadyClock::now();
	startTime_   = now0;
	nextConnect_ = now0;
	lastStat_    = now0;
	prev_        = m_.snapshot();

	std::printf("HeadlessClient: target=%s:%u bots=%d playersPerRoom=%d moveHz=%d duration=%ds\n",
		cfg_.ip.c_str(), cfg_.port, cfg_.botCount, cfg_.playersPerRoom,
		cfg_.movePacketHz, cfg_.testDurationSec);

	for (;;) {
		const TimePoint now = SteadyClock::now();

		if (cfg_.testDurationSec > 0 &&
			secondsBetween(startTime_, now) >= cfg_.testDurationSec) {
			break;
		}

		rampUp(now);
		pollOnce();
		driveSends(SteadyClock::now());

		if (secondsBetween(lastStat_, SteadyClock::now()) >= 1.0) {
			printStats(SteadyClock::now(), /*finalLine*/ false);
			lastStat_ = SteadyClock::now();
		}

		if (allDone()) {
			std::printf("All bots closed before duration elapsed (server down or all disconnected).\n");
			break;
		}
	}

	printStats(SteadyClock::now(), /*finalLine*/ true);

	// 정상 종료: 서버가 disconnect 를 감지하도록 모든 봇을 닫는다.
	for (auto& b : bots_) {
		b->shutdownBot();
	}
	return 0;
}

void StressRunner::rampUp(TimePoint now) {
	if (started_ >= cfg_.botCount) {
		return;
	}

	const bool burst = cfg_.connectPerSecond <= 0.0;
	const std::chrono::nanoseconds period = burst
		? std::chrono::nanoseconds(0)
		: std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::duration<double>(1.0 / cfg_.connectPerSecond));

	while (started_ < cfg_.botCount && now >= nextConnect_) {
		const int roomIndex = started_ / cfg_.playersPerRoom;
		char code[7];
		// "R" + 5자리 = 6자(+null). 같은 코드를 쓴 playersPerRoom 명이 한 Room 에 모인다.
		std::snprintf(code, sizeof(code), "R%05d", roomIndex % 100000);

		auto bot = std::make_unique<BotSession>(started_, code, cfg_, m_);
		bot->startConnect();
		bots_.push_back(std::move(bot));
		++started_;

		if (burst) {
			continue;  // 한 번에 전부.
		}
		nextConnect_ += period;
	}
}

void StressRunner::pollOnce() {
	pfds_.clear();
	pmap_.clear();

	for (auto& b : bots_) {
		if (!b->active() || b->sock() == INVALID_SOCKET) {
			continue;
		}
		WSAPOLLFD pfd{};
		pfd.fd = b->sock();
		pfd.events = POLLRDNORM;
		if (b->wantsWrite()) {
			pfd.events |= POLLWRNORM;
		}
		pfds_.push_back(pfd);
		pmap_.push_back(b.get());
	}

	if (pfds_.empty()) {
		// 폴링할 소켓이 없으면(아직 ramp 중) 잠깐 쉬어 busy-spin 방지.
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		return;
	}

	const int r = ::WSAPoll(pfds_.data(), static_cast<ULONG>(pfds_.size()), 5 /*ms*/);
	if (r <= 0) {
		return;  // 타임아웃(0) 또는 오류.
	}

	for (size_t i = 0; i < pfds_.size(); ++i) {
		const short re = pfds_[i].revents;
		if (re == 0) {
			continue;
		}
		BotSession* b = pmap_[i];

		if (re & (POLLERR | POLLNVAL)) {
			b->onError();
			continue;
		}
		if (re & POLLWRNORM) {
			b->onWritable();
		}
		if (re & (POLLRDNORM | POLLHUP)) {
			b->onReadable();
		}
	}
}

void StressRunner::driveSends(TimePoint now) {
	for (auto& b : bots_) {
		if (b->state() == BotState::InRoom) {
			b->update(now);
		}
	}
}

bool StressRunner::allDone() const {
	if (started_ < cfg_.botCount) {
		return false;
	}
	for (auto& b : bots_) {
		if (b->active()) {
			return false;
		}
	}
	return true;
}

void StressRunner::printStats(TimePoint now, bool finalLine) {
	const Metrics::Snapshot snap = m_.snapshot();
	double dt = secondsBetween(lastStat_, now);
	if (dt <= 0.0) {
		dt = 1.0;
	}

	const double sendPps = static_cast<double>(snap.sendPackets - prev_.sendPackets) / dt;
	const double recvPps = static_cast<double>(snap.recvPackets - prev_.recvPackets) / dt;
	const double sendBps = static_cast<double>(snap.sendBytes - prev_.sendBytes) / dt;
	const double recvBps = static_cast<double>(snap.recvBytes - prev_.recvBytes) / dt;

	int connected = 0;
	int inRoom    = 0;
	for (auto& b : bots_) {
		if (isLive(b->state())) ++connected;
		if (b->state() == BotState::InRoom) ++inRoom;
	}

	const long long sec = static_cast<long long>(secondsBetween(startTime_, now) + 0.5);

	std::printf("[%llds]%s bots=%d connected=%d inRoom=%d sendPps=%.0f recvPps=%.0f "
		"sendBytes=%s recvBytes=%s disconnect=%llu parseErr=%llu connFail=%llu\n",
		sec, finalLine ? " FINAL" : "",
		cfg_.botCount, connected, inRoom, sendPps, recvPps,
		fmtBps(sendBps).c_str(), fmtBps(recvBps).c_str(),
		static_cast<unsigned long long>(snap.disconnect),
		static_cast<unsigned long long>(snap.parseError),
		static_cast<unsigned long long>(snap.connectFail));

	if (csv_) {
		std::fprintf(csv_, "%lld,%d,%d,%d,%.1f,%.1f,%.1f,%.1f,%llu,%llu,%llu,%llu\n",
			sec, cfg_.botCount, connected, inRoom, sendPps, recvPps, sendBps, recvBps,
			static_cast<unsigned long long>(snap.disconnect),
			static_cast<unsigned long long>(snap.parseError),
			static_cast<unsigned long long>(snap.connectFail),
			static_cast<unsigned long long>(snap.npcBatchRecv));
		std::fflush(csv_);
	}

	prev_ = snap;
}

} // namespace hc
