#include "game/physicsSystem.hpp"

#include "game/level.hpp"

#include "TMP.hpp"

RigidBody::RigidBody(const ecs::Entity& entity) NOEXCEPT
	: Component(entity), velocity_(), momentum_(), compressedDeltaVelocity_(0),
	invMass_(0.f), kFriction_(0.f), willSimulateGravity_(false) {}

void MU_CALLCONV RigidBody::accMomentum(mu::Vec3 momentum) NOEXCEPT {
	if (invMass_ <= minInvMass) {
		return;
	}

	// 2-3: x, 4-5: y, 6-7: z, precision: 0.00003m
	static constexpr auto precision = 0.00003f;
	const auto oldV = compressedDeltaVelocity_.load();

	const auto deltaAccV = momentum * invMass_;

	auto deltaVx = static_cast<i16t>(oldV >> 32);
	deltaVx += static_cast<i16t>(deltaAccV.x() / precision);

	auto deltaVy = static_cast<i16t>(oldV >> 16);
	deltaVy += static_cast<i16t>(deltaAccV.y() / precision);

	
	auto deltaVz = static_cast<i16t>(oldV);
	deltaVz += static_cast<i16t>(deltaAccV.z() / precision);

	const auto newV = static_cast<u64t>(static_cast<u16t>(deltaVx)) << 32 |
		static_cast<u64t>(static_cast<u16t>(deltaVy)) << 16 |
		static_cast<u64t>(static_cast<u16t>(deltaVz));

	compressedDeltaVelocity_.store(newV);
}

void RigidBody::update(MilliSeconds deltaTime) {
	const auto detlaTimeSec = std::chrono::duration_cast<Seconds>(deltaTime);

	if (invMass_ <= minInvMass) {
		return;
	}

	// 2-3: x, 4-5: y, 6-7: z, precision: 0.00003m
	static constexpr auto precision = 0.00003f;

	const auto dvCompressed = compressedDeltaVelocity_.load();
	compressedDeltaVelocity_.store(0);

	const auto dvx = static_cast<i16t>(dvCompressed >> 32) * precision;
	const auto dvy = static_cast<i16t>(dvCompressed >> 16) * precision;
	const auto dvz = static_cast<i16t>(dvCompressed) * precision;

	auto deltaV = mu::Vec3(dvx, dvy, dvz);
	velocity_ += deltaV;

	// apply friction
	if (kFriction_ > 0) {
		auto frictionDir = mu::NVec3(velocity_);
		auto friction = mu::Vec3(frictionDir) * (kFriction_ * gravityConst * detlaTimeSec.count());

		const auto newV = velocity_ - friction;
		if (mu::dot(newV, velocity_) < 0) {
			velocity_ = mu::Vec3(0, 0, 0);
		}
		else {
			velocity_ = newV;
		}
	}

	// apply gravity
	if (willSimulateGravity_) {
		velocity_ += mu::Vec3(0, -gravityConst, 0) * detlaTimeSec.count();
	}

	// apply air drag

	momentum_ = velocity_ * mass();
}

void PhysicsSystem::update(MilliSeconds deltaTime)
{
	for (auto& pRigidBody : components<RigidBody>()) {
		if (pRigidBody) {
			pRigidBody->update(deltaTime);

			if (auto pCoord = gameEngine::Coord::at(pRigidBody->entityID().value())) {
				const auto detlaTimeSec = std::chrono::duration_cast<Seconds>(deltaTime);
				const auto dp = pRigidBody->velocity() * detlaTimeSec.count();
				pCoord->accTranslation(dp);
			}
		}
	}
}

bool MU_CALLCONV contains(const BoundingCapsule a, const mu::Vec3 point) {
	auto aNormal = mu::Vec3(mu::NVec3(a.tip - a.base));
	auto aLineEndOffset = aNormal * a.radius;
	auto aA = a.base + aLineEndOffset;
	auto aB = a.tip - aLineEndOffset;

	auto best = ClosestPointOnLineSegment(aA, aB, point);

	return (best - point).len2() < a.radius * a.radius;
}

bool MU_CALLCONV intersects(const BoundingCapsule a, const BoundingCapsule& b) {
	// capsule A:
	mu::Vec3 aNormal = mu::NVec3(a.tip - a.base); 
	auto aLineEndOffset = aNormal * a.radius; 
	auto aA = a.base + aLineEndOffset; 
	auto aB = a.tip - aLineEndOffset;

	// capsule B:
	mu::Vec3 bNormal = mu::NVec3(b.tip - b.base); 
	auto bLineEndOffset = bNormal * b.radius; 
	auto bA = b.base + bLineEndOffset; 
	auto bB = b.tip - bLineEndOffset;

	// vectors between line endpoints:
	auto v0 = bA - aA; 
	auto v1 = bB - aA; 
	auto v2 = bA - aB; 
	auto v3 = bB - aB;

	// squared distances:
	float d0 = v0.len2();
	float d1 = v1.len2();
	float d2 = v2.len2();
	float d3 = v3.len2();

	// select best potential endpoint on capsule A:
	auto bestA = mu::Vec3();
	if (d2 < d0 || d2 < d1 || d3 < d0 || d3 < d1) {
		bestA = aB;
	}
	else {
		bestA = aA;
	}

	// select point on capsule B line segment nearest to best potential endpoint on A capsule:
	auto bestB = ClosestPointOnLineSegment(bA, bB, bestA);

	// now do the same for capsule A segment:
	bestA = ClosestPointOnLineSegment(aA, aB, bestB);

	auto len = (bestA - bestB).len();
	float penetrationDepth = a.radius + b.radius - len;

	return penetrationDepth > 0;
}

