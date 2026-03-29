#ifndef room_manager_hpp
#define room_manager_hpp

class Room;
struct Level;
struct Model;

/**
* @brief SingletonBase
*/
class RoomManager {
public:
	static Room* makeRoom();
	static Room* getRoom(int32 roomId);
	static void removeRoom(int32 roomId);

	static void setLevelData(const Level* pLevel) { pLevel_ = pLevel; }
	static void setPlayerModelData(const Model* pModel) { pPlayerModel_ = pModel; }
	static const Model* playerModelData() { return pPlayerModel_; }

private:
	static std::mutex rmMtx_;
	static std::vector<Room*> rooms_;
	static std::unordered_map<int32, Room*> roomIdMap_;

	static const Level* pLevel_;
	static const Model* pPlayerModel_;
};

#endif // room_manager_hpp