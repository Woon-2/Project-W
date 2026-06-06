#include "rspch.hpp"
#include "Model.hpp"
#include "binaryImport.hpp"

// --- BVH build helpers ---

struct ImportedBVBox {
    std::string name;
    std::string boneName;
    DirectX::XMFLOAT3 center;
    DirectX::XMFLOAT3 size;       // full extents (width, height, depth)
    DirectX::XMFLOAT3 rotEuler;   // degrees
    bool              isStatic = false;  // Unity GameObject static flag
    float             damageCoeff = 1.0f;
};

static bool isZeroEuler(const DirectX::XMFLOAT3& e) {
    return std::abs(e.x) < 1e-4f && std::abs(e.y) < 1e-4f && std::abs(e.z) < 1e-4f;
}

static mu::NQuat eulerDegsToQuat(const DirectX::XMFLOAT3& e) {
    constexpr float toRad = DirectX::XM_PI / 180.f;
    return mu::NQuat(mu::Degree(e.z), mu::Degree(e.x), mu::Degree(e.y));
}

static AABB computeNodeBounds(const std::variant<AABB, OBB>& shape) {
    return std::visit([](auto&& s) -> AABB {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, OBB>) return obbToAABB(s);
        else                                   return s;
        }, shape);
}

static AABB unionAABB(const AABB& a, const AABB& b) {
    const mu::Vec3 minA = a.center - a.size * 0.5f;
    const mu::Vec3 maxA = a.center + a.size * 0.5f;
    const mu::Vec3 minB = b.center - b.size * 0.5f;
    const mu::Vec3 maxB = b.center + b.size * 0.5f;
    const mu::Vec3 mn = mu::min(minA, minB);
    const mu::Vec3 mx = mu::max(maxA, maxB);
    return AABB{(mn + mx) * 0.5f, mx - mn};
}

static BVHNode makeBVHNode(const ImportedBVBox& box) {
    BVHNode node;
    node.name = box.name;
    node.boneName = box.boneName;

    const mu::Vec3 center = DirectX::XMLoadFloat3(&box.center);
    const mu::Vec3 size = DirectX::XMLoadFloat3(&box.size);

    // static이면서 AABB로 표현 가능(로컬 회전이 0)한 박스만 AABB로 로드한다.
    // 그 외(애니메이션 본에 붙은 박스, 회전이 있는 박스, 동적 오브젝트)는 OBB로 로드해
    // 월드 회전이 충돌 판정에 정확히 반영되도록 한다.
    if (box.isStatic && isZeroEuler(box.rotEuler)) {
        node.shape = AABB{center, size};
    }
    else {
        node.shape = OBB{center, size * 0.5f, eulerDegsToQuat(box.rotEuler)};
    }
    node.bounds      = computeNodeBounds(node.shape);
    node.damageCoeff = box.damageCoeff;
    return node;
}

// Builds a BVH from LOD-based box arrays.
// LOD 0 = root level (1 coarse box expected), LOD 1 = sub-BVs, LOD 2 = finer sub-BVs.
// Each LOD N+1 box is assigned to the nearest center LOD N parent.
static BVH buildBVHFromLODs(const std::vector<std::vector<ImportedBVBox>>& allLODs) {
    BVH bvh;
    if (allLODs.empty() || allLODs[0].empty()) return bvh;

    if (allLODs.size() == 1) {
        for (const auto& box : allLODs[0])
            bvh.nodes.push_back(makeBVHNode(box));
        return bvh;
    }

    // Build LOD 0 root nodes first (indices 0 .. n0-1)
    for (const auto& box : allLODs[0])
        bvh.nodes.push_back(makeBVHNode(box));

    int parentStart = 0;
    int parentEnd = (int)bvh.nodes.size();

    for (int lod = 1; lod < (int)allLODs.size(); ++lod) {
        const int childStart = (int)bvh.nodes.size();

        for (const auto& box : allLODs[lod]) {
            BVHNode child = makeBVHNode(box);

            const mu::Vec3 childCenter = std::visit(
                [](auto&& s) { return s.center; }, child.shape);

            int bestParent = parentStart;
            float bestDist = std::numeric_limits<float>::max();
            for (int p = parentStart; p < parentEnd; ++p) {
                const mu::Vec3 pCenter = std::visit(
                    [](auto&& s) { return s.center; }, bvh.nodes[p].shape);
                const float d = (childCenter - pCenter).len2();
                if (d < bestDist) { bestDist = d; bestParent = p; }
            }

            bvh.nodes[bestParent].children.push_back((int)bvh.nodes.size());
            bvh.nodes.push_back(std::move(child));
        }

        // Expand parent bounds to encompass new children
        for (int p = parentStart; p < parentEnd; ++p) {
            for (int c : bvh.nodes[p].children)
                bvh.nodes[p].bounds = unionAABB(bvh.nodes[p].bounds, bvh.nodes[c].bounds);
        }

        parentStart = childStart;
        parentEnd = (int)bvh.nodes.size();
    }

    return bvh;
}

// 모델의 바운딩 볼륨 하나의 정보를 읽어온다.
void importBoundingVolumes(std::ifstream& ifs, Model& model) {
    readHeadTag(ifs, "BoundingVolumes");

    const int lodCount = readInteger(ifs, "LODCount");
    if (lodCount == 0) {
        readTailTag(ifs, "BoundingVolumes");
        return;
    }

    std::vector<std::vector<ImportedBVBox>> allLODs(lodCount);

    for (int lod = 0; lod < lodCount; ++lod) {
        readHeadTag(ifs, "LOD");
        const int lodIdx = readInteger(ifs, "Index");
        const int boxCount = readInteger(ifs, "BoxCount");

        const int target = (lodIdx >= 0 && lodIdx < lodCount) ? lodIdx : lod;
        allLODs[target].reserve(boxCount);

        for (int b = 0; b < boxCount; ++b) {
            readHeadTag(ifs, "Box");
            ImportedBVBox box;
            box.name        = readText(ifs, "Name");
            box.boneName    = readText(ifs, "Bone");
            box.center      = readVec3(ifs, "Center");
            box.size        = readVec3(ifs, "Size");
            box.rotEuler    = readVec3(ifs, "Rotation");
            box.isStatic    = readInteger(ifs, "IsStatic") != 0;
            box.damageCoeff = readFloat(ifs, "DamageCoeff");
            allLODs[target].push_back(box);
            readTailTag(ifs, "Box");

            gSharedLog << "[Resource Load] BV box " << box.name << " (LOD " << target << ")\n";
        }

        readTailTag(ifs, "LOD");
    }

    model.bvh = buildBVHFromLODs(allLODs);
    gSharedLog << "[Resource Load] BVH 구축 완료 - " << model.bvh.nodes.size() << " nodes\n";

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

    const bool hasSkeleton = static_cast<bool>(readInteger(ifs, "HasSkeleton"));
    if (hasSkeleton)
        importSkeleton(ifs, ret.skeleton);

    for (auto& node : ret.bvh.nodes) {
        if (!node.boneName.empty()) {
            auto it = ret.skeleton.nameToIdx.find(node.boneName);
            if (it != ret.skeleton.nameToIdx.end())
                node.boneIdx = it->second;
        }
    }

    gSharedLog << "[Resource Load] File I/O: 모델 " << ret.name << '(' << path << ") 로드 완료\n";

    return ret;
}