bool MU_CALLCONV intersects(const BoundingOrientedBox lhs, const BoundingOrientedBox& rhs) {
    // Build the 3x3 rotation matrix that defines the orientation of B relative to A.
	auto R = mu::Mat3x3(lhs.orientation * rhs.orientation.dual());

    // Compute the translation of B relative to A.
	auto t = lhs.orientation.dual().rotate(rhs.center - lhs.center);

    //
    // h(A) = extents of A.
    // h(B) = extents of B.
    //
    // a(u) = axes of A = (1,0,0), (0,1,0), (0,0,1)
    // b(u) = axes of B relative to A = (r00,r10,r20), (r01,r11,r21), (r02,r12,r22)
    //
    // For each possible separating axis l:
    //   d(A) = sum (for i = u,v,w) h(A)(i) * abs( a(i) dot l )
    //   d(B) = sum (for i = u,v,w) h(B)(i) * abs( b(i) dot l )
    //   if abs( t dot l ) > d(A) + d(B) then disjoint
    //

    // Rows. Note R[0,1,2]X.w = 0.
    auto R0X = R.row(0u);
    auto R1X = R.row(1u);
    auto R2X = R.row(2u);

    R = mu::transpose(R);

    // Columns. Note RX[0,1,2].w = 0.
    auto RX0 = R.row(0u);
    auto RX1 = R.row(1u);
    auto RX2 = R.row(2u);

    // Absolute value of rows.
    auto AR0X = mu::abs(R0X);
    auto AR1X = mu::abs(R1X);
    auto AR2X = mu::abs(R2X);

    // Absolute value of columns.
    auto ARX0 = mu::abs(RX0);
    auto ARX1 = mu::abs(RX1);
    auto ARX2 = mu::abs(RX2);

    // Test each of the 15 possible seperating axii.

    // l = a(u) = (1, 0, 0)
    // t dot l = t.x
    // d(A) = h(A).x
    // d(B) = h(B) dot abs(r00, r01, r02)
	auto d = mu::Vec3(t.x());
	auto dA = mu::Vec3(lhs.extents.x());
	auto dB = mu::Vec3(mu::dot(rhs.extents, AR0X));
    auto noIntersection = mu::greater(mu::abs(d), dA + dB);

    // l = a(v) = (0, 1, 0)
    // t dot l = t.y
    // d(A) = h(A).y
    // d(B) = h(B) dot abs(r10, r11, r12)
    d = mu::Vec3(t.y());
	dA = mu::Vec3(lhs.extents.y());
	dB = mu::Vec3(mu::dot(rhs.extents, AR1X));
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));

    // l = a(w) = (0, 0, 1)
    // t dot l = t.z
    // d(A) = h(A).z
    // d(B) = h(B) dot abs(r20, r21, r22)
	d = mu::Vec3(t.z());
	dA = mu::Vec3(lhs.extents.z());
	dB = mu::Vec3(mu::dot(rhs.extents, AR2X));
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));

    // l = b(u) = (r00, r10, r20)
    // d(A) = h(A) dot abs(r00, r10, r20)
    // d(B) = h(B).x
	d = mu::Vec3(mu::dot(t, RX0));
	dA = mu::Vec3(mu::dot(lhs.extents, ARX0));
	dB = mu::Vec3(rhs.extents.x());
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));

    // l = b(v) = (r01, r11, r21)
    // d(A) = h(A) dot abs(r01, r11, r21)
    // d(B) = h(B).y
	d = mu::Vec3(mu::dot(t, RX1));
	dA = mu::Vec3(mu::dot(lhs.extents, ARX1));
	dB = mu::Vec3(rhs.extents.y());
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));

    // l = b(w) = (r02, r12, r22)
    // d(A) = h(A) dot abs(r02, r12, r22)
    // d(B) = h(B).z
	d = mu::Vec3(mu::dot(t, RX2));
	dA = mu::Vec3(mu::dot(lhs.extents, ARX2));
	dB = mu::Vec3(rhs.extents.z());
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));

    // l = a(u) x b(u) = (0, -r20, r10)
    // d(A) = h(A) dot abs(0, r20, r10)
    // d(B) = h(B) dot abs(0, r02, r01)
	d = mu::Vec3(mu::dot(t, mu::Vec3(mu::permute<3u, 6u, 1u, 0u>(mu::Vec4(RX0), mu::Vec4(-RX0))))); // use vec4 permute for optimization
	dA = mu::Vec3(mu::dot(lhs.extents, mu::Vec3(mu::Vec4(ARX0).wzyx())));	// use vec4 swizzle for optimization
	dB = mu::Vec3(mu::dot(rhs.extents, mu::Vec3(mu::Vec4(AR0X).wzyx())));	// use vec4 swizzle for optimization
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));

    // l = a(u) x b(v) = (0, -r21, r11)
    // d(A) = h(A) dot abs(0, r21, r11)
    // d(B) = h(B) dot abs(r02, 0, r00)
	d = mu::Vec3(mu::dot(t, mu::Vec3(mu::permute<3u, 6u, 1u, 0u>(mu::Vec4(RX1), mu::Vec4(-RX1)))));
	dA = mu::Vec3(mu::dot(lhs.extents, mu::Vec3(mu::Vec4(ARX1).wzyx())));
	dB = mu::Vec3(mu::dot(rhs.extents, mu::Vec3(mu::Vec4(AR0X).zwxy())));
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));

    // l = a(u) x b(w) = (0, -r22, r12)
    // d(A) = h(A) dot abs(0, r22, r12)
    // d(B) = h(B) dot abs(r01, r00, 0)
	d = mu::Vec3(mu::dot(t, mu::Vec3(mu::permute<3u, 6u, 1u, 0u>(mu::Vec4(RX2), mu::Vec4(-RX2)))));
	dA = mu::Vec3(mu::dot(lhs.extents, mu::Vec3(mu::Vec4(ARX2).wzyx())));
	dB = mu::Vec3(mu::dot(rhs.extents, mu::Vec3(mu::Vec4(AR0X).yxwz())));
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));

    // l = a(v) x b(u) = (r20, 0, -r00)
    // d(A) = h(A) dot abs(r20, 0, r00)
    // d(B) = h(B) dot abs(0, r12, r11)
	d = mu::Vec3(mu::dot(t, mu::Vec3(mu::permute<2u, 3u, 4u, 1u>(mu::Vec4(RX0), mu::Vec4(-RX0)))));
	dA = mu::Vec3(mu::dot(lhs.extents, mu::Vec3(mu::Vec4(ARX0).zwxy())));
	dB = mu::Vec3(mu::dot(rhs.extents, mu::Vec3(mu::Vec4(AR1X).wzyx())));
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));

    // l = a(v) x b(v) = (r21, 0, -r01)
    // d(A) = h(A) dot abs(r21, 0, r01)
    // d(B) = h(B) dot abs(r12, 0, r10)
	d = mu::Vec3(mu::dot(t, mu::Vec3(mu::permute<2u, 3u, 4u, 1u>(mu::Vec4(RX1), mu::Vec4(-RX1)))));
	dA = mu::Vec3(mu::dot(lhs.extents, mu::Vec3(mu::Vec4(ARX1).zwxy())));
	dB = mu::Vec3(mu::dot(rhs.extents, mu::Vec3(mu::Vec4(AR1X).zwxy())));
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));


    // l = a(v) x b(w) = (r22, 0, -r02)
    // d(A) = h(A) dot abs(r22, 0, r02)
    // d(B) = h(B) dot abs(r11, r10, 0)
	d = mu::Vec3(mu::dot(t, mu::Vec3(mu::permute<2u, 3u, 4u, 1u>(mu::Vec4(RX2), mu::Vec4(-RX2)))));
	dA = mu::Vec3(mu::dot(lhs.extents, mu::Vec3(mu::Vec4(ARX2).zwxy())));
	dB = mu::Vec3(mu::dot(rhs.extents, mu::Vec3(mu::Vec4(AR1X).yxwz())));
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));

    // l = a(w) x b(u) = (-r10, r00, 0)
    // d(A) = h(A) dot abs(r10, r00, 0)
    // d(B) = h(B) dot abs(0, r22, r21)
	d = mu::Vec3(mu::dot(t, mu::Vec3(mu::permute<5u, 0u, 3u, 2u>(mu::Vec4(RX0), mu::Vec4(-RX0)))));
	dA = mu::Vec3(mu::dot(lhs.extents, mu::Vec3(mu::Vec4(ARX0).yxwz())));
	dB = mu::Vec3(mu::dot(rhs.extents, mu::Vec3(mu::Vec4(AR2X).wzyx())));
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));

    // l = a(w) x b(v) = (-r11, r01, 0)
    // d(A) = h(A) dot abs(r11, r01, 0)
    // d(B) = h(B) dot abs(r22, 0, r20)
	d = mu::Vec3(mu::dot(t, mu::Vec3(mu::permute<5u, 0u, 3u, 2u>(mu::Vec4(RX1), mu::Vec4(-RX1)))));
	dA = mu::Vec3(mu::dot(lhs.extents, mu::Vec3(mu::Vec4(ARX1).yxwz())));
	dB = mu::Vec3(mu::dot(rhs.extents, mu::Vec3(mu::Vec4(AR2X).zwxy())));
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));

    // l = a(w) x b(w) = (-r12, r02, 0)
    // d(A) = h(A) dot abs(r12, r02, 0)
    // d(B) = h(B) dot abs(r21, r20, 0)
	d = mu::Vec3(mu::dot(t, mu::Vec3(mu::permute<5u, 0u, 3u, 2u>(mu::Vec4(RX2), mu::Vec4(-RX2)))));
	dA = mu::Vec3(mu::dot(lhs.extents, mu::Vec3(mu::Vec4(ARX2).yxwz())));
	dB = mu::Vec3(mu::dot(rhs.extents, mu::Vec3(mu::Vec4(AR2X).yxwz())));
	noIntersection = mu::bwOr(noIntersection, mu::greater(mu::abs(d), dA + dB));

    // No seperating axis found, boxes must intersect.
	return mu::notEqualAllI(noIntersection, mu::Vec3::trueV());
}

