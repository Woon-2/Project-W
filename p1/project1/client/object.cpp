#include "object.hpp"
#include "errorHandling.hpp"

void Object::setModel(const Model* pModel){
	pModel_ = pModel;
	currPhysicState_.aabbs.resize(pModel_->aabbs.size());
	for (std::size_t i = 0u; i < currPhysicState_.aabbs.size(); ++i) {
		currPhysicState_.aabbs[i].center
			= pModel_->aabbs[i].center * currPhysicState_.scale
			+ currPhysicState_.pos;
		currPhysicState_.aabbs[i].size
			= pModel_->aabbs[i].size * currPhysicState_.scale;
	}
	prevPhysicState_.aabbs = currPhysicState_.aabbs;
	renderState_.worldBVs.resize(currPhysicState_.aabbs.size());
}

void Object::update(Milliseconds deltaTime, float tPhysicInterpolation) {
	const auto& prev = prevPhysicState_;
	const auto& curr = currPhysicState_;
	const auto t = tPhysicInterpolation;

	// 방향 벡터 갱신
	right_ = curr.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
	up_ = curr.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
	forward_ = curr.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));
	
	// 렌더 상태 갱신
	const auto pos = mu::lerp(prev.pos, curr.pos, t);
	const auto orient = mu::slerp(prev.orient, curr.orient, t);	// 쿼터니언
	const auto scale = mu::lerp(prev.scale, curr.scale, t);

	renderState_.world = mu::Mat4x4(mu::scale(scale)) * mu::Mat4x4(orient) * mu::translate(pos);
	for (std::size_t i = 0; i < currPhysicState_.aabbs.size(); ++i) {
		const auto center = pModel_->aabbs[i].center * scale + pos;
		const auto size = pModel_->aabbs[i].size * scale;
		renderState_.worldBVs[i] = mu::Mat4x4(mu::scale(size)) * mu::translate(center);
	}
}

void Object::render(GFX& gfx) {
	if (pModel_) {
		for (auto& [mesh, dressXform] : pModel_->meshWithDressXforms) {
			for (std::size_t i = 0u; i < mesh.subMeshes.size(); ++i) {
				gfx.addDrawEvent(PBRPipeline::DrawEvent{
					.world = dressXform * renderState_.world,
					.mesh = &mesh,
					.subMesh = &mesh.subMeshes[i],
					.material = &mesh.materialSets[materialSetIdx_].materials[i]
				});
			}
		}
	}

	if (willRenderBV_) {
		for (std::size_t i = 0u; i < currPhysicState_.aabbs.size(); ++i) {
			gfx.addDrawEvent( BVPipeline::DrawEvent{
				.world = renderState_.worldBVs[i],
				.bvModel = BVPipeline::BVModel::Box
			} );
		}
	}
}

void MU_CALLCONV Object::setPos(mu::Vec3 newPos) {
	prevPhysicState_.pos = newPos;
	currPhysicState_.pos = newPos;

	if (pModel_) {
		currPhysicState_.aabbs.resize(pModel_->aabbs.size());
		for (std::size_t i = 0u; i < currPhysicState_.aabbs.size(); ++i) {
			currPhysicState_.aabbs[i].center
				= pModel_->aabbs[i].center * currPhysicState_.scale
				+ currPhysicState_.pos;
			currPhysicState_.aabbs[i].size
				= pModel_->aabbs[i].size * currPhysicState_.scale;
		}
		prevPhysicState_.aabbs = currPhysicState_.aabbs;
	}
}

void MU_CALLCONV Object::setOmega(mu::Vec3 newOmega) {
	prevPhysicState_.omega = newOmega;
	currPhysicState_.omega = newOmega;
}

void MU_CALLCONV Object::setOrient(mu::NQuat newOrient) {
	prevPhysicState_.orient = newOrient;
	currPhysicState_.orient = newOrient;
	right_ = currPhysicState_.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
	up_ = currPhysicState_.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
	forward_ = currPhysicState_.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));
}

void MU_CALLCONV Object::setScale(mu::Vec3 newScale) {
	prevPhysicState_.scale = newScale;
	currPhysicState_.scale = newScale;

	if (pModel_) {
		currPhysicState_.aabbs.resize(pModel_->aabbs.size());
		for (std::size_t i = 0u; i < currPhysicState_.aabbs.size(); ++i) {
			currPhysicState_.aabbs[i].center
				= pModel_->aabbs[i].center * currPhysicState_.scale
				+ currPhysicState_.pos;
			currPhysicState_.aabbs[i].size
				= pModel_->aabbs[i].size * currPhysicState_.scale;
		}
		prevPhysicState_.aabbs = currPhysicState_.aabbs;
	}
}