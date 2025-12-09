#include "pch.hpp"
#include "basicPlayerHpUI.hpp"
#include "errorHandling.hpp"
#include "gfxUtil.hpp"

BasicPlayerHpUI::BasicPlayerHpUI() {
	mu::Vec3 scale{ 1024.0f / 2.0f, 64.0f / 2.0f, 1.0f };
	mu::Vec3 translation{ 512.0f, 40.0f, 0.0f };
	world_ = mu::Mat4x4( mu::scale( scale ) ) * mu::translate( translation );
	
	mu::Vec3 scale2{ 512.0f, 256.0f, 1.0f };
	textWorld_ = mu::Mat4x4( mu::scale( scale2 ) );
}

void BasicPlayerHpUI::update( Milliseconds deltaTime, GFX& gfx, FontHandle* pFontHandle ) {
	int iTextWidth = 0;
	int iTextHeight = 0;
	WCHAR	wchTxt[64] = {};
	DWORD	dwTxtLen = swprintf_s( wchTxt, L"Current FrameRate:" );
	std::ranges::fill( pTextHp_->pData, 0 );
	gfx.WriteTextToBitmap( pTextHp_, pTextHp_->width, pTextHp_->height, pTextHp_->width * 4, &iTextWidth, &iTextHeight, nullptr, wchTxt, dwTxtLen );
	gfx.UpdateTextureWithTextImage( pTextHp_, pTextHp_->width, pTextHp_->height );
	gfx.UpdateTexure( pTextHp_->texture.res.Get(), pTextHp_->textureUpload.res.Get() );
}

void BasicPlayerHpUI::render( GFX& gfx ) {
	//if ( pTex_ ) {
	//	gfx.addDrawEvent( UIPipeline::DrawEvent{
	//		.world = world_,
	//		.pTex = pTex_
	//	} );
	//}

	if ( pTextHp_ ) {
		gfx.addDrawEvent( UIPipeline::DrawEvent{
			.world = textWorld_,
			.pTex = &pTextHp_->texture,
			.pTextImage = pTextHp_
		} );
	}
}

