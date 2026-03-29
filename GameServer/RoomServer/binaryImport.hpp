#ifndef room_server_binaryImport_hpp
#define room_server_binaryImport_hpp

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
void readHeadTag(std::ifstream& ifs, const std::string& expectedSource);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
void readTailTag(std::ifstream& ifs, const std::string& expectedSource);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
bool isTailTag(const std::string& str, const std::string& expectedSource);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::string readString(std::ifstream& ifs);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::string readText(std::ifstream& ifs, const char* tagSource);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::string untagHead(const std::string& tag);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
int readInteger(std::ifstream& ifs);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
int readInteger(std::ifstream& ifs, const char* tagSource);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<int> readIntegers(std::ifstream& ifs, const char* tagSource);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<uint16> readU16s(std::ifstream& ifs, const char* tagSource);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
float readFloat(std::ifstream& ifs);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
float readFloat(std::ifstream& ifs, const char* tagSource);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT4 readColor(std::ifstream& ifs);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT4X4 readMatrix(std::ifstream& ifs, const char* tagSource);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT2 readVec2(std::ifstream& ifs);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT2 readVec2(std::ifstream& ifs, const char* tagSource);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT3 readVec3(std::ifstream& ifs);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT3 readVec3(std::ifstream& ifs, const char* tagSource);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT4 readVec4(std::ifstream& ifs);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT4 readVec4(std::ifstream& ifs, const char* tagSource);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<XMFLOAT2> readVec2s(std::ifstream& ifs);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<XMFLOAT3> readVec3s(std::ifstream& ifs);

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<XMFLOAT4> readVec4s(std::ifstream& ifs);

#endif // room_server_binaryImport_hpp