bool MU_CALLCONV intersects(const BoundingOrientedBox lhs, const BoundingFrustum& rhs) {
	// transform box into frustum space
	auto R = mu::Mat3x3(lhs.orientation * rhs.orientation.dual());
	auto c = lhs.center - rhs.origin;
	c *= R;

	R = mu::transpose(R);
	const auto x0 = -R.row(0) * lhs.extents.x();
	const auto x1 = R.row(0) * lhs.extents.x();
	const auto y0 = -R.row(1) * lhs.extents.y();
	const auto y1 = R.row(1) * lhs.extents.y();
	const auto z0 = -R.row(2) * lhs.extents.z();
	const auto z1 = R.row(2) * lhs.extents.z();

	// project all corners of box via perspective projection matrix
	const auto persp = mu::persp(rhs.fovy, rhs.aspect, rhs.nearZ, rhs.farZ);

	const auto p0 = mu::Vec3(mu::Vec4(c + x0 + y0 + z0, 1.f) * persp);
	const auto p0w = p0.w();
	const auto p1 = mu::Vec3(mu::Vec4(c + x1 + y0 + z0, 1.f) * persp);
	const auto p1w = p1.w();
	const auto p2 = mu::Vec3(mu::Vec4(c + x0 + y1 + z0, 1.f) * persp);
	const auto p2w = p2.w();
	const auto p3 = mu::Vec3(mu::Vec4(c + x1 + y1 + z0, 1.f) * persp);
	const auto p3w = p3.w();
	const auto p4 = mu::Vec3(mu::Vec4(c + x0 + y0 + z1, 1.f) * persp);
	const auto p4w = p4.w();
	const auto p5 = mu::Vec3(mu::Vec4(c + x1 + y0 + z1, 1.f) * persp);
	const auto p5w = p5.w();
	const auto p6 = mu::Vec3(mu::Vec4(c + x0 + y1 + z1, 1.f) * persp);
	const auto p6w = p6.w();
	const auto p7 = mu::Vec3(mu::Vec4(c + x1 + y1 + z1, 1.f) * persp);
	const auto p7w = p7.w();

	// if any corner is inside the frustum, we have an intersection
	auto hasIntersection = p0.x() >= -p0w && p0.x() <= p0w
		&& p0.y() >= -p0w && p0.y() <= p0w
		&& p0.z() >= 0.f && p0.z() <= p0w;

	hasIntersection = hasIntersection || (p1.x() >= -p1w && p1.x() <= p1w
		&& p1.y() >= -p1w && p1.y() <= p1w
		&& p1.z() >= 0.f && p1.z() <= p1w);

	hasIntersection = hasIntersection || (p2.x() >= -p2w && p2.x() <= p2w
		&& p2.y() >= -p2w && p2.y() <= p2w
		&& p2.z() >= 0.f && p2.z() <= p2w);

	hasIntersection = hasIntersection || (p3.x() >= -p3w && p3.x() <= p3w
		&& p3.y() >= -p3w && p3.y() <= p3w
		&& p3.z() >= 0.f && p3.z() <= p3w);

	hasIntersection = hasIntersection || (p4.x() >= -p4w && p4.x() <= p4w
		&& p4.y() >= -p4w && p4.y() <= p4w
		&& p4.z() >= 0.f && p4.z() <= p4w);

	hasIntersection = hasIntersection || (p5.x() >= -p5w && p5.x() <= p5w
		&& p5.y() >= -p5w && p5.y() <= p5w
		&& p5.z() >= 0.f && p5.z() <= p5w);

	hasIntersection = hasIntersection || (p6.x() >= -p6w && p6.x() <= p6w
		&& p6.y() >= -p6w && p6.y() <= p6w
		&& p6.z() >= 0.f && p6.z() <= p6w);

	hasIntersection = hasIntersection || (p7.x() >= -p7w && p7.x() <= p7w
		&& p7.y() >= -p7w && p7.y() <= p7w
		&& p7.z() >= 0.f && p7.z() <= p7w);
	
	return hasIntersection;
}

