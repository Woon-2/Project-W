#ifndef __billboard_HPP
#define __billboard_HPP

#include "gfx.hpp"

class Billboard {
public:
	void setMesh( const Mesh* pMesh ) { pMesh_ = pMesh; }
	void update( Milliseconds deltaTime );
	void render( GFX& gfx );

private:
	mu::Mat4x4 world_{};	// GFX에 행렬을 전달할 때만 사용된다.
	const Mesh* pMesh_ = nullptr;
};


#endif	// __billboard_HPP