#include "Common.hpp"
#include "StressConfig.hpp"
#include "StressRunner.hpp"

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
	hc::StressConfig cfg;
	std::string err;
	if (!hc::parseArgs(argc, argv, cfg, err)) {
		if (err == "help") {
			std::printf("%s", hc::usageText());
			return 0;
		}
		std::printf("argument error: %s\n\n%s", err.c_str(), hc::usageText());
		return 1;
	}

	WSADATA wsa{};
	const int wr = ::WSAStartup(MAKEWORD(2, 2), &wsa);
	if (wr != 0) {
		std::printf("WSAStartup failed: %d\n", wr);
		return 1;
	}

	int rc = 0;
	{
		hc::StressRunner runner(cfg);
		rc = runner.run();
	}

	::WSACleanup();
	return rc;
}
