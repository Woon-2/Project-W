#ifndef client_packet_manager_hpp
#define client_packet_manager_hpp

namespace Online { class Game; }
class SendBuffer;

class PacketManager {
public:
	static void handlePacket(byte* buffer, int32 len);
	static void handleSEnterPacket(byte* buffer, int32 len);
	static void handleSEnterOtherPacket(byte* buffer, int32 len);
	static void handleSLeavePacket(byte* buffer, int32 len);
	static void handleSMovePacket(byte* buffer, int32 len);
	static void handleSMouseMovePacket(byte* buffer, int32 len);

	static std::shared_ptr<SendBuffer> makeCMovePacket(DirectX::XMFLOAT3 pos);
	static std::shared_ptr<SendBuffer> makeCMouseMovePacket(float yawRad);
};

#endif // client_packet_manager_hpp