#ifndef __basicPlayerHpUI_HPP
#define __basicPlayerHpUI_HPP

#include "gfx.hpp"
#include "event.hpp"

class BasicPlayerHpUI {
public:
	class EventBus : public IEventBus {
	public:
		void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override;
	};

	BasicPlayerHpUI();
	void setTexture( const Texture* pTex ) { pTex_ = pTex; }
	void setTextImage( TextImage* pTextImg ) { pTextHp_ = pTextImg; }
	void update( Milliseconds deltaTime, GFX& gfx, FontHandle* pFontHandle );
	void render( GFX& gfx );

	int hp() const { return hp_; }
	int ammo() const { return bullet_; }

	EventBus* eventBus() { return &eventBus_; }

private:
	mu::Mat4x4 world_{};
	mu::Mat4x4 textWorld_{};
	const Texture* pTex_ = nullptr;
	TextImage* pTextHp_ = nullptr;

	EventBus eventBus_{};

	int hp_ = 100;
	int bullet_ = 30;
};

#endif	// __basicPlayerHpUI_HPP
