#include "pch.hpp"
#include "light.hpp"
#include "errorHandling.hpp"
#include "camera.hpp"
#include <algorithm>
#include <cmath>

void Light::update(Milliseconds deltaTime) {

}

void MU_CALLCONV Light::updateShadowAuxDirectional( mu::Vec3 pointOfView, float distance,
	float left, float right, float bottom, float top, float nearZ, float farZ
) {
	const auto dir = mu::NVec3(orient().rotate(mu::Vec3(0.f, 0.f, 1.f)));
	pos_ = pointOfView - mu::Vec3(dir) * distance;
	view_ = mu::lookAt(pos_, pointOfView, mu::NVec3(0.f, 1.f, 0.f, mu::NVec3::NoNormalize_t{}));
	proj_ = mu::ortho(left, right, bottom, top, nearZ, farZ);
}

void MU_CALLCONV Light::updateCSMCascades(
	mu::Mat4x4 camView, mu::Mat4x4 camProj,
	const AssetConfigs::CascadeConfig& cascadeCfg,
	const AssetConfigs::ShadowMapConfig& shadowCfg
) {
	const u32t cascadeCount = shadowCfg.cascadeCount;
	cascadeCount_ = cascadeCount;

	// Practical Split Scheme: blend of uniform and logarithmic splits
	// C_i = lambda * nearZ*(farZ/nearZ)^((i+1)/N) + (1-lambda)*(nearZ + (farZ-nearZ)*(i+1)/N)
	float cascadeFarDistances[MAX_CSM_CASCADES] = {};
	const float nearZ  = cascadeCfg.nearZ;
	const float farZ   = cascadeCfg.farZ;
	const float lambda = cascadeCfg.lambda;
	const float ratio  = farZ / nearZ;
	const float N      = static_cast<float>(cascadeCount);
	for (u32t i = 0u; i < cascadeCount; ++i) {
		const float t  = static_cast<float>(i + 1u) / N;
		const float cLog  = nearZ * std::pow(ratio, t);
		const float cUnif = nearZ + (farZ - nearZ) * t;
		cascadeFarDistances[i] = lambda * cLog + (1.f - lambda) * cUnif;
	}

	const auto lightDir = mu::NVec3(orient_.rotate(mu::Vec3(0.f, 0.f, 1.f)));

	// Choose world-up axis orthogonal to light direction to avoid gimbal lock
	mu::Vec3 worldUp(0.f, 1.f, 0.f);
	if (std::abs(lightDir[1]) > 0.99f) {
		worldUp = mu::Vec3(0.f, 0.f, 1.f);
	}
	const auto worldUpN = mu::NVec3(worldUp, mu::NVec3::NoNormalize_t{});

	// Light view matrix: fixed world-space orientation (stable for texel snapping)
	const auto lightView = mu::lookAt(mu::Vec3(0.f, 0.f, 0.f), mu::Vec3(lightDir), worldUpN);

	// Extract A, B from projection matrix to convert view-space depth to NDC z:
	// NDC_z = A + B/viewZ  (LH: nearZ -> NDC_z=0, farZ -> NDC_z=1)
	const float A = camProj.row(2)[2];
	const float B = camProj.row(3)[2];

	// Camera near plane in view space: when NDC_z=0, viewZ = -B/A
	float prevFarV = -B / A;

	// Precompute inverse(camView * camProj) for NDC -> world unproject
	const auto invVP = mu::inverse(camView * camProj);

	for (u32t i = 0u; i < cascadeCount; ++i) {
		const float nearV = prevFarV;
		const float farV  = cascadeFarDistances[i];

		// NDC z for this cascade's near and far planes
		const float ndcZNear = A + B / nearV;
		const float ndcZFar  = A + B / farV;

		// Unproject 8 NDC corners to world space, then transform to light-view space
		float minX =  FLT_MAX, maxX = -FLT_MAX;
		float minY =  FLT_MAX, maxY = -FLT_MAX;
		float minZ =  FLT_MAX, maxZ = -FLT_MAX;

		for (float zNDC : {ndcZNear, ndcZFar}) {
			for (float x : {-1.f, 1.f}) {
				for (float y : {-1.f, 1.f}) {
					// NDC corner -> world space (row-vector convention: v * M)
					mu::Vec4 hClip = mu::Vec4(x, y, zNDC, 1.f) * invVP;
					const float invW = 1.f / hClip[3];
					mu::Vec3 world(hClip[0] * invW, hClip[1] * invW, hClip[2] * invW);

					// World -> light-view space
					mu::Vec4 lv = mu::Vec4(world, 1.f) * lightView;
					minX = std::min(minX, lv[0]); maxX = std::max(maxX, lv[0]);
					minY = std::min(minY, lv[1]); maxY = std::max(maxY, lv[1]);
					minZ = std::min(minZ, lv[2]); maxZ = std::max(maxZ, lv[2]);
				}
			}
		}

		// Texel snapping: round AABB to texel boundaries to prevent shadow swimming
		const float res    = static_cast<float>(shadowCfg.cascadeResolutions[i]);
		const float texelW = (maxX - minX) / res;
		const float texelH = (maxY - minY) / res;
		minX = std::floor(minX / texelW) * texelW;
		maxX = std::ceil(maxX  / texelW) * texelW;
		minY = std::floor(minY / texelH) * texelH;
		maxY = std::ceil(maxY  / texelH) * texelH;

		// Extend nearZ backward to catch shadow casters behind the frustum slice
		const float nearZPadding = 100.f;

		cascadeViews_[i] = lightView;
		cascadeProjs_[i] = mu::ortho(minX, maxX, minY, maxY, minZ - nearZPadding, maxZ);

		prevFarV = farV;
	}

	// Pack cascade far depths into float4 (view-space)
	float splits[4] = {};
	for (u32t i = 0u; i < cascadeCount && i < 4u; ++i) {
		splits[i] = cascadeFarDistances[i];
	}
	cascadeSplitsFarV_ = XMFLOAT4(splits[0], splits[1], splits[2], splits[3]);
}

