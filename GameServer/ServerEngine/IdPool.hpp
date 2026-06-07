#ifndef id_pool_hpp
#define id_pool_hpp

/**
* @brief SingletonBase
*/
class IdPool {
public:
	static void init() {
		constexpr uint32 maxVal = std::numeric_limits<uint16>::max() + 1;
		// id 0은 "무효/타깃 없음" sentinel로 전역 예약한다(전술 AI의 targetId==0 규약과 일치).
		// 1..65535 발급. 0을 발급하면 플레이어가 id 0을 받아 전술 NPC가 타깃을 "없음"으로
		// 오인해 전원 정지하는 버그가 발생한다.
		for (uint32 i = 1; i < maxVal; ++i) {
			ASSERT_CRASH(pool_.enqueue(i));
		}
	}

	static void push(uint32 id) {
		ASSERT_CRASH(pool_.enqueue(id));
	}

	static uint32 pop() {
		uint32 id{};
		ASSERT_CRASH(pool_.try_dequeue(id));
		return id;
	}

private:
	static ccqueue<uint32> pool_;
};

/**
* @brief SingletonBase / temporary
*/
class RoomIdPool {
public:
	static void init() {
		constexpr uint32 maxVal = std::numeric_limits<uint16>::max() + 1;
		for (uint32 i = 0; i < maxVal; ++i) {
			ASSERT_CRASH(pool_.enqueue(i));
		}
	}

	static void push(uint32 id) {
		ASSERT_CRASH(pool_.enqueue(id));
	}

	static uint32 pop() {
		uint32 id{};
		ASSERT_CRASH(pool_.try_dequeue(id));
		currId_ = id;
		return id;
	}

	static uint32 currId() {
		return currId_;
	}

private:
	static ccqueue<uint32> pool_;
	static std::atomic_int32_t currId_;
};

#endif // id_pool_hpp