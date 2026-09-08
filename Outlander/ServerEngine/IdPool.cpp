#include "sepch.hpp"
#include "IdPool.hpp"

ccqueue<uint32> IdPool::pool_;
ccqueue<uint32> RoomIdPool::pool_;
std::atomic_int32_t RoomIdPool::currId_{-1};

std::mutex IdPool::auditMtx_;
std::unordered_set<uint32> IdPool::issued_;

// Return an id to the pool.
//
// The pool defends itself: an id that this pool never issued is REJECTED instead of
// enqueued. Letting a foreign id in is not a cosmetic problem -- the pool would then
// hold the same value twice and hand one live object's id to another, which shows up
// as a monster that cannot be damaged, a spawn silently swallowed by the client's
// idempotency guard, or a player whose skill VFX plays on top of a monster.
// This actually happened: ~Room() used to return the room id (a RoomIdPool value)
// here. See RoomServer/docs/objectIdLifecycle.md.
void IdPool::push(uint32 id) {
	{
		std::lock_guard<std::mutex> lock(auditMtx_);

		if (id < kMinId || id > kMaxId) {
			std::cout << "[IdPool] INVALID PUSH id=" << id
				<< " (outside " << kMinId << ".." << kMaxId
				<< "; object never received an id?) -- REJECTED\n";
			return;
		}
		if (issued_.erase(id) == 0) {
			std::cout << "[IdPool] STRAY PUSH id=" << id
				<< " (never issued by this pool -- foreign id space) -- REJECTED\n";
			return;
		}
	}

	ASSERT_CRASH(pool_.enqueue(id));
}

// Take an id from the pool. A DUPLICATE POP means the pool held the same value twice,
// so two live objects are about to share one id. push() rejects the known ways that
// can happen, so this firing means a new leak path exists -- do not ignore it.
uint32 IdPool::pop() {
	uint32 id{};
	ASSERT_CRASH(pool_.try_dequeue(id));

	{
		std::lock_guard<std::mutex> lock(auditMtx_);

		if (!issued_.insert(id).second) {
			std::cout << "[IdPool] DUPLICATE POP id=" << id
				<< " (already issued -- two objects will share this id)\n";
		}
		if (id < kMinId || id > kMaxId) {
			std::cout << "[IdPool] INVALID POP id=" << id
				<< " (outside " << kMinId << ".." << kMaxId << ")\n";
		}
	}

	return id;
}
