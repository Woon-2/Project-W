#include "billboard.hpp"
#include "errorHandling.hpp"

void Billboard::update( Milliseconds deltaTime ) {
	// 딱히 업데이트 할 내용이 없는 것 같음 아마도?
}

void Billboard::render( GFX& gfx ) {
	if ( pMesh_ ) {
		gfx.addDrawEvent( BillboardPipeline::DrawEvent{
			.world = world_,
			.mesh = pMesh_,
			.subMesh = &pMesh_->subMeshes.at( L"PointMesh_SubMesh" )
		} );
	}
}

void MU_CALLCONV Billboard::setPos( mu::Vec3 newPos ) {
	for ( auto& snapshot : physicSnapshots_ ) {
		snapshot.pos = newPos;
	}
}