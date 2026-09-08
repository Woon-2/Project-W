#ifndef id_pool_hpp
#define id_pool_hpp

#include <mutex>
#include <unordered_set>

/**
* @brief SingletonBase
*/
class IdPool {
public:
	static constexpr uint32 kMinId = 1u;
	static constexpr uint32 kMaxId = 65535u;

	static void init() {
		// id 0은 "무효/타깃 없음" sentinel로 전역 예약한다(전술 AI의 targetId==0 규약과 일치).
		// 1..65535 발급. 0을 발급하면 플레이어가 id 0을 받아 전술 NPC가 타깃을 "없음"으로
		// 오인해 전원 정지하는 버그가 발생한다.
		for (uint32 i = kMinId; i <= kMaxId; ++i) {
			ASSERT_CRASH(pool_.enqueue(i));
		}
	}

	// 발급 원장(issued_)을 갱신하며 풀 규약을 강제한다. 위반은 콘솔에 보고한다.
	//   - INVALID PUSH  : 발급 범위(1..65535) 밖 반납 → **거부**(id 미할당 객체 반납)
	//   - STRAY PUSH    : 발급된 적 없는 id 반납 → **거부**(다른 id 공간이 섞여 들어옴)
	//   - DUPLICATE POP : 이미 발급된 id가 또 나왔다(= 풀에 같은 값이 두 개, 새 누수 경로)
	// 거부까지 하는 이유는 오염된 id가 풀에 남으면 두 객체가 같은 id를 갖게 되기 때문이다.
	// 배경: RoomServer/docs/objectIdLifecycle.md
	static void push(uint32 id);
	static uint32 pop();

private:
	static ccqueue<uint32> pool_;

	// 현재 발급 중인 id 집합. 발급/반납 빈도는 룸 생성당 수백 회 수준이라
	// 뮤텍스 비용은 무시할 수 있다.
	static std::mutex auditMtx_;
	static std::unordered_set<uint32> issued_;
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