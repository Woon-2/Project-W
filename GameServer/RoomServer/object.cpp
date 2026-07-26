#include "rspch.hpp"
#include "object.hpp"
#include "Model.hpp"
#include "GameSession.hpp"
#include <cmath>

void Object::setModel(const Model* pModel){
	DISPLAY_ERROR_STR(pModel != nullptr, "[Game Error] Object::setModel: null model.", false);
	if (pModel == nullptr) {
		return;
	}

	pModel_ = pModel;
	modelBaseScale_ = pModel->baseScale;   // model's own scale (unbaked); applied via body scale
	applyCompositeScale();

	// Build the aim-pitch spine chain (spine_01..03) and per-bone subtree depth.
	// The chain is nested (spine_01 ⊃ spine_02 ⊃ spine_03), so depth = number of
	// chain bones among the bone's ancestors-or-self. Missing bones cut the chain.
	spineChainIdx_ = { -1, -1, -1 };
	spineDepth_.clear();
	const ServerSkeleton& sk = pModel->skeleton;
	if (!sk.empty()) {
		static constexpr const char* kSpineChain[] = { "spine_01", "spine_02", "spine_03" };
		spineDepth_.assign(sk.bones.size(), 0);
		for (int c = 0; c < 3; ++c) {
			auto it = sk.nameToIdx.find(kSpineChain[c]);
			if (it == sk.nameToIdx.end()) break;
			spineChainIdx_[c] = it->second;
		}
		for (std::size_t b = 0; b < sk.bones.size(); ++b) {
			uint8 depth = 0;
			for (int i = static_cast<int>(b); i >= 0; i = sk.bones[static_cast<std::size_t>(i)].parentIdx) {
				for (int c = 0; c < 3; ++c) {
					if (spineChainIdx_[c] == i)
						depth = std::max<uint8>(depth, static_cast<uint8>(c + 1));
				}
			}
			spineDepth_[b] = depth;
		}
	}
}