void Light::render(GFX& gfx) {
	auto pbrLD = PBRPipeline::LightData{
		.pos = pos_,
		.dir = mu::NVec3(orient().rotate(mu::Vec3(0.f, 0.f, 1.f))),
		.color = color,
		.intensity = intensity,
		.cosTheta = cosTheta,
		.cosPhi = cosPhi,
		.falloff = falloff,
		.atten = atten,
		.type = type,
		.isMainDirectionalLight = isMainDirectionalLight,
		.cascadeViews  = cascadeViews_,
		.cascadeProjs  = cascadeProjs_,
		.cascadeSplitsFarV = cascadeSplitsFarV_,
		.cascadeCount  = cascadeCount_
	};
	gfx.addLightData(pbrLD);

	auto pbrSkinnedLD = PBRSkinnedPipeline::LightData{
		.pos = pos_,
		.dir = mu::NVec3(orient().rotate(mu::Vec3(0.f, 0.f, 1.f))),
		.color = color,
		.intensity = intensity,
		.cosTheta = cosTheta,
		.cosPhi = cosPhi,
		.falloff = falloff,
		.atten = atten,
		.type = static_cast<PBRSkinnedPipeline::LightData::Type>(type),
		.isMainDirectionalLight = isMainDirectionalLight,
		.cascadeViews  = cascadeViews_,
		.cascadeProjs  = cascadeProjs_,
		.cascadeSplitsFarV = cascadeSplitsFarV_,
		.cascadeCount  = cascadeCount_
	};
	gfx.addLightData(pbrSkinnedLD);

	auto terrainLD = TerrainPipeline::LightData{
		.dir                    = mu::NVec3(orient().rotate(mu::Vec3(0.f, 0.f, 1.f))),
		.color                  = color,
		.intensity              = intensity,
		.type                   = static_cast<TerrainPipeline::LightData::Type>(type),
		.isMainDirectionalLight = isMainDirectionalLight,
		.cascadeViews           = cascadeViews_,
		.cascadeProjs           = cascadeProjs_,
		.cascadeSplitsFarV      = cascadeSplitsFarV_,
		.cascadeCount           = cascadeCount_
	};
	gfx.addLightData(terrainLD);
}

void MU_CALLCONV Light::setPos(mu::Vec3 newPos) {
	pos_ = newPos;
}

void MU_CALLCONV Light::setOrient(mu::NQuat newOrient) {
	orient_ = newOrient;
}