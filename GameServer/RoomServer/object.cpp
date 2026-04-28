#include "rspch.hpp"
#include "object.hpp"
#include "Model.hpp"
#include "GameSession.hpp"

void Object::setModel(const Model* pModel){
	DISPLAY_ERROR_STR(pModel != nullptr, "[Game Error] Object::setModel: null model.", false);
	if (pModel == nullptr) {
		return;
	}

	pModel_ = pModel;
	rebuildBodyBVH();
}

void Object::update(Milliseconds deltaTime) {

}

void MU_CALLCONV Object::setPos(mu::Vec3 newPos) {
	body_.setPos(newPos);

	if (pModel_ && !pModel_->bvh.empty()) {
		rebuildBodyBVH();
	}
}

void MU_CALLCONV Object::setOrient(mu::NQuat newOrient) {
	body_.setOrient(newOrient);
	right_   = body_.orient().rotate(mu::Vec3(1.f, 0.f, 0.f));
	up_      = body_.orient().rotate(mu::Vec3(0.f, 1.f, 0.f));
	forward_ = body_.orient().rotate(mu::Vec3(0.f, 0.f, 1.f));

	if (pModel_ && !pModel_->bvh.empty()) {
		rebuildBodyBVH();
	}
}

void MU_CALLCONV Object::setScale(mu::Vec3 newScale) {
	body_.setScale(newScale);

	if (pModel_ && !pModel_->bvh.empty()) {
		rebuildBodyBVH();
	}
}

// Rebuilds the world-space BVH in body_ from the model's local-space BVH template.
// Tree structure (children indices) is preserved; only shape/bounds values are transformed.
void Object::rebuildBodyBVH() {
	if (!pModel_ || pModel_->bvh.empty()) return;

	const auto& localBVH = pModel_->bvh;
	BVH& worldBVH = body_.worldBVH();

	const mu::Vec3  pos    = body_.pos();
	const mu::NQuat orient = body_.orient();
	const mu::Vec3  scale  = body_.scale();

	const mu::Mat4x4 objWorld = mu::Mat4x4(mu::scale(scale))
		* mu::Mat4x4(orient)
		* mu::translate(pos);

	worldBVH.nodes.resize(localBVH.nodes.size());
	for (std::size_t i = 0; i < localBVH.nodes.size(); ++i) {
		const auto& src = localBVH.nodes[i];
		auto& dst = worldBVH.nodes[i];

		dst.children = src.children;
		dst.name = src.name;
		dst.boneIdx = src.boneIdx;

		dst.shape = std::visit([&](auto&& s) -> std::variant<AABB, OBB> {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, AABB>) {
				return AABB{
					s.center * scale + pos,
					s.size * scale,
				};
			}
			else {
				mu::NQuat worldOrient = orient;
				worldOrient *= s.orient;
				return OBB{
					orient.rotate(s.center * scale) + pos,
					s.halfExtents * scale,
					worldOrient,
				};
			}
			}, src.shape);

		dst.bounds = std::visit([](auto&& s) -> AABB {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, OBB>) return obbToAABB(s);
			else                                   return s;
			}, dst.shape);
	}
}