bool MU_CALLCONV intersects(const BoundingCapsule lhs, const BoundingOrientedBox& rhs) {
	// Align capsule to the box's local space.
	mu::Vec3 aNormal = mu::NVec3(lhs.tip - lhs.base); 
	auto aLineEndOffset = aNormal * lhs.radius; 
	auto aA = lhs.base + aLineEndOffset;
	auto aB = lhs.tip - aLineEndOffset;
	aA -= rhs.center;
	aB -= rhs.center;
	aA = rhs.orientation.dual().rotate(aA);
	aB = rhs.orientation.dual().rotate(aB);

	auto halfX = rhs.extents.x();
	auto halfY = rhs.extents.y();
	auto halfZ = rhs.extents.z();

	const auto bestAL = ClosestPointOnLineSegment(aA, aB, mu::Vec3(-halfX, 0.f, 0.f));
	const auto bestAR = ClosestPointOnLineSegment(aA, aB, mu::Vec3(halfX, 0.f, 0.f));
	const auto bestAT = ClosestPointOnLineSegment(aA, aB, mu::Vec3(0.f, halfY, 0.f));
	const auto bestAB = ClosestPointOnLineSegment(aA, aB, mu::Vec3(0.f, -halfY, 0.f));
	const auto bestAF = ClosestPointOnLineSegment(aA, aB, mu::Vec3(0.f, 0.f, halfZ));
	const auto bestAN = ClosestPointOnLineSegment(aA, aB, mu::Vec3(0.f, 0.f, -halfZ));


	const auto bestALProj = mu::Vec3(-halfX, std::clamp(bestAL.y(), -halfY, halfY), std::clamp(bestAL.z(), -halfZ, halfZ));
	const auto bestARProj = mu::Vec3(halfX, std::clamp(bestAR.y(), -halfY, halfY), std::clamp(bestAR.z(), -halfZ, halfZ));
	const auto bestATProj = mu::Vec3(std::clamp(bestAT.x(), -halfX, halfX), halfY, std::clamp(bestAT.z(), -halfZ, halfZ));
	const auto bestABProj = mu::Vec3(std::clamp(bestAB.x(), -halfX, halfX), -halfY, std::clamp(bestAB.z(), -halfZ, halfZ));
	const auto bestAFProj = mu::Vec3(std::clamp(bestAF.x(), -halfX, halfX), std::clamp(bestAF.y(), -halfY, halfY), halfZ);
	const auto bestANProj = mu::Vec3(std::clamp(bestAN.x(), -halfX, halfX), std::clamp(bestAN.y(), -halfY, halfY), -halfZ);

	const auto r2 = lhs.radius * lhs.radius;

	return ((bestAL - bestALProj).len2() <= r2)
		|| ((bestAR - bestARProj).len2() <= r2)
		|| ((bestAT - bestATProj).len2() <= r2)
		|| ((bestAB - bestABProj).len2() <= r2)
		|| ((bestAF - bestAFProj).len2() <= r2)
		|| ((bestAN - bestANProj).len2() <= r2);
}

bool MU_CALLCONV intersects(const BoundingCapsule lhs, const BoundingFrustum& rhs) {
	// approximate the capsule as a OBB
	const auto center = (lhs.base + lhs.tip) * 0.5f;
	const auto halfHeight = (lhs.tip - lhs.base).len() * 0.5f;
	const auto v = mu::NVec3(lhs.tip - lhs.base);
	
	const auto front = mu::NVec3(0.f, 0.f, 1.f, mu::NVec3::NoNormalize_t{});
	auto n = mu::cross(v, front);
	static constexpr auto endurance = 1e-6f;
	if (n.len2() < endurance) {
		n = mu::NVec3(0.f, 1.f, 0.f, mu::NVec3::NoNormalize_t{});	
	}

	const auto theta = std::acos(mu::dot(v, front));
	const auto halfCos = std::cos(theta) * 0.5f;
	const auto halfSin = std::sin(theta) * 0.5f;

	const auto quatV = mu::Vec3(n) * halfSin;
	const auto quatW = halfCos;

	auto orientation = mu::NQuat(quatV, quatW, mu::NQuat::NoNormalize_t{});
	auto extents = mu::Vec3(lhs.radius, lhs.radius, halfHeight);

	auto obb = BoundingOrientedBox{ center, extents, orientation };
	return intersects(obb, rhs);
}

