#ifndef __light_HPP
#define __light_HPP

#include "gfx.hpp"
#include "frustumCull.hpp"   // Frustum, intersects(Frustum, AABB/OBB); collision.hpp (AABB/OBB) via this
#include <array>
#include <variant>

class Camera;

class Light {
public:
	mu::Vec3 color{};
	float intensity = 0.f;
	mu::Vec3 atten{};
	float cosTheta = 0.f;
	float cosPhi = 0.f;
	float falloff = 0.f;
	PBRPipeline::LightData::Type type = PBRPipeline::LightData::Type::PointLight;
	bool isMainDirectionalLight = false;

	void update(Milliseconds deltaTime);

	// Single-light orthographic shadow (legacy / non-CSM path)
	void MU_CALLCONV updateShadowAuxDirectional( mu::Vec3 pointOfView, float distance,
		float left, float right, float bottom, float top, float nearZ, float farZ
	);

	// Computes CSM cascade view/proj matrices from camera frustum slices.
	// Cascade far depths are derived internally via the Practical Split Scheme
	// (lambda blend of uniform and logarithmic splits).
	void MU_CALLCONV updateCSMCascades(
		mu::Mat4x4 camView, mu::Mat4x4 camProj,
		const AssetConfigs::CascadeConfig& cascadeCfg,
		const AssetConfigs::ShadowMapConfig& shadowCfg
	);

	void render(GFX& gfx);

	void MU_CALLCONV setPos(mu::Vec3 newPos);
	mu::Vec3  MU_CALLCONV pos() const { return pos_; }
	void MU_CALLCONV setOrient(mu::NQuat newOrient);
	mu::NQuat MU_CALLCONV orient() const { return orient_; }

	// Single-shadow accessors (set by updateShadowAuxDirectional)
	mu::Mat4x4 MU_CALLCONV shadowView() const { return view_; }
	mu::Mat4x4 MU_CALLCONV shadowProj() const { return proj_; }

	// CSM cascade accessors (set by updateCSMCascades)
	const std::array<mu::Mat4x4, MAX_CSM_CASCADES>& cascadeViews() const { return cascadeViews_; }
	const std::array<mu::Mat4x4, MAX_CSM_CASCADES>& cascadeProjs() const { return cascadeProjs_; }
	XMFLOAT4 cascadeSplitsFarV() const { return cascadeSplitsFarV_; }
	u32t cascadeCount() const { return cascadeCount_; }
	const std::array<float, MAX_CSM_CASCADES>& cascadeNormalOffsets() const { return cascadeNormalOffsets_; }
	// Camera eye used to build the camera-relative cascade lightVPs (set by updateCSMCascades).
	// Casters/receivers rebase positions by this before applying lightVP.
	mu::Vec3 MU_CALLCONV cascadeCameraPos() const { return cascadeCameraPos_; }

	// --- Shadow (light) frustum culling: single entry point ---------------------
	// Returns true if the world-space shape casts into ANY cascade (i.e. it should
	// NOT be shadow-culled). The cascade frusta are cached in updateCSMCascades, in
	// camera-relative space, so the shape is rebased by cascadeCameraPos_ internally.
	// `expand` inflates the half-extents (e.g. 3.0 for large terrain chunks) to keep
	// the test conservative. When no cascades exist, returns true (never culls).
	bool MU_CALLCONV shadowVisible(const AABB& worldAABB, float expand = 1.f) const;
	bool MU_CALLCONV shadowVisible(const OBB&  worldOBB,  float expand = 1.f) const;
	bool MU_CALLCONV shadowVisible(const std::variant<AABB, OBB>& worldShape, float expand = 1.f) const;

	mu::NVec3 MU_CALLCONV dir() const {
		return mu::NVec3(orient_.rotate(mu::Vec3(0.f, 0.f, 1.f)));
	}

private:
	mu::Vec3  pos_{};
	mu::NQuat orient_{};

	// Single-shadow VP (legacy)
	mu::Mat4x4 view_{};
	mu::Mat4x4 proj_{};

	// CSM cascade data
	std::array<mu::Mat4x4, MAX_CSM_CASCADES> cascadeViews_{};
	std::array<mu::Mat4x4, MAX_CSM_CASCADES> cascadeProjs_{};
	std::array<Frustum, MAX_CSM_CASCADES> cascadeFrusta_{};  // cached for shadowVisible()
	XMFLOAT4 cascadeSplitsFarV_{};
	u32t cascadeCount_ = 0u;
	std::array<float, MAX_CSM_CASCADES> cascadeNormalOffsets_{};
	mu::Vec3 cascadeCameraPos_{};  // camera eye for camera-relative cascade space
};

#endif	// __light_HPP
