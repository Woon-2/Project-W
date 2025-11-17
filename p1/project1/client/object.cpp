#include "object.hpp"
#include "errorHandling.hpp"

// 모델을 설정한다.
// 모델이 있는 게임 객체는 render 시 GFX에 DrawEvent를 제출한다.
// 모델에 바운딩 볼륨이 존재할 경우, 월드 공간 바운딩 볼륨을 구축한다.
// (모델의 바운딩 볼륨을 기반으로 게임 객체의 월드 변환을 적용한
//  월드 공간 바운딩 볼륨을 따로 두어야 월드 공간 충돌 처리가 가능하다.)
void Object::setModel(const Model* pModel){
	DISPLAY_ERROR_STR(pModel != nullptr, "[Game Error] Object::setModel: 널 모델이 전달되었습니다.", false);
	if (pModel == nullptr) {
		return;
	}

	renderState_.pModel = pModel;
	currPhysicState_.aabbs.resize(pModel->aabbs.size());
	for (std::size_t i = 0u; i < currPhysicState_.aabbs.size(); ++i) {
		currPhysicState_.aabbs[i].center
			= pModel->aabbs[i].center * currPhysicState_.scale
			+ currPhysicState_.pos;
		currPhysicState_.aabbs[i].size
			= pModel->aabbs[i].size * currPhysicState_.scale;
	}
	prevPhysicState_.aabbs = currPhysicState_.aabbs;
	renderState_.worldBVs.resize(currPhysicState_.aabbs.size());
}

// @brief 게임 객체의 RenderState와 방향 벡터들을 갱신한다.
//		RenderState는 이전 PhysicState와 현재 PhysicState를 보간하여 얻어지고,
//      방향 벡터들은 현재 PhysicState의 내용으로 계산한다.
// @param deltaTime 마지막 프레임으로부터 경과한 시간
// @param tPhysicInterpolation 이전 PhysicState와 현재 PhysicState의 보간 비율
//		(게임 객체가 계산해서 일괄적으로 전달해야 한다.)
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

	const auto pModel = renderState_.pModel;

	renderState_.world = mu::Mat4x4(mu::scale(scale)) * mu::Mat4x4(orient) * mu::translate(pos);
	for (std::size_t i = 0; i < currPhysicState_.aabbs.size(); ++i) {
		const auto center = pModel->aabbs[i].center * scale + pos;
		const auto size = pModel->aabbs[i].size * scale;
		renderState_.worldBVs[i] = mu::Mat4x4(mu::scale(size)) * mu::translate(center);
	}
}

void Object::render(GFX& gfx) {
	const auto pModel = renderState_.pModel;
	if (pModel) {
		for (auto& [mesh, dressXform] : pModel->meshWithDressXforms) {
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

// 게임 객체의 위치를 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 위치가 모두 갱신된다.
// 각 PhysicState의 AABB 역시 갱신된다.
void MU_CALLCONV Object::setPos(mu::Vec3 newPos) {
	prevPhysicState_.pos = newPos;
	currPhysicState_.pos = newPos;

	const auto pModel = renderState_.pModel;

	if (pModel) {
		currPhysicState_.aabbs.resize(pModel->aabbs.size());
		for (std::size_t i = 0u; i < currPhysicState_.aabbs.size(); ++i) {
			currPhysicState_.aabbs[i].center
				= pModel->aabbs[i].center * currPhysicState_.scale
				+ currPhysicState_.pos;
			currPhysicState_.aabbs[i].size
				= pModel->aabbs[i].size * currPhysicState_.scale;
		}
		prevPhysicState_.aabbs = currPhysicState_.aabbs;
	}
}

// 게임 객체의 각속도를 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 각속도가 모두 갱신된다.
void MU_CALLCONV Object::setOmega(mu::Vec3 newOmega) {
	prevPhysicState_.omega = newOmega;
	currPhysicState_.omega = newOmega;
}

// 게임 객체의 방향을 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 방향이 모두 갱신된다.
// 게임 객체의 방향 벡터들도 전부 갱신된다.
void MU_CALLCONV Object::setOrient(mu::NQuat newOrient) {
	prevPhysicState_.orient = newOrient;
	currPhysicState_.orient = newOrient;
	right_ = currPhysicState_.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
	up_ = currPhysicState_.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
	forward_ = currPhysicState_.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));
}

// 게임 객체의 크기를 갱신한다.
// 이전 PhysicState와 현재 PhysicState의 크기가 모두 갱신된다.
// 각 PhysicState의 AABB 역시 갱신된다.
void MU_CALLCONV Object::setScale(mu::Vec3 newScale) {
	prevPhysicState_.scale = newScale;
	currPhysicState_.scale = newScale;

	const auto pModel = renderState_.pModel;

	if (pModel) {
		currPhysicState_.aabbs.resize(pModel->aabbs.size());
		for (std::size_t i = 0u; i < currPhysicState_.aabbs.size(); ++i) {
			currPhysicState_.aabbs[i].center
				= pModel->aabbs[i].center * currPhysicState_.scale
				+ currPhysicState_.pos;
			currPhysicState_.aabbs[i].size
				= pModel->aabbs[i].size * currPhysicState_.scale;
		}
		prevPhysicState_.aabbs = currPhysicState_.aabbs;
	}
}