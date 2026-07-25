#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct DbConfig {
    std::wstring connectionString{};   // ODBC 연결 문자열 (SQLDriverConnect에 그대로 전달)
    std::int32_t connectionCount = 0;  // 커넥션 풀 크기
};

// exe 디렉터리에서 부모로 거슬러 올라가며 가장 가까운 db_config.json을 찾아 파싱한다.
// 폴백 값 없음: false면 프로세스는 에러를 출력하고 종료해야 한다. (networkConfig와 동일 규약)
//
// 서버 전용: 이 파일은 클라이언트 프로젝트에 추가하지 않는다.
// DB 접속 정보가 클라이언트 배포에 섞이는 것을 막기 위해 network_config.json과 분리했다.
bool loadDbConfig(DbConfig& out, std::filesystem::path& loadedPath, std::string& error);
