#ifndef headless_stress_config_hpp
#define headless_stress_config_hpp

#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace hc {

// 부하 테스트 설정. 기본값은 코드 상수, command-line 인자로 override.
struct StressConfig {
	std::string ip            = "127.0.0.1";
	uint16_t    port          = 9000;        // RoomServer 기본 포트(roomServerPort)
	int         botCount      = 200;
	int         playersPerRoom = 4;
	int         movePacketHz  = 20;          // 실제 클라와 동일한 이동 패킷 주기
	bool        enableAttack  = false;       // 확장 슬롯(미구현)
	double      attackPerSecond = 1.0;
	bool        enableReconnect = false;     // 확장 슬롯(미구현)
	int         testDurationSec = 60;        // 0 = 무한
	double      connectPerSecond = 100.0;    // ramp-up 속도(0 이하 = 한 번에 전부)

	// BotBehavior(원형 이동) 파라미터.
	float       moveRadius       = 3.0f;
	float       moveAngularSpeed = 1.5f;     // rad/s

	std::string csvPath;                     // 비어있지 않으면 통계를 CSV로도 저장.
};

// 아주 단순한 "--key value" 파서. 알 수 없는 키는 무시(경고는 호출부에서).
inline bool parseArgs(int argc, char** argv, StressConfig& c, std::string& err) {
	auto needVal = [&](int i) -> bool {
		if (i + 1 >= argc) { err = std::string("missing value for ") + argv[i]; return false; }
		return true;
	};

	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if      (a == "--ip")              { if (!needVal(i)) return false; c.ip = argv[++i]; }
		else if (a == "--port")            { if (!needVal(i)) return false; c.port = static_cast<uint16_t>(std::atoi(argv[++i])); }
		else if (a == "--bots")            { if (!needVal(i)) return false; c.botCount = std::atoi(argv[++i]); }
		else if (a == "--playersPerRoom")  { if (!needVal(i)) return false; c.playersPerRoom = std::atoi(argv[++i]); }
		else if (a == "--moveHz")          { if (!needVal(i)) return false; c.movePacketHz = std::atoi(argv[++i]); }
		else if (a == "--duration")        { if (!needVal(i)) return false; c.testDurationSec = std::atoi(argv[++i]); }
		else if (a == "--connectPerSec")   { if (!needVal(i)) return false; c.connectPerSecond = std::atof(argv[++i]); }
		else if (a == "--attackPerSec")    { if (!needVal(i)) return false; c.attackPerSecond = std::atof(argv[++i]); }
		else if (a == "--radius")          { if (!needVal(i)) return false; c.moveRadius = static_cast<float>(std::atof(argv[++i])); }
		else if (a == "--angular")         { if (!needVal(i)) return false; c.moveAngularSpeed = static_cast<float>(std::atof(argv[++i])); }
		else if (a == "--csv")             { if (!needVal(i)) return false; c.csvPath = argv[++i]; }
		else if (a == "--enableAttack")    { c.enableAttack = true; }
		else if (a == "--enableReconnect") { c.enableReconnect = true; }
		else if (a == "--help" || a == "-h") { err = "help"; return false; }
		else { err = std::string("unknown argument: ") + a; return false; }
	}

	if (c.botCount < 1)        c.botCount = 1;
	if (c.playersPerRoom < 1)  c.playersPerRoom = 1;
	if (c.movePacketHz < 1)    c.movePacketHz = 1;
	return true;
}

inline const char* usageText() {
	return
		"HeadlessClient - RoomServer load/stress test bot\n"
		"Usage: HeadlessClient.exe [options]\n"
		"  --ip <addr>            server ip (default 127.0.0.1)\n"
		"  --port <n>             server port (default 9000)\n"
		"  --bots <n>             total bot count (default 8)\n"
		"  --playersPerRoom <n>   bots per room (default 4)\n"
		"  --moveHz <n>           C_Move send rate (default 20)\n"
		"  --duration <sec>       test duration, 0=infinite (default 60)\n"
		"  --connectPerSec <n>    ramp-up connect rate (default 100)\n"
		"  --radius <f>           circular move radius (default 3.0)\n"
		"  --angular <f>          circular move angular speed rad/s (default 1.5)\n"
		"  --csv <path>           also write per-second stats to CSV\n"
		"  --enableAttack         (reserved, not implemented yet)\n"
		"  --enableReconnect      (reserved, not implemented yet)\n";
}

} // namespace hc

#endif // headless_stress_config_hpp
