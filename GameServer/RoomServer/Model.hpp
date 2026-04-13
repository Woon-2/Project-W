#ifndef room_server_model_hpp
#define room_server_model_hpp

#include "collision.hpp"

// 서버에서 게임 객체의 모델과 관련하여 알아야 할 정보는 현재
// 이름과 충돌체 정보 뿐이다.
struct Model {
	std::string name;
	BVH bvh;
};

// 바이너리 파일로부터 모델을 읽어온다.
// * 수정 시 주의사항: 유니티의 추출 스크립트와 구조가 대칭이어야 한다.
Model loadModelFromFile(const std::filesystem::path& path);

#endif // room_server_model_hpp