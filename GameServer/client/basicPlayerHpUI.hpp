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

	void setHp(int hp) { hp_ = hp; }
	void setCulled(bool culled) { culled_ = culled; }
	void MU_CALLCONV setPivot(mu::Vec2 pivot) { pivot_ = pivot; }
	void MU_CALLCONV setScale(mu::Vec2 scale) { scale_ = scale; }
	int hp() const { return hp_; }
	void setAmmo(int ammo) { bullet_ = ammo; }
	int ammo() const { return bullet_; }

	EventBus* eventBus() { return &eventBus_; }

private:
	mu::Mat4x4 world_{};
	mu::Mat4x4 textWorld_{};

	mu::Vec2 pivot_{};
	mu::Vec2 scale_{};

	const Texture* pTex_ = nullptr;
	TextImage* pTextHp_ = nullptr;

	EventBus eventBus_{};

	int hp_{};
	int bullet_{};

	bool culled_ = false;
};

#endif	// __basicPlayerHpUI_HPP