// Mirrors the client's AnimBlenderPlayer::onPostDress in model (dress) space:
// rotates each spine-chain subtree around the chain bone's pivot by
// cameraPitch()/N about the local X axis, accumulating down the chain.
// Applied before the entity world transform, so bone-attached hitboxes
// (BoneAttach("spine_01") + applyAttachRotation) follow the pitched pose
// the caster predicted with.
void Object::applySpinePitch(std::vector<mu::Mat4x4>& boneModelXforms) const {
	int chainCnt = 0;
	for (int c = 0; c < 3; ++c) {
		if (spineChainIdx_[c] >= 0) ++chainCnt;
	}
	if (chainCnt == 0) return;
	if (spineDepth_.size() != boneModelXforms.size()) return;

	const auto perJoint = mu::Radian(cameraPitch() / static_cast<float>(chainCnt));

	for (int c = 0; c < chainCnt; ++c) {
		const int k = spineChainIdx_[c];
		// Pivot from the CURRENT matrix (previous chain steps already applied).
		const mu::Vec3 pivot = mu::Vec3(mu::Vec4(0.f, 0.f, 0.f, 1.f) * boneModelXforms[static_cast<std::size_t>(k)]);
		const mu::Mat4x4 K = mu::translate(-pivot) * mu::rotateXH(perJoint) * mu::translate(pivot);
		for (std::size_t b = 0; b < boneModelXforms.size(); ++b) {
			if (spineDepth_[b] > c)
				boneModelXforms[b] = boneModelXforms[b] * K;
		}
	}
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

// Compose model base scale x per-instance scale into body_ (mirrors client Object).
void Object::applyCompositeScale() {
	body_.setScale(modelBaseScale_ * instanceScale_);

	if (pModel_ && !pModel_->bvh.empty()) {
		rebuildBodyBVH();
	}
}

void MU_CALLCONV Object::setScale(mu::Vec3 newScale) {
	instanceScale_ = newScale;
	applyCompositeScale();
}

mu::Vec3 MU_CALLCONV Object::calcSeparationForce( const std::vector<mu::Vec3>& nearby, float radius ) const {
	mu::Vec3 force{ 0.f, 0.f, 0.f };

	for ( const mu::Vec3& op : nearby ) {
		mu::Vec3 away = pos() - op;
		float d = away.len();

		if ( d < 1e-4f ) {
			float a = static_cast<float>( getId() ) * 1.2f;
			force += mu::Vec3{ std::cosf( a ), 0.f, std::sinf( a ) };
			continue;
		}

		float strength = 1.f - (d / radius);
		force += (away / d) * strength;
	}
	return force;
}

void Object::updateAnimBones(Seconds dt) {
	// Locomotion clips play back proportionally to ground speed, mirroring the client's
	// foot-slide reduction. The bone poses computed below feed the hit BVH, so if only the
	// client scaled playback a walking monster's limbs would sit where the server thinks
	// they are, not where the player sees them. Every other clip stays at 1x -- skill
	// timelines and hit windows assume real-time playback.
	animController_.setPlaybackRate(
		(animRefSpeed_ > 0.f && animController_.isPlayingLocomotion())
			? ServerAnimState::locomotionRate(horizontalSpeed(), animRefSpeed_, animBandEnd_)
			: 1.f);

	animController_.advance(dt.count());
	if (!pModel_ || pModel_->skeleton.empty() || !animController_.clip) return;

	const int boneCnt = pModel_->skeleton.boneCount();
	std::vector<mu::Mat4x4> boneModelXforms(boneCnt);
	animController_.computeBoneModelXforms(pModel_->skeleton, boneModelXforms);

	// Aim-pitch spine recomposition (players only in practice — NPCs keep 0).
	if (cameraPitch() != 0.f && spineChainIdx_[0] >= 0) {
		applySpinePitch(boneModelXforms);
	}

	// Includes body scale so bone-attached BV centers grow with the model (client parity).
	const mu::Mat4x4 entityWorld = mu::Mat4x4(mu::scale(body_.scale()))
		* mu::Mat4x4(orient().mat()) * mu::translate(pos());
	std::vector<mu::Mat4x4> boneWorldXforms(boneCnt);
	for (int i = 0; i < boneCnt; ++i)
		boneWorldXforms[i] = boneModelXforms[i] * entityWorld;

	updateBoneWorldXforms(std::move(boneWorldXforms));
}

void Object::updateBoneWorldXforms(std::vector<mu::Mat4x4>&& xforms) {
	boneWorldXforms_ = std::move(xforms);
	rebuildBodyBVH();
}

// Rebuilds the world-space BVH in body_ from the model's local-space BVH template.
// Tree structure (children indices) is preserved; only shape/bounds values are transformed.
// For nodes with a valid boneIdx, uses boneWorldXforms_ (bone-local → world) via
// transformOBBByMatrix. Falls back to the entity's pos/orient/scale for unrigged nodes.
void Object::rebuildBodyBVH() {
	if (!pModel_ || pModel_->bvh.empty()) return;

	const auto& localBVH = pModel_->bvh;
	BVH& worldBVH = body_.worldBVH();

	const mu::Vec3  pos    = body_.pos();
	const mu::NQuat orient = body_.orient();
	const mu::Vec3  scale  = body_.scale();

	worldBVH.nodes.resize(localBVH.nodes.size());
	for (std::size_t i = 0; i < localBVH.nodes.size(); ++i) {
		const auto& src = localBVH.nodes[i];
		auto& dst = worldBVH.nodes[i];

		dst.children    = src.children;
		dst.name        = src.name;
		dst.boneIdx     = src.boneIdx;
		dst.damageCoeff = src.damageCoeff;

		const bool hasBoneXform = src.boneIdx >= 0
			&& src.boneIdx < static_cast<int>(boneWorldXforms_.size());

		if (hasBoneXform) {
			const mu::Mat4x4& boneXform = boneWorldXforms_[src.boneIdx];
			const mu::Vec3    s         = body_.scale();
			dst.shape = std::visit([&](auto&& shp) -> std::variant<AABB, OBB> {
				using T = std::decay_t<decltype(shp)>;
				OBB obb = [&] {
					if constexpr (std::is_same_v<T, AABB>) return toOBB(shp);
					else                                    return shp;
				}();
				// Bone xform is rigid; entityWorld scale only moves the center, so scale the
				// half-extents separately for the BV to grow with the model (client parity).
				obb.halfExtents = obb.halfExtents * s;
				return transformOBBByMatrix(obb, boneXform);
			}, src.shape);
		} else {
			// Rigid (non-bone) path shared with ScatterCollider. Correctly turns a
			// rotated axis-aligned model box into a world OBB (previously the
			// rotation was dropped for AABB shapes).
			dst.shape = transformShapeRigid(src.shape, pos, orient, scale);
		}

		dst.bounds = std::visit([](auto&& s) -> AABB {
			using T = std::decay_t<decltype(s)>;
			if constexpr (std::is_same_v<T, OBB>) return obbToAABB(s);
			else                                   return s;
		}, dst.shape);
	}
}

