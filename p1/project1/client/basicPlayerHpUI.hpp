#ifndef __basicPlayerHpUI_HPP
#define __basicPlayerHpUI_HPP

#include "pch.hpp"
#include "gfx.hpp"

class BasicPlayerHpUI {
public:
	BasicPlayerHpUI();
	void setTexture( const Texture* pTex ) { pTex_ = pTex; }
	void update( Milliseconds deltaTime );
	void render( GFX& gfx );

private:
	mu::Mat4x4 world_{};
	const Texture* pTex_ = nullptr;
};

#endif	// __basicPlayerHpUI_HPP
