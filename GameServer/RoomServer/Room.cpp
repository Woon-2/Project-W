#include "rspch.hpp"
#include "Room.hpp"
#include "RoomManager.hpp"
#include "GameSession.hpp"
#include "MemoryManager.hpp"
#include "PacketManager.hpp"
#include "Level.hpp"

void Room::init(const Level* levelData) {
	cubes_ = levelData->cubes;
	playerStarts_ = levelData->playerStarts;
}

void Room::enter(GameSession* session) {
	// 서버에서 사용할 player 객체 세팅
	auto player = session->player();
	player->setModel(RoomManager::playerModelData());
	player->setPos(playerStarts_[sessions_.size() % playerStarts_.size()].pos());	// 새로 들어오는 플레이어는 playerStarts_에서 순서대로 위치를 받는다.
	player->setOrient(playerStarts_[sessions_.size() % playerStarts_.size()].orient());
	player->setScale(playerStarts_[sessions_.size() % playerStarts_.size()].scale());

	// 새로 들어오는 플레이어에 대한 snapshot 만들기
	auto newPlayerInfo = PlayerInfo{
		.playerId = static_cast<uint16>(session->id()),
		.materialSetIdx = 0,
		.pos = player->physicState().pos.getXmf(),
		.orient = player->physicState().orient.getXmf(),
		.scale = player->physicState().scale.getXmf(),
	};

	// 기존 플레이어들 및 기타 object들에 대한 snapshot 만들기
	auto objInfos = std::vector<ObjectInfo>();
	objInfos.reserve(sessions_.size() + cubes_.size());

	for (auto session : sessions_) {
		auto playerInfo = ObjectInfo{
			.type = ObjectType::Player,
			.objectId = static_cast<uint16>(session->id()),
			.materialSetIdx = 0,
			.pos = session->player()->physicState().pos.getXmf(),
			.orient = session->player()->physicState().orient.getXmf(),
			.scale = session->player()->physicState().scale.getXmf(),
		};
		objInfos.emplace_back(playerInfo);
	}

	auto cubeInfo = ObjectInfo{
		.type = ObjectType::Ground,
		.objectId = static_cast<uint16>(IdPool::pop()),	// cube는 objectId가 필요하긴 하지만, 클라이언트에서 cube에 대한 패킷은 따로 없으므로 일단 IdPool에서 pop해서 사용한다. 나중에 개선 필요.
		.materialSetIdx = 0,
		.pos = cubes_[0].physicState().pos.getXmf(),
		.orient = cubes_[0].physicState().orient.getXmf(),
		.scale = cubes_[0].physicState().scale.getXmf(),
	};
	objInfos.emplace_back(cubeInfo);

	// 패킷 생성 후 새로 들어온 플레이어에게 전송
	auto enterPkt = PacketManager::makeSEnterPacket(newPlayerInfo, objInfos);
	session->send(enterPkt);

	// 새로 들어온 플레이어의 정보를 기존 플레이어들에게 브로드캐스트
	if (sessions_.size() > 0) {	// 기존 플레이어가 있을 때만 브로드캐스트
		auto enterOtherPkt = PacketManager::makeSEnterOtherPacket(newPlayerInfo);
		broadcast(enterOtherPkt);
	}

	// room 상태 변경
	sessions_.push_back(session);
	idSessionMap_[session->id()] = session;
}

void Room::leave(GameSession* session) {
	std::erase_if(sessions_, [session](GameSession* s) { return s == session; });
	idSessionMap_.erase(session->id());

	auto leavePkt = PacketManager::makeSLeavePacket(static_cast<uint16>(session->id()));
	broadcast(leavePkt);

	odelete(session);

	if (sessions_.size() == 0) {
		RoomManager::removeRoom(id_);
	}
}

void Room::broadcast(SendBuffer* sendBuffer) {
	for(auto session : sessions_) {
		session->send(sendBuffer);
	}
}
