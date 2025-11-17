#ifndef __billboard_HPP
#define __billboard_HPP

#include "pch.hpp"
#include "gfx.hpp"
#include "physics.hpp"

class Billboard {
public:
	void setMesh( const Mesh* pMesh ) { pMesh_ = pMesh; }
	void update( Milliseconds deltaTime );
	void render( GFX& gfx );

	void MU_CALLCONV setPos( mu::Vec3 newPos );
	mu::Vec3 MU_CALLCONV pos() const { return physicSnapshots_.back().pos; }

private:
	mu::Mat4x4 world_{};	// GFX에 행렬을 전달할 때만 사용된다.
	const Mesh* pMesh_ = nullptr;

	std::list<PhysicSnapshot> physicSnapshots_ = std::list<PhysicSnapshot>( 1u );
};


#endif	// __billboard_HPP