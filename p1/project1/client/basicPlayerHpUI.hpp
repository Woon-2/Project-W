#ifndef __basicPlayerHpUI_HPP
#define __basicPlayerHpUI_HPP

#include "gfx.hpp"

class BasicPlayerHpUI {
public:
	BasicPlayerHpUI();
	void setTexture( const Texture* pTex ) { pTex_ = pTex; }
	void setTextImage( TextImage* pTextImg ) { pTextHp_ = pTextImg; }
	void update( Milliseconds deltaTime, GFX& gfx, FontHandle* pFontHandle );
	void render( GFX& gfx );

private:
	mu::Mat4x4 world_{};
	mu::Mat4x4 textWorld_{};
	const Texture* pTex_ = nullptr;
	TextImage* pTextHp_ = nullptr;
};

#endif	// __basicPlayerHpUI_HPP
