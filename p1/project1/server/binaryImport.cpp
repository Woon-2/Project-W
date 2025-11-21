#include "pch.hpp"
#include "binaryImport.hpp"

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
void readHeadTag(std::ifstream& ifs, const std::string& expectedSource) {
    char tmpBuffer[32]{'\0'};
    unsigned char sz{};

    const auto expected = "<"s + expectedSource + ":>"s;
    ifs.read(reinterpret_cast<char*>(&sz), sizeof(unsigned char));
    ifs.read(tmpBuffer, sz);

    std::string wExpected{};
    wExpected.assign(expected.begin(), expected.end());

    std::string wReceived{};
    wReceived.assign(tmpBuffer, tmpBuffer + sz);

    DISPLAY_ERROR_STR(expected == tmpBuffer,
        "[File I/O Error] readHeadTag: "s + wExpected + " 토큰을 기대했지만 "s
        + wReceived + " 토큰을 받았습니다.", true
    );
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
void readTailTag(std::ifstream& ifs, const std::string& expectedSource) {
    char tmpBuffer[32]{'\0'};
    unsigned char sz{};

    const auto expected = "</"s + expectedSource + ">"s;
    ifs.read(reinterpret_cast<char*>(&sz), sizeof(unsigned char));
    ifs.read(tmpBuffer, sz);

    std::string wExpected{};
    wExpected.assign(expected.begin(), expected.end());

    std::string wReceived{};
    wReceived.assign(tmpBuffer, tmpBuffer + sz);

    DISPLAY_ERROR_STR(expected == tmpBuffer,
        "[File I/O Error] readTailTag: "s + wExpected + " 토큰을 기대했지만 "s
        + wReceived + " 토큰을 받았습니다.", true
    );
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
bool isTailTag(const std::string& str, const std::string& expectedSource) {
    const auto expected = "</"s + expectedSource + ">"s;
    return str == expected;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::string readString(std::ifstream& ifs) {
    char tmpBuffer[64]{'\0'};
    unsigned char sz{};

    ifs.read(reinterpret_cast<char*>(&sz), sizeof(unsigned char));
    ifs.read(tmpBuffer, sz);

    return std::string(tmpBuffer, tmpBuffer + sz);
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::string readText(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    auto ret = readString(ifs);
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::string untagHead(const std::string& tag) {
    return tag.substr(1u, tag.size() - 1u - 2u);
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
int readInteger(std::ifstream& ifs) {
    int ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(int));
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
int readInteger(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    int ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(int));
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<int> readIntegers(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    auto cnt = readInteger(ifs, "Cnt");
    auto ret = std::vector<int>(cnt);
    for (int i = 0; i < cnt; ++i) {
        ifs.read(reinterpret_cast<char*>(&ret[i]), sizeof(int));
    }
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<u16t> readU16s(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    auto cnt = readInteger(ifs, "Cnt");
    auto ret = std::vector<u16t>(cnt);
    for (int i = 0; i < cnt; ++i) {
        ifs.read(reinterpret_cast<char*>(&ret[i]), sizeof(u16t));
    }
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
float readFloat(std::ifstream& ifs) {
    float ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(float));
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
float readFloat(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    float ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(float));
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT4 readColor(std::ifstream& ifs) {
    XMFLOAT4 ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(XMFLOAT4));
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT4X4 readMatrix(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    XMFLOAT4X4 ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(XMFLOAT4X4));
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT2 readVec2(std::ifstream& ifs) {
    XMFLOAT2 ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(XMFLOAT2));
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT2 readVec2(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    auto ret = readVec2(ifs);
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT3 readVec3(std::ifstream& ifs) {
    XMFLOAT3 ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(XMFLOAT3));
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT3 readVec3(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    auto ret = readVec3(ifs);
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT4 readVec4(std::ifstream& ifs) {
    XMFLOAT4 ret{};
    ifs.read(reinterpret_cast<char*>(&ret), sizeof(XMFLOAT4));
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
XMFLOAT4 readVec4(std::ifstream& ifs, const char* tagSource) {
    readHeadTag(ifs, tagSource);
    auto ret = readVec4(ifs);
    readTailTag(ifs, tagSource);
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<XMFLOAT2> readVec2s(std::ifstream& ifs) {
    auto cnt = readInteger(ifs, "Cnt");
    auto ret = std::vector<XMFLOAT2>(cnt);
    for (int i = 0; i < cnt; ++i) {
        ifs.read(reinterpret_cast<char*>(&ret[i]), sizeof(XMFLOAT2));
    }
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<XMFLOAT3> readVec3s(std::ifstream& ifs) {
    auto cnt = readInteger(ifs, "Cnt");
    auto ret = std::vector<XMFLOAT3>(cnt);
    for (int i = 0; i < cnt; ++i) {
        ifs.read(reinterpret_cast<char*>(&ret[i]), sizeof(XMFLOAT3));
    }
    return ret;
}

// 바이너리 파일에서 데이터를 읽는데 쓰이는 유틸리티 함수
std::vector<XMFLOAT4> readVec4s(std::ifstream& ifs) {
    auto cnt = readInteger(ifs, "Cnt");
    auto ret = std::vector<XMFLOAT4>(cnt);
    for (int i = 0; i < cnt; ++i) {
        ifs.read(reinterpret_cast<char*>(&ret[i]), sizeof(XMFLOAT4));
    }
    return ret;
}