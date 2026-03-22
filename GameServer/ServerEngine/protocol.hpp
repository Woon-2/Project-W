#ifndef protocol_hpp
#define protocol_hpp

#include "macro.hpp"
#include "types.hpp"
#include "../common/mathUtil.hpp"

constexpr const char* serverIp = "127.0.0.1";
constexpr uint16 serverPort = 9000;

enum class PacketType : uint16 {
	C_Enter,
	S_Enter,
};

struct PacketHeader {
	uint16 size;
	PacketType type;
};

template<class T>
class DataList {
public:
	DataList() : data_(nullptr), cnt_(0u) {}
	DataList(T* data, uint16 cnt) : data_(data), cnt_(cnt) {}

	T& operator[](uint16 idx) {
		ASSERT_CRASH(idx < cnt_);
		return data_[idx];
	}

	uint16 count() const { return cnt_; }

private:
	T* data_;
	uint16 cnt_;
};

#pragma pack(push, 1)

struct CEnterPacket : public PacketHeader {

};

struct PlayerInfo {
	uint16 playerId;
	uint16 materialSetIdx;
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 orient;
	DirectX::XMFLOAT3 scale;
};

struct SEnterPacket : public PacketHeader {
	uint16 playerId;
	uint16 playersOffset;	// playerInfos 배열의 시작 위치
	uint16 playerCnt;
};

#pragma pack(pop)

#endif // protocol_hpp