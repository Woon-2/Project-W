#ifndef ROOM_MANAGER_HPP
#define ROOM_MANAGER_HPP

class RoomManager {
public:
	static SPRoom findRoom( int32 roomId );
	static SPRoom createRoom( int32 roomId );
	static void removeRoom( int32 roomId );

private:
	static std::mutex mtx_;
	static std::unordered_map<int32, SPRoom> rooms_;
};

#endif // ROOM_MANAGER_HPP