bool MU_CALLCONV intersects(const BoundingBox lhs, const BoundingBox rhs) {
	return lhs.min.x() <= rhs.max.x() && lhs.max.x() >= rhs.min.x()
		&& lhs.min.y() <= rhs.max.y() && lhs.max.y() >= rhs.min.y()
		&& lhs.min.z() <= rhs.max.z() && lhs.max.z() >= rhs.min.z();
}

bool MU_CALLCONV intersects(const BoundingBox lhs, const BoundingCapsule rhs) {
	mu::Vec3 aNormal = mu::NVec3(rhs.tip - rhs.base); 
	auto aLineEndOffset = aNormal * rhs.radius; 
	auto aA = rhs.base + aLineEndOffset;
	auto aB = rhs.tip - aLineEndOffset;

	const auto bCenter = (lhs.min + lhs.max) * 0.5f;
	const auto bCenterX = bCenter.x();
	const auto bCenterY = bCenter.y();
	const auto bCenterZ = bCenter.z();
	const auto bMinX = lhs.min.x();
	const auto bMinY = lhs.min.y();
	const auto bMinZ = lhs.min.z();
	const auto bMaxX = lhs.max.x();
	const auto bMaxY = lhs.max.y();
	const auto bMaxZ = lhs.max.z();

	const auto bestAL = ClosestPointOnLineSegment(aA, aB, mu::Vec3(bMinX, bCenterY, bCenterZ));
	const auto bestAR = ClosestPointOnLineSegment(aA, aB, mu::Vec3(bMaxX, bCenterY, bCenterZ));
	const auto bestAT = ClosestPointOnLineSegment(aA, aB, mu::Vec3(bCenterX, bMaxY, bCenterZ));
	const auto bestAB = ClosestPointOnLineSegment(aA, aB, mu::Vec3(bCenterX, bMinY, bCenterZ));
	const auto bestAF = ClosestPointOnLineSegment(aA, aB, mu::Vec3(bCenterX, bCenterY, bMaxZ));
	const auto bestAN = ClosestPointOnLineSegment(aA, aB, mu::Vec3(bCenterX, bCenterY, bMinZ));

	const auto bestALProj = mu::Vec3(bMinX, std::clamp(bestAL.y(), bMinY, bMaxY), std::clamp(bestAL.z(), bMinZ, bMaxZ));
	const auto bestARProj = mu::Vec3(bMaxX, std::clamp(bestAR.y(), bMinY, bMaxY), std::clamp(bestAR.z(), bMinZ, bMaxZ));
	const auto bestATProj = mu::Vec3(std::clamp(bestAT.x(), bMinX, bMaxX), bMaxY, std::clamp(bestAT.z(), bMinZ, bMaxZ));
	const auto bestABProj = mu::Vec3(std::clamp(bestAB.x(), bMinX, bMaxX), bMinY, std::clamp(bestAB.z(), bMinZ, bMaxZ));
	const auto bestAFProj = mu::Vec3(std::clamp(bestAF.x(), bMinX, bMaxX), std::clamp(bestAF.y(), bMinY, bMaxY), bMaxZ);
	const auto bestANProj = mu::Vec3(std::clamp(bestAN.x(), bMinX, bMaxX), std::clamp(bestAN.y(), bMinY, bMaxY), bMinZ);

	const auto r2 = rhs.radius * rhs.radius;

	return ((bestAL - bestALProj).len2() <= r2)
		|| ((bestAR - bestARProj).len2() <= r2)
		|| ((bestAT - bestATProj).len2() <= r2)
		|| ((bestAB - bestABProj).len2() <= r2)
		|| ((bestAF - bestAFProj).len2() <= r2)
		|| ((bestAN - bestANProj).len2() <= r2);
}

bool MU_CALLCONV intersects(const BoundingBox lhs, const BoundingOrientedBox& rhs) {
	// transform box to obb's local space
	auto R = mu::Mat3x3(rhs.orientation.dual());
	auto aExtents = mu::Vec3(lhs.max - lhs.min) * 0.5f;
	auto aCenter = (lhs.min + lhs.max) * 0.5f;
	aCenter -= rhs.center;
	aCenter *= R;

	R = mu::transpose(R);
	const auto ARX0 = mu::abs(R.row(0u));
	const auto ARX1 = mu::abs(R.row(1u));
	const auto ARX2 = mu::abs(R.row(2u));

	aExtents = aExtents.x() * ARX0 + aExtents.y() * ARX1 + aExtents.z() * ARX2;

	// check collision between two aabbs
	const auto aMin = aCenter - aExtents;
	const auto aMax = aCenter + aExtents;
	const auto bMin = -rhs.extents;
	const auto bMax = rhs.extents;

	return aMin.x() <= bMax.x() && aMax.x() >= bMin.x()
		&& aMin.y() <= bMax.y() && aMax.y() >= bMin.y()
		&& aMin.z() <= bMax.z() && aMax.z() >= bMin.z();
}

