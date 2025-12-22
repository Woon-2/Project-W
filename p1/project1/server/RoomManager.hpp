#ifndef ROOM_MANAGER_HPP
#define ROOM_MANAGER_HPP

struct Level;
struct Model;

class RoomManager {
public:
	static SPRoom findRoom( int32 roomId );
	static SPRoom createRoom( int32 roomId );
	static void removeRoom( int32 roomId );

	static void setLevelData(const Level* pLevel) { pLevel_ = pLevel; }
	static void setPlayerModelData(const Model* pModel) { pPlayerModel_ = pModel; }
	static const Model* playerModelData() { return pPlayerModel_; }

private:
	static std::mutex mtx_;
	static std::unordered_map<int32, SPRoom> rooms_;
	static const Level* pLevel_;
	static const Model* pPlayerModel_;
};

#endif // ROOM_MANAGER_HPP