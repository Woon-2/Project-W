#include "game/physicsSystem.hpp"

RigidBody::RigidBody(const ecs::Entity& entity) NOEXCEPT
	: Component(entity) {
	mass_ = 1;
	cornerLocation_ = 0;
}

void MU_CALLCONV RigidBody::addForce(mu::Vec3 force) NOEXCEPT
{
	force_ += force;
}

void MU_CALLCONV RigidBody::updateRigid(float dt, float friction) NOEXCEPT
{
	updateForce(dt, friction);
	updateAngular(dt);
}

void RigidBody::updateForce(float dt, float friction) NOEXCEPT
{
	oldPosition_ = position_;

	// 선운동량과 위치
	position_ += momentum_ / mass_ * dt;
	momentum_ += force_ * dt;

	// 속도 계산
	velocity_ += (force_ / mass_) * dt;	// 가속도 적용 - acceleration = force / mass;

	// 마찰력 적용 (속도에 비례하는 반대 방향의 힘)
	mu::Vec3 frictionForce = -friction * velocity_;

	// 마찰력을 선운동량과 속도에 적용
	momentum_ += frictionForce * dt;
	velocity_ += (frictionForce / mass_) * dt;	// 가속도 적용

	// 속도가 아주 작을 경우 속도를 0으로 설정
	static constexpr auto epsilon = 0.0002f;
	if (velocity_.len() < epsilon) {
		velocity_ = mu::Vec3(0.0f, 0.0f, 0.0f); // 속도를 0으로 설정
		momentum_ = mu::Vec3(0.0f, 0.0f, 0.0f); // 운동량도 0으로 설정
	}

	// 힘을 다 사용했으므로 초기화
	force_ = mu::Vec3(0.0f, 0.0f, 0.0f);
}

void RigidBody::updateAngular(float dt) NOEXCEPT
{
}

void PhysicsSystem::update(float deltaTime)
{
	for (auto& pRigidBody : components<RigidBody>()) {
		if (pRigidBody) {
			pRigidBody->updateRigid(deltaTime, 4.5f);
		}
	}
}

bool MU_CALLCONV contains(const BoundingCapsule& a, const mu::Vec3& point) {
	auto aNormal = mu::Vec3(mu::NVec3(a.tip - a.base));
	auto aLineEndOffset = aNormal * a.radius;
	auto aA = a.base + aLineEndOffset;
	auto aB = a.tip - aLineEndOffset;

	auto best = ClosestPointOnLineSegment(aA, aB, point);

	return (best - point).len2() < a.radius * a.radius;
}

bool MU_CALLCONV intersects(const BoundingCapsule& a, const BoundingCapsule& b) {
	// capsule A:
	mu::Vec3 as = mu::Vec3(mu::Vec2(1.f, 1.f), 1.f);
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

	auto penetrationNormal = bestA - bestB;
	auto len = penetrationNormal.len();
	penetrationNormal /= len;  // normalize
	float penetrationDepth = a.radius + b.radius - len;

	return penetrationDepth > 0;
}

bool MU_CALLCONV intersects(const BoundingOrientedBox& lhs, const BoundingOrientedBox& rhs) {
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

bool MU_CALLCONV intersects(const BoundingOrientedBox& lhs, const BoundingFrustum& rhs) {
	// transform box into frustum space
	auto R = mu::Mat3x3(lhs.orientation * rhs.orientation.dual());
	auto c = lhs.center - rhs.origin;
	c *= R;

	R = mu::transpose(R);
	const auto x0 = c - R.row(0) * lhs.extents.x();
	const auto x1 = c + R.row(0) * lhs.extents.x();
	const auto y0 = c - R.row(1) * lhs.extents.y();
	const auto y1 = c + R.row(1) * lhs.extents.y();
	const auto z0 = c - R.row(2) * lhs.extents.z();
	const auto z1 = c + R.row(2) * lhs.extents.z();

	// project all corners of box via perspective projection matrix
	const auto persp = mu::persp(rhs.fovy, rhs.aspect, rhs.nearZ, rhs.farZ);

	const auto p0 = mu::Vec3(mu::Vec4(x0, 1.0f) * persp);
	const auto p0w = p0.w();
	const auto p1 = mu::Vec3(mu::Vec4(x1, 1.0f) * persp);
	const auto p1w = p1.w();
	const auto p2 = mu::Vec3(mu::Vec4(y0, 1.0f) * persp);
	const auto p2w = p2.w();
	const auto p3 = mu::Vec3(mu::Vec4(y1, 1.0f) * persp);
	const auto p3w = p3.w();
	const auto p4 = mu::Vec3(mu::Vec4(z0, 1.0f) * persp);
	const auto p4w = p4.w();
	const auto p5 = mu::Vec3(mu::Vec4(z1, 1.0f) * persp);
	const auto p5w = p5.w();

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
	
	return hasIntersection;
}

bool MU_CALLCONV Collider::intersects(const Collider& other) const {
	switch (type_) {
	case Type::Capsule:
		switch (other.type_) {
		default:
			break;
		}
	case Type::Box:
		switch (other.type_) {
		case Type::Box:
			return ::intersects(box_, other.box_);
		case Type::Frustum:
			return ::intersects(box_, other.frustum_);
		default:
			break;
		}
	case Type::Frustum:
		switch (other.type_) {
		case Type::Box:
			return ::intersects(other.box_, frustum_);
		default:
			break;
		}
	}

	throw std::runtime_error("requested coolision detection is currently not implemented.");
}

bool MU_CALLCONV Collider::contains(const mu::Vec3& point) const {
	switch (type_) {
	case Type::Capsule:
		return ::contains(capsule_, point);
	default:
		break;
	}

	throw std::runtime_error("requested coolision detection is currently not implemented.");
}