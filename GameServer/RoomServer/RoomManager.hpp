#ifndef room_manager_hpp
#define room_manager_hpp

#include <vector>

class Room;
struct Level;
struct Model;
struct ServerAnimClip;

/**
* @brief SingletonBase
*/
class RoomManager {
public:
	static Room* makeRoom();
	static Room* findRoom(int32 roomId);
	static void removeRoom(int32 roomId);

	static void setLevelData(const Level* pLevel) { pLevel_ = pLevel; }
	static void setPlayerModelData(const Model* pModel) { pPlayerModel_ = pModel; }
	static const Model* playerModelData() { return pPlayerModel_; }

	static void setPlayerAnimations(const std::vector<ServerAnimClip>* p) { pPlayerAnimations_ = p; }
	static const std::vector<ServerAnimClip>* playerAnimations() { return pPlayerAnimations_; }

private:
	static std::mutex rmMtx_;
	static std::vector<Room*> rooms_;
	static std::unordered_map<int32, Room*> roomIdMap_;

	static const Level*  pLevel_;
	static const Model*  pPlayerModel_;
	static const std::vector<ServerAnimClip>* pPlayerAnimations_;
};

#endif // room_manager_hpp