bool MU_CALLCONV intersects(const BoundingBox lhs, const BoundingFrustum& rhs) {
	// transform box into frustum space
	auto R = mu::Mat3x3(rhs.orientation.dual());
	auto aExtents = mu::Vec3(lhs.max - lhs.min) * 0.5f;
	auto c = (lhs.min + lhs.max) * 0.5f;
	c -= rhs.origin;
	c *= R;

	R = mu::transpose(R);

	const auto x0 = -R.row(0) * aExtents.x();
	const auto x1 = R.row(0) * aExtents.x();
	const auto y0 = -R.row(1) * aExtents.y();
	const auto y1 = R.row(1) * aExtents.y();
	const auto z0 = -R.row(2) * aExtents.z();
	const auto z1 = R.row(2) * aExtents.z();

	// project all corners of box via perspective projection matrix
	const auto persp = mu::persp(rhs.fovy, rhs.aspect, rhs.nearZ, rhs.farZ);

	const auto p0 = mu::Vec3(mu::Vec4(c + x0 + y0 + z0, 1.f) * persp);
	const auto p0w = p0.w();
	const auto p1 = mu::Vec3(mu::Vec4(c + x1 + y0 + z0, 1.f) * persp);
	const auto p1w = p1.w();
	const auto p2 = mu::Vec3(mu::Vec4(c + x0 + y1 + z0, 1.f) * persp);
	const auto p2w = p2.w();
	const auto p3 = mu::Vec3(mu::Vec4(c + x1 + y1 + z0, 1.f) * persp);
	const auto p3w = p3.w();
	const auto p4 = mu::Vec3(mu::Vec4(c + x0 + y0 + z1, 1.f) * persp);
	const auto p4w = p4.w();
	const auto p5 = mu::Vec3(mu::Vec4(c + x1 + y0 + z1, 1.f) * persp);
	const auto p5w = p5.w();
	const auto p6 = mu::Vec3(mu::Vec4(c + x0 + y1 + z1, 1.f) * persp);
	const auto p6w = p6.w();
	const auto p7 = mu::Vec3(mu::Vec4(c + x1 + y1 + z1, 1.f) * persp);
	const auto p7w = p7.w();

	// if any corner is inside the frustum, we have an intersection
	auto hasIntersection = p0.x() >= -p0w && p0.x() <= p0w
		&& p0.y() >= -p0w && p0.y() <= p0w
		&& p0.z() >= 0.f && p0.z() <= p0w;

	hasIntersection = hasIntersection || (p1.x() >= -p1w && p1.x() <= p1w
		&& p1.y() >= -p1w && p1.y() <= p1w
		&& p1.z() >= 0.f && p1.z() <= p1w);

	hasIntersection = hasIntersection || (p2.x() >= -p2w && p2.x() <= p2w
		&& p2.y() >= -p2w && p2.y() <= p2w
		&& p2.z() >= 0.f && p2.z() <= p2w);
	
	hasIntersection = hasIntersection || (p3.x() >= -p3w && p3.x() <= p3w
		&& p3.y() >= -p3w && p3.y() <= p3w
		&& p3.z() >= 0.f && p3.z() <= p3w);
	
	hasIntersection = hasIntersection || (p4.x() >= -p4w && p4.x() <= p4w
		&& p4.y() >= -p4w && p4.y() <= p4w
		&& p4.z() >= 0.f && p4.z() <= p4w);
	
	hasIntersection = hasIntersection || (p5.x() >= -p5w && p5.x() <= p5w
		&& p5.y() >= -p5w && p5.y() <= p5w
		&& p5.z() >= 0.f && p5.z() <= p5w);

	hasIntersection = hasIntersection || (p6.x() >= -p6w && p6.x() <= p6w
		&& p6.y() >= -p6w && p6.y() <= p6w
		&& p6.z() >= 0.f && p6.z() <= p6w);
	
	hasIntersection = hasIntersection || (p7.x() >= -p7w && p7.x() <= p7w
		&& p7.y() >= -p7w && p7.y() <= p7w
		&& p7.z() >= 0.f && p7.z() <= p7w);

	return hasIntersection;
}

bool Collider::intersects(const Collider& other) const {
	switch (type_) {
	case Type::Capsule:
		switch (other.type_) {
		case Type::Capsule:
			return ::intersects(capsule_, other.capsule_);
		case Type::Box:
			return ::intersects(other.aabb_, capsule_);
		case Type::OrientedBox:
			return ::intersects(capsule_, other.obb_);
		case Type::Frustum:
			return ::intersects(capsule_, other.frustum_);
		default:
			break;
		}
	case Type::OrientedBox:
		switch (other.type_) {
		case Type::Capsule:
			return ::intersects(other.capsule_, obb_);
		case Type::Box:
			return ::intersects(other.aabb_, obb_);
		case Type::OrientedBox:
			return ::intersects(obb_, other.obb_);
		case Type::Frustum:
			return ::intersects(obb_, other.frustum_);
		default:
			break;
		}
	case Type::Frustum:
		switch (other.type_) {
		case Type::Capsule:
			return ::intersects(other.capsule_, frustum_);
		case Type::Box:
			return ::intersects(other.aabb_, frustum_);
		case Type::OrientedBox:
			return ::intersects(other.obb_, frustum_);		
		default:
			break;
		}
	}

	throw std::runtime_error("requested coolision detection is currently not implemented.");
}

bool MU_CALLCONV Collider::contains(const mu::Vec3 point) const {
	switch (type_) {
	case Type::Capsule:
		return ::contains(capsule_, point);
	default:
		break;
	}

	throw std::runtime_error("requested coolision detection is currently not implemented.");
}

Collider MU_CALLCONV transformCollider(mu::Mat4x4 transform, const Collider& collider) {
	switch (collider.type_) {
	case Collider::Type::Capsule:
		return Collider{ transformCollider(transform, collider.capsule_) };
	case Collider::Type::Box:
		return Collider{ transformCollider(transform, collider.aabb_) };
	case Collider::Type::OrientedBox:
		return Collider{ transformCollider(transform, collider.obb_) };
	case Collider::Type::Frustum:
		return Collider{ transformCollider(transform, collider.frustum_) };
	default:
		throw std::runtime_error("requested coolision detection is currently not implemented.");
	}
}

BoundingCapsule MU_CALLCONV transformCollider(
	mu::Mat4x4 transform, const BoundingCapsule& capsule
) {
	auto newBase = mu::Vec3(mu::Vec4(capsule.base, 1.f) * transform);
	auto newTip = mu::Vec3(mu::Vec4(capsule.tip, 1.f) * transform);
	// TODO: check uniform scale when debugging
	auto newRadius = transform.row(0).len() * capsule.radius;

	return BoundingCapsule{
		.base = newBase,
		.tip = newTip,
		.radius = newRadius
	};
}

