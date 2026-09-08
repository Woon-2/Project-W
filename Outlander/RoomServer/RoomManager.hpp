#ifndef room_manager_hpp
#define room_manager_hpp

#include <vector>

class Room;
class Job;
struct Level;
struct Model;
struct ServerAnimClip;

/**
* @brief SingletonBase
*/
class RoomManager {
public:
	// lobbyCode로 방을 찾고 없으면 생성한다(원자적). 같은 코드 파티가 같은 Room에 모인다.
	static Room* findOrCreateRoomByCode(const std::string& code);
	static Room* findRoom(int32 roomId);
	// 방을 맵에서만 제거한다. 실제 파괴(풀 반납)는 sweepPendingRooms가 JobQueue 유휴를
	// 확인한 뒤에 한다 — 즉시 파괴하면 ~Room()이 자기 execute() 콜스택 안에서 돌아
	// 잔여 잡/큐 접근이 전부 UAF가 된다. docs/serverHandoff.md §4-P0 D.
	static void removeRoom(int32 roomId);
	// 지연 파괴 reaper. JobTimer 스레드가 주기적으로 호출한다.
	static void sweepPendingRooms();
	// roomId 조회와 잡 push를 rmMtx_ 한 락 안에서 수행한다(조회-사용 사이 파괴 방지).
	// 방이 없으면 false를 반환하고 job은 건드리지 않는다(호출자가 풀로 반납할 것).
	static bool postJob(int32 roomId, Job* job);

	static void setLevelData(const Level* pLevel) { pLevel_ = pLevel; }
	static void setPlayerModelData(const Model* pModel) { pPlayerModel_ = pModel; }
	static const Model* playerModelData() { return pPlayerModel_; }

	static void setPlayerAnimations(const std::vector<ServerAnimClip>* p) { pPlayerAnimations_ = p; }
	static const std::vector<ServerAnimClip>* playerAnimations() { return pPlayerAnimations_; }

private:
	struct PendingRoom {
		Room* room;
		int32 idleSweeps;   // 연속 유휴 관측 횟수. 2회 이상이면 반납한다.
	};

	static std::mutex rmMtx_;
	static std::vector<Room*> rooms_;
	static std::unordered_map<int32, Room*> roomIdMap_;
	static std::unordered_map<std::string, Room*> codeRoomMap_;
	// 맵에서 빠졌지만 아직 파괴하면 안 되는 방들(rmMtx_ 보호). sweepPendingRooms가 비운다.
	static std::vector<PendingRoom> pendingDestroy_;

	static const Level*  pLevel_;
	static const Model*  pPlayerModel_;
	static const std::vector<ServerAnimClip>* pPlayerAnimations_;
};

#endif // room_manager_hpp