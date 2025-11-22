#ifndef ROOM_MANAGER_HPP
#define ROOM_MANAGER_HPP

struct Level;

class RoomManager {
public:
	static SPRoom findRoom( int32 roomId );
	static SPRoom createRoom( int32 roomId );
	static void removeRoom( int32 roomId );

	static void setLevelData(const Level* pLevel) { pLevel_ = pLevel; }

private:
	static std::mutex mtx_;
	static std::unordered_map<int32, SPRoom> rooms_;
	static const Level* pLevel_;
};

#endif // ROOM_MANAGER_HPP