BoundingBox MU_CALLCONV transformCollider(
	mu::Mat4x4 transform, const BoundingBox& box
) {
	// TODO: check no rotation when debugging
	auto newMinX = box.min.x() * transform.row(0).len();
	auto newMinY = box.min.y() * transform.row(1).len();
	auto newMinZ = box.min.z() * transform.row(2).len();
	auto newMaxX = box.max.x() * transform.row(0).len();
	auto newMaxY = box.max.y() * transform.row(1).len();
	auto newMaxZ = box.max.z() * transform.row(2).len();

	return BoundingBox{
		.min = mu::Vec3(newMinX, newMinY, newMinZ),
		.max = mu::Vec3(newMaxX, newMaxY, newMaxZ)
	};
}

BoundingOrientedBox MU_CALLCONV transformCollider(
	mu::Mat4x4 transform, const BoundingOrientedBox& box
) {
	dx::XMMATRIX nM;
	nM.r[0] = mu::NVec3(transform.row(0)).get();
	nM.r[1] = mu::NVec3(transform.row(1)).get();
	nM.r[2] = mu::NVec3(transform.row(2)).get();
	nM.r[3] = dx::g_XMIdentityR3;

	auto rotation = mu::quatRotMat(nM);
	auto newOrientation = mu::NQuat(box.orientation * rotation);
	auto newCenter = mu::Vec3(mu::Vec4(box.center, 1.f) * transform);

	auto dx = mu::Vec3(transform.row(0)).lenV();
	auto dy = mu::Vec3(transform.row(1)).lenV();
	auto dz = mu::Vec3(transform.row(2)).lenV();

	auto vScale = mu::select(dy, dx, mu::Vec3(dx::g_XMSelect1000));
	vScale = mu::select(dz, vScale, mu::Vec3(dx::g_XMSelect1100));
	auto newExtents = box.extents * vScale;

	return BoundingOrientedBox{
		.center = newCenter,
		.extents = newExtents,
		.orientation = newOrientation
	};
}

BoundingFrustum MU_CALLCONV transformCollider(
	mu::Mat4x4 transform, const BoundingFrustum& frustum
) {
	auto newOrigin = mu::Vec3(mu::Vec4(frustum.origin, 1.f) * transform);
	// TODO: check no scale when debugging
	auto newOrientation = mu::NQuat(frustum.orientation * mu::quatRotMat(transform));

	return BoundingFrustum{
		.origin = newOrigin,
		.orientation = newOrientation,
		.fovy = frustum.fovy,
		.aspect = frustum.aspect,
		.nearZ = frustum.nearZ,
		.farZ = frustum.farZ
	};
}

// needs optimization
bool BoundingVolumeNode::collides(const BoundingVolumeNode& other) const {
	auto collided = false;

	for (const auto& collider : colliders_) {
		for (const auto& otherCollider : other.colliders_) {
			if (collider.intersects(otherCollider)) {
				collided = true;
				break;
			}
		}
	}

	if (!collided) {
		return false;
	}

	if (children_.empty() && other.children_.empty()) {
		return true;
	}

	if (children_.empty()) {
		for (const auto& otherChild : other.children_) {
			if (collides(otherChild)) {
				return true;
			}
		}
		return false;
	}

	if (other.children_.empty()) {
		for (const auto& child : children_) {
			if (other.collides(child)) {
				return true;
			}
		}
		return false;
	}

	for (const auto& child : children_) {
		for (const auto& otherChild : other.children_) {
			if (child.collides(otherChild)) {
				return true;
			}
		}
	}

	return false;
}

bool MU_CALLCONV BoundingVolumeNode::collides(
	mu::Mat4x4 lhsTransform, const BoundingVolumeNode& lhs,
	const mu::Mat4x4& rhsTransform, const BoundingVolumeNode& rhs
) {
	auto worldLhs = lhs.transform(lhsTransform);
	auto worldRhs = rhs.transform(rhsTransform);
	
	return worldLhs.collides(worldRhs);
}

BoundingVolumeNode MU_CALLCONV BoundingVolumeNode::transform(
	mu::Mat4x4 transform
) const {
	BoundingVolumeNode ret{};

	ret.reserveColliders(colliders_.size());
	for (const auto& collider : colliders_) {
		ret.addCollider(transformCollider(transform, collider));
	}

	ret.reserveChildren(children_.size());
	for (const auto& child : children_) {
		ret.addChild(child.transform(transform));
	}

	return ret;
}

BoundingVolume::BoundingVolume(const ecs::Entity& entity, std::ifstream& bvhStream)
	: Component(entity) {
	importBVHNode(bvhStream, root_);
}

void BoundingVolume::importBVHNode(std::ifstream& bvhStream, BoundingVolumeNode& node) {
	auto& is = bvhStream;

	char pstrToken[64] = { '\0' };

	std::uint8_t nStrLength = 0;
	std::uint32_t nReads = 0;

	int intVal{};

	readStream(is, nStrLength);
	readStream(is, pstrToken, nStrLength);

	if (std::strcmp(pstrToken, "<Node:>")) {
		throw std::runtime_error("[BoundingVolume::importBVHNode]: <Node:> token expected but not found.");
	}

	for (;;) {
		readStream(is, nStrLength);
		readStream(is, pstrToken, nStrLength);

		if (!std::strcmp(pstrToken, "</Node>")) {
			break;
		}
		else if (!std::strcmp(pstrToken, "<Colliders:>")) {
			readStream(is, intVal);
			readColliders(is, node, static_cast<std::size_t>(intVal));
		}
		else if (!std::strcmp(pstrToken, "<Children:>")) {
			readStream(is, intVal);
			node.reserveChildren(static_cast<std::size_t>(intVal));
			for (std::size_t i = 0; i < static_cast<std::size_t>(intVal); ++i) {
				importBVHNode(is, node.addChild());
			}
		}
		else {
			throw std::runtime_error("[BoundingVolume::importBVHNode]: unknown token found.");
		}
	}
}

