#ifndef room_manager_hpp
#define room_manager_hpp

class Room;

/**
* @brief SingletonBase
*/
class RoomManager {
public:
	static void addRoom(Room* room);
	static void removeRoom(int32 roomId);

	static Room* getRoom(int32 roomId);

private:
	static std::mutex rmMtx_;
	static std::vector<Room*> rooms_;
	static std::unordered_map<int32, Room*> roomIdMap_;
};

#endif // room_manager_hpp