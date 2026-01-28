#ifndef id_pool_hpp
#define id_pool_hpp

class IdPool {
public:
	static void init() {
		constexpr auto maxValue = std::numeric_limits<uint16>::max();
		for (int32 i = 1; i <= maxValue; ++i) {
			idQueue_.enqueue(i);
		}
	}

	static void push(uint32 id) {
		ASSERT_CRASH(idQueue_.enqueue(id));
	}

	static uint32 pop() {
		uint32 id{};
		ASSERT_CRASH(idQueue_.try_dequeue(id));
		return id;
	}

private:
	static ccqueue<uint32> idQueue_;
};

#endif	// id_pool_hpp