void BoundingVolume::readColliders( std::ifstream& is,
	BoundingVolumeNode& node, std::size_t colliderCnt
) {
	char pstrToken[64] = { '\0' };

	std::uint8_t nStrLength = 0;
	std::uint32_t nReads = 0;

	int intVal{};

	node.reserveColliders(colliderCnt);

	for (std::size_t i = 0; i < colliderCnt; ++i) {
		readStream(is, nStrLength);
		readStream(is, pstrToken, nStrLength);

		if (std::strcmp(pstrToken, "<Collider:>")) {
			throw std::runtime_error("[BoundingVolume::readColliders]: <Collider:> token expected but not found.");
		}

		for (;;) {
			readStream(is, nStrLength);
			readStream(is, pstrToken, nStrLength);

			if (!std::strcmp(pstrToken, "</Collider>")) {
				break;
			}
			else if (!std::strcmp(pstrToken, "<Name:>")) {
				readStream(is, nStrLength);
				readStream(is, pstrToken, nStrLength);
			}
			else if (!std::strcmp(pstrToken, "<Type:>")) {
				readStream(is, intVal);
				switch (intVal) {
				case etoi(Collider::Type::Capsule):
					readCapsuleCollider(is, node);
					break;

				case etoi(Collider::Type::OrientedBox):
					readOBBCollider(is, node);
					break;

				default:
					throw std::runtime_error("[BoundingVolume::readColliders]: unsupported collider type found.");
				}
			}
			else {
				throw std::runtime_error("[BoundingVolume::readColliders]: unknown token found.");
			}
		}
	}

	readStream(is, nStrLength);
	readStream(is, pstrToken, nStrLength);

	if (std::strcmp(pstrToken, "</Colliders>")) {
		throw std::runtime_error("[BoundingVolume::readColliders]: </Colliders> token expected but not found.");
	}
}

void BoundingVolume::readCapsuleCollider(std::ifstream& bvhStream, BoundingVolumeNode& node) {
	char pstrToken[64] = { '\0' };

	std::uint8_t nStrLength = 0;
	std::uint32_t nReads = 0;

    float floatVal{};
	int intVal{};
    dx::XMFLOAT3 float3Val{};

	readStream(bvhStream, nStrLength);
	readStream(bvhStream, pstrToken, nStrLength);

	if (std::strcmp(pstrToken, "<Base:>")) {
		throw std::runtime_error("[BoundingVolume::readCapsuleCollider]: <Base:> token expected but not found.");
	}

	readStream(bvhStream, float3Val);
	const auto base = mu::Vec3(float3Val.x, float3Val.y, float3Val.z);

	readStream(bvhStream, nStrLength);
	readStream(bvhStream, pstrToken, nStrLength);

	if (std::strcmp(pstrToken, "<Tip:>")) {
		throw std::runtime_error("[BoundingVolume::readCapsuleCollider]: <Tip:> token expected but not found.");
	}

	readStream(bvhStream, float3Val);
	const auto tip = mu::Vec3(float3Val.x, float3Val.y, float3Val.z);

	readStream(bvhStream, nStrLength);
	readStream(bvhStream, pstrToken, nStrLength);

	if (std::strcmp(pstrToken, "<Radius:>")) {
		throw std::runtime_error("[BoundingVolume::readCapsuleCollider]: <Radius:> token expected but not found.");
	}

	readStream(bvhStream, floatVal);
	const auto radius = floatVal;

	node.addCollider( BoundingCapsule{
		.base = base, .tip = tip, .radius = radius
	} );
}

void BoundingVolume::readOBBCollider(std::ifstream& is, BoundingVolumeNode& node) {
	char pstrToken[64] = { '\0' };

	std::uint8_t nStrLength = 0;
	std::uint32_t nReads = 0;

    float floatVal{};
	int intVal{};
    dx::XMFLOAT3 float3Val{};
	dx::XMFLOAT4 float4Val{};

	readStream(is, nStrLength);
	readStream(is, pstrToken, nStrLength);

	if (std::strcmp(pstrToken, "<Center:>")) {
		throw std::runtime_error("[BoundingVolume::readOBBCollider]: <Center:> token expected but not found.");
	}

	readStream(is, float3Val);
	const auto center = mu::Vec3(float3Val.x, float3Val.y, float3Val.z);

	readStream(is, nStrLength);
	readStream(is, pstrToken, nStrLength);

	if (std::strcmp(pstrToken, "<Extents:>")) {
		throw std::runtime_error("[BoundingVolume::readOBBCollider]: <Extents:> token expected but not found.");
	}

	readStream(is, float3Val);
	const auto extents = mu::Vec3(float3Val.x, float3Val.y, float3Val.z);

	readStream(is, nStrLength);
	readStream(is, pstrToken, nStrLength);

	if (std::strcmp(pstrToken, "<Orientation:>")) {
		throw std::runtime_error("[BoundingVolume::readOBBCollider]: <Orientation:> token expected but not found.");
	}

	readStream(is, float4Val);
	const auto orientation = mu::NQuat(float4Val.x, float4Val.y, float4Val.z, float4Val.w);

	node.addCollider( BoundingOrientedBox{
		.center = center, .extents = extents, .orientation = orientation
	} );
}

void CollisionSystem::update() {
	// reset collisions from previous frame
	for (auto pBV : components<BoundingVolume>()) {
		pBV->resetCollisions();
	}

	// check for collisions between bounding volumes
	for (auto pBV : components<BoundingVolume>()) {
		for (auto pOtherBV : components<BoundingVolume>()) {
			if (pBV == pOtherBV) {
				continue;
			}

			auto xform = mu::Mat4x4();
			if (auto pCoord = gameEngine::Coord::atC(pBV->entityID().value())) {
				xform = pCoord->get().xform();
			}

			auto otherXform = mu::Mat4x4();
			if (auto pOtherCoord = gameEngine::Coord::atC(pOtherBV->entityID().value())) {
				otherXform = pOtherCoord->get().xform();
			}

			if ( BoundingVolumeNode::collides(
					xform, pBV->root(),
					otherXform, pOtherBV->root()
			) ) {
				pBV->markCollision(pOtherBV);
				pOtherBV->markCollision(pBV);
			}
		}
	}
}