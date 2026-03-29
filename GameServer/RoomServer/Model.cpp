#include "rspch.hpp"
#include "Model.hpp"
#include "binaryImport.hpp"

void importBoundingBox(std::ifstream& ifs, Model& model) {
    auto& aabb = model.aabbs.emplace_back();

    const auto bvName = readText(ifs, "Name");
    model.aabbIdxMap.try_emplace(bvName, static_cast<int>(model.aabbs.size() - 1u));

    const auto localMatrix = readMatrix(ifs, "LocalMatrix");

    readHeadTag(ifs, "LocalTRS");
    const auto localT = readVec3(ifs, "Position");
    const auto localR = readVec4(ifs, "Rotation");
    const auto localS = readVec3(ifs, "Scale");
    readTailTag(ifs, "LocalTRS");

    const auto center = readVec3(ifs, "Center");
    aabb.center = DirectX::XMLoadFloat3(&center);
    const auto size = readVec3(ifs, "Size");
    aabb.size = DirectX::XMLoadFloat3(&size);
}

void importBoundingRect(std::ifstream& ifs, Model& model) {
    auto& rect = model.boundingRects.emplace_back();

    const auto bvName = readText(ifs, "Name");
    model.boundingRectIdxMap.try_emplace(
        bvName, static_cast<int>(model.boundingRects.size() - 1u)
    );

    const auto center = readVec2(ifs, "Center");
    rect.center = DirectX::XMLoadFloat2(&center);
    const auto size = readVec2(ifs, "Size");
    rect.size = DirectX::XMLoadFloat2(&size);
    const auto halfSize = XMFLOAT2(size.x * 0.5f, size.y * 0.5f);
}

// 모델의 바운딩 볼륨 하나의 정보를 읽어온다.
void importBoundingVolume(std::ifstream& ifs, Model& model) {
    readHeadTag(ifs, "BoundingVolume");
    const auto type = readText(ifs, "Type");
    if (type == "Box") {
        importBoundingBox(ifs, model);
    }
    else if (type == "Rect") {
        importBoundingRect(ifs, model);
    }
    readTailTag(ifs, "BoundingVolume");
}

// 모델의 바운딩 볼륨 정보를 읽어온다.
// 모델에는 여러 개의 바운딩 볼륨이 존재할 수 있다.
void importBoundingVolumes(std::ifstream& ifs, Model& model) {
    readHeadTag(ifs, "BoundingVolumes");

    const auto bvCnt = readInteger(ifs, "Count");

    for (int i = 0; i < bvCnt; ++i) {
        importBoundingVolume(ifs, model);
    }

    readTailTag(ifs, "BoundingVolumes");
}

// 바이너리 파일로부터 모델을 읽어온다.
// 메시들을 생성하며 각 메시들의 버텍스 버퍼와 서브메시, 그리고 재질 집합을 생성한다.
// 그 과정에서 필요한 텍스처들이 texHashMap에 존재하지 않는다면, 로드한다.
// (로드되는 텍스처의 경로들은 바이너리 파일 내에 적혀있다.)
// * 수정 시 주의사항: 유니티의 추출 스크립트와 구조가 대칭이어야 한다.
Model loadModelFromFile(const std::filesystem::path& path) {
    Model ret{};

    auto ifs = std::ifstream(path, std::ios::binary);
    DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadModelFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, false);
    if (!ifs) {
        return ret;
    }

    ret.name = readText(ifs, "ModelName");
    importBoundingVolumes(ifs, ret);
    gSharedLog << "[Resource Load] File I/O: 모델 " << ret.name << '(' << path << ") 로드 완료\n";

    return ret;
}
