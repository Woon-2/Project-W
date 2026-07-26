#include "rspch.hpp"
#include "TacticalSlime.hpp"

TacticalNpcConfig TacticalSlime::trooperConfig() {
	// Grandbaum 재설계: 평상시 슬라임은 무적이고 ShieldWall 중에만 50% 피해를 받음 → 슬라임이
	// "못 잡는 딜러"가 되지 않도록 공격력 하향(주 위협은 보스). 슬라임은 Slime_Attack1 스킬을 시전하므로
	// 실제 데미지 레버는 attackDamageScale(= lua damage 9 × scale). attackDamage는 레거시 폴백(스킬 없을 때만, 미사용).
	return TacticalNpcConfig{
		.maxHp             = 600.f,
		.moveSpeed         = 2.5f,
		.animRefSpeed      = 3.0f,   // Slime_Walk authored speed (not measured -- default)
		.attackRange       = 2.6f,
		.attackDamage      = 8.f,
		.attackWindupTime  = 0.35s,
		.attackRecoverTime = 0.8s,
		.separationRadius  = 1.5f,  // 방패벽 밀도 결정값. 슬라임이 ~1.5 간격으로 촘촘히 패킹(< barrier 연결거리 2.9 → 연속 벽).
		                            // 슬롯 간격(SHIELD_RING_*_SPACING)은 이 값 이상으로 맞춰야 슬라임이 슬롯에 정착함.
		.separationWeight  = 1.0f,
		.attackDamageScale = 0.3f
	};
}
