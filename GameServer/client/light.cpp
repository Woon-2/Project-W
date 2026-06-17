#include "pch.hpp"
#include "light.hpp"
#include "errorHandling.hpp"
#include "camera.hpp"
#include "terrainDeferredPipeline.hpp"
#include <algorithm>
#include <cmath>

void Light::update(Milliseconds deltaTime) {

}

void MU_CALLCONV Light::updateShadowAuxDirectional( mu::Vec3 pointOfView, float distance,
	float left, float right, float bottom, float top, float nearZ, float farZ
) {
	const auto lightDir = mu::NVec3(orient().rotate(mu::Vec3(0.f, 0.f, 1.f)));
	pos_ = pointOfView - mu::Vec3(lightDir) * distance;

	mu::Vec3 worldUp(0.f, 1.f, 0.f);
	if (std::abs(lightDir[1]) > 0.99f) {
		worldUp = mu::Vec3(0.f, 0.f, 1.f);
	}
	const auto worldUpN = mu::NVec3(worldUp, mu::NVec3::NoNormalize_t{});

	// Pure rotation lightView (eye = 0), same approach as updateCSMCascades
	const auto lightView = mu::lookAt(mu::Vec3(0.f, 0.f, 0.f), mu::Vec3(lightDir), worldUpN);

	// Transform pos_ to light-view space to offset the ortho bounds.
	// Caller passes left/right/bottom/top relative to pos_; we preserve that semantics.
	const mu::Vec4 posLV = mu::Vec4(pos_, 1.f) * lightView;

	view_ = lightView;
	proj_ = mu::ortho(
		left   + posLV[0], right + posLV[0],
		bottom + posLV[1], top   + posLV[1],
		nearZ  + posLV[2], farZ  + posLV[2]
	);
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

	// Light view matrix: pure rotation (eye = 0), fixed world-space orientation.
	// Because eye = 0 and the frustum corners below are built in CAMERA-RELATIVE space
	// (camera eye at the origin), the resulting cascade ortho bounds — and therefore
	// lightVP = cascadeView * cascadeProj — map camera-relative positions (posW - camPos)
	// to light NDC. Receivers/casters subtract camPos in the shaders before applying
	// lightVP, so the large world magnitude (~5500) never enters the light-space math.
	const auto lightView = mu::lookAt(mu::Vec3(0.f, 0.f, 0.f), mu::Vec3(lightDir), worldUpN);

	// --- Camera basis in WORLD space, extracted directly from camView (no inverse). ---
	// camView's upper-3x3 is the orthonormal world->view rotation R. For a row-vector
	// view matrix (worldDir * R = viewDir) the rows of R are the view-space axes; the
	// world-space camera axes (right, up, forward) are therefore the COLUMNS of R.
	// Reading columns is an exact transpose — no floating-point inverse is performed, so
	// this is exact regardless of camera position. mu::Mat::col() returns a Vec sized to
	// the matrix; for a 4x4 view matrix we take the upper 3 components of each column.
	const auto colRight   = camView.col(0);
	const auto colUp      = camView.col(1);
	const auto colForward = camView.col(2);
	const mu::Vec3 camRight  (colRight[0],   colRight[1],   colRight[2]);
	const mu::Vec3 camUp     (colUp[0],      colUp[1],      colUp[2]);
	const mu::Vec3 camForward(colForward[0], colForward[1], colForward[2]);

	// --- Camera eye (world position), extracted from camView without an inverse. ---
	// For a row-vector view matrix, camView = [[R],[t]] with viewPos = worldPos*R + t.
	// At the eye viewPos = 0, so eye = -t * R^T. R^T's rows are R's columns, i.e. the
	// camera axes (camRight/camUp/camForward), giving the closed form below. Uses only the
	// orthonormal transpose (exact), never a floating-point inverse.
	const mu::Vec3 camT(camView.row(3)[0], camView.row(3)[1], camView.row(3)[2]);
	const mu::Vec3 camEye =
		camRight * (-camT[0]) + camUp * (-camT[1]) + camForward * (-camT[2]);
	cascadeCameraPos_ = camEye;

	// --- Vertical FOV and aspect derived directly from the projection matrix. ---
	// LH perspective: camProj.m[1][1] = 1/tan(fovY/2), camProj.m[0][0] = 1/tan(fovX/2).
	// tan(fovY/2) = 1 / m11 ;  tan(fovX/2) = tan(fovY/2) * (m11 / m00) = (m11/m00)/m11 = 1/m00.
	const float projM00 = camProj.row(0)[0];
	const float projM11 = camProj.row(1)[1];
	const float tanHalfY = 1.f / projM11;   // half-height per unit view depth
	const float tanHalfX = 1.f / projM00;   // half-width  per unit view depth

	// Extract A, B from projection matrix to convert view-space depth to NDC z:
	// NDC_z = A + B/viewZ  (LH: nearZ -> NDC_z=0, farZ -> NDC_z=1)
	const float A = camProj.row(2)[2];
	const float B = camProj.row(3)[2];

	// Camera near plane in view space: when NDC_z=0, viewZ = -B/A
	float prevFarV = -B / A;

	for (u32t i = 0u; i < cascadeCount; ++i) {
		const float nearV = prevFarV;
		const float farV  = cascadeFarDistances[i];

		// Build the 8 frustum-slice corners DIRECTLY from the camera basis in
		// camera-relative space (eye at the origin). For a view-space depth d the slice
		// plane spans +/- d*tanHalfX horizontally and +/- d*tanHalfY vertically:
		//   corner = forward*d + right*(d*tanHalfX*sx) + up*(d*tanHalfY*sy)
		// This avoids inverse(camView*camProj); the only inputs are the (exact) camera
		// basis, the projection's fov terms, and the per-cascade near/far depths, so the
		// corners are independent of the camera's world position and free of large-
		// magnitude unproject error.
		float lv_x[8], lv_y[8], lv_z[8];
		int   ci    = 0;
		float minZ  = FLT_MAX, maxZ = -FLT_MAX;

		for (float d : {nearV, farV}) {
			const float hx = d * tanHalfX;
			const float hy = d * tanHalfY;
			for (float sx : {-1.f, 1.f}) {
				for (float sy : {-1.f, 1.f}) {
					// Camera-relative world-space corner (camera eye at origin).
					const mu::Vec3 cornerRel =
						camForward * d + camRight * (hx * sx) + camUp * (hy * sy);

					// Camera-relative world -> light-view space (lightView eye = 0).
					mu::Vec4 lv = mu::Vec4(cornerRel, 1.f) * lightView;
					lv_x[ci] = lv[0]; lv_y[ci] = lv[1]; lv_z[ci] = lv[2];
					minZ = std::min(minZ, lv[2]); maxZ = std::max(maxZ, lv[2]);
					++ci;
				}
			}
		}

		// Bounding sphere: centroid of the 8 corners in light-view space.
		// The sphere radius is invariant to camera rotation (depends only on FOV and
		// cascade near/far depths), which gives a stable, constant worldUnitsPerTexel.
		float cx = 0.f, cy = 0.f, cz = 0.f;
		for (int k = 0; k < 8; ++k) { cx += lv_x[k]; cy += lv_y[k]; cz += lv_z[k]; }
		cx /= 8.f; cy /= 8.f; cz /= 8.f;

		float radius = 0.f;
		for (int k = 0; k < 8; ++k) {
			const float dx = lv_x[k] - cx, dy = lv_y[k] - cy, dz = lv_z[k] - cz;
			radius = std::max(radius, std::sqrt(dx*dx + dy*dy + dz*dz));
		}

		const float res = static_cast<float>(shadowCfg.cascadeResolutions[i]);

		// Guard against degenerate cascades (e.g. camera not yet initialised,
		// nearV == farV, or NaN propagation from an ill-conditioned projection).
		// A zero or NaN radius makes minX == maxX which asserts inside mu::ortho().
		if (!std::isfinite(radius) || radius < 0.01f) {
			cascadeViews_[i] = lightView;
			cascadeProjs_[i] = mu::ortho(-0.5f, 0.5f, -0.5f, 0.5f, -1.f, 1.f);
			cascadeNormalOffsets_[i] = 0.f;
			prevFarV = farV;
			continue;
		}

		// Texel snapping: snap the sphere center XY to the texel grid. worldUnitsPerTexel is
		// derived from the bounding-sphere radius, which is rotation-invariant (depends only on
		// FOV and the cascade near/far depths), so the grid stays stable frame-to-frame and
		// matches the final ortho extent exactly.
		// NOTE: per-cascade radius quantization was tried here and removed — the discrete radius
		// step caused visible jitter while moving, and the camera-relative cascade space already
		// removes the shadow shimmer on its own.
		const float worldUnitsPerTexel = (2.f * radius) / res;
		cx = std::round(cx / worldUnitsPerTexel) * worldUnitsPerTexel;
		cy = std::round(cy / worldUnitsPerTexel) * worldUnitsPerTexel;

		// Fixed-size ortho bounds from the bounding sphere
		const float minX = cx - radius, maxX = cx + radius;
		const float minY = cy - radius, maxY = cy + radius;

		// nearZPadding = radius: scales with cascade size, preserves depth precision.
		// Catches shadow casters behind the frustum slice without over-extending the Z range.
		constexpr float kNormalOffsetTexels = 2.0f;
		cascadeNormalOffsets_[i] = worldUnitsPerTexel * kNormalOffsetTexels;
		cascadeViews_[i] = lightView;
		cascadeProjs_[i] = mu::ortho(minX, maxX, minY, maxY, minZ - 2.f * radius, maxZ);

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
		.cascadeViews        = cascadeViews_,
		.cascadeProjs        = cascadeProjs_,
		.cascadeSplitsFarV   = cascadeSplitsFarV_,
		.cascadeCount        = cascadeCount_,
		.cascadeNormalOffsets = cascadeNormalOffsets_,
		.cascadeCameraPos    = cascadeCameraPos_
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
		.cascadeViews        = cascadeViews_,
		.cascadeProjs        = cascadeProjs_,
		.cascadeSplitsFarV   = cascadeSplitsFarV_,
		.cascadeCount        = cascadeCount_,
		.cascadeNormalOffsets = cascadeNormalOffsets_,
		.cascadeCameraPos    = cascadeCameraPos_
	};
	gfx.addLightData(pbrSkinnedLD);

	auto pbrDeferredLD = PBRDeferredPipeline::LightData{
		.pos = pos_,
		.dir = mu::NVec3(orient().rotate(mu::Vec3(0.f, 0.f, 1.f))),
		.color = color,
		.intensity = intensity,
		.cosTheta = cosTheta,
		.cosPhi = cosPhi,
		.falloff = falloff,
		.atten = atten,
		.type = static_cast<PBRDeferredPipeline::LightData::Type>(type),
		.isMainDirectionalLight = isMainDirectionalLight,
		.cascadeViews        = cascadeViews_,
		.cascadeProjs        = cascadeProjs_,
		.cascadeSplitsFarV   = cascadeSplitsFarV_,
		.cascadeCount        = cascadeCount_,
		.cascadeNormalOffsets = cascadeNormalOffsets_,
		.cascadeCameraPos    = cascadeCameraPos_
	};
	gfx.addLightData(pbrDeferredLD);

	auto pbrDeferredSkinnedLD = PBRDeferredSkinnedPipeline::LightData{
		.pos = pos_,
		.dir = mu::NVec3(orient().rotate(mu::Vec3(0.f, 0.f, 1.f))),
		.color = color,
		.intensity = intensity,
		.cosTheta = cosTheta,
		.cosPhi = cosPhi,
		.falloff = falloff,
		.atten = atten,
		.type = static_cast<PBRDeferredSkinnedPipeline::LightData::Type>(type),
		.isMainDirectionalLight = isMainDirectionalLight,
		.cascadeViews        = cascadeViews_,
		.cascadeProjs        = cascadeProjs_,
		.cascadeSplitsFarV   = cascadeSplitsFarV_,
		.cascadeCount        = cascadeCount_,
		.cascadeNormalOffsets = cascadeNormalOffsets_,
		.cascadeCameraPos    = cascadeCameraPos_
	};
	gfx.addLightData(pbrDeferredSkinnedLD);

	auto terrainLD = TerrainPipeline::LightData{
		.dir                    = mu::NVec3(orient().rotate(mu::Vec3(0.f, 0.f, 1.f))),
		.color                  = color,
		.intensity              = intensity,
		.type                   = static_cast<TerrainPipeline::LightData::Type>(type),
		.isMainDirectionalLight = isMainDirectionalLight,
		.cascadeViews           = cascadeViews_,
		.cascadeProjs           = cascadeProjs_,
		.cascadeSplitsFarV      = cascadeSplitsFarV_,
		.cascadeCount           = cascadeCount_,
		.cascadeNormalOffsets   = cascadeNormalOffsets_,
		.cascadeCameraPos       = cascadeCameraPos_
	};
	gfx.addLightData(terrainLD);

	auto terrainDeferredLD = TerrainDeferredPipeline::LightData{
		.dir                    = mu::NVec3(orient().rotate(mu::Vec3(0.f, 0.f, 1.f))),
		.color                  = color,
		.intensity              = intensity,
		.type                   = static_cast<TerrainDeferredPipeline::LightData::Type>(type),
		.isMainDirectionalLight = isMainDirectionalLight,
		.cascadeViews           = cascadeViews_,
		.cascadeProjs           = cascadeProjs_,
		.cascadeSplitsFarV      = cascadeSplitsFarV_,
		.cascadeCount           = cascadeCount_,
		.cascadeNormalOffsets   = cascadeNormalOffsets_,
		.cascadeCameraPos       = cascadeCameraPos_
	};
	gfx.addLightData(terrainDeferredLD);
}

void MU_CALLCONV Light::setPos(mu::Vec3 newPos) {
	pos_ = newPos;
}

void MU_CALLCONV Light::setOrient(mu::NQuat newOrient) {
	orient_ = newOrient;
}