#include "pch.hpp"
#include "damageNumberSystem.hpp"
#include "gfx.hpp"
#include "camera.hpp"
#include "ui/digitAtlas.hpp"
#include <algorithm>
#include <random>

namespace {

float randRange(float lo, float hi) {
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng);
}

float easeOutCubic(float x) {
    const float inv = 1.f - std::clamp(x, 0.f, 1.f);
    return 1.f - inv * inv * inv;
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

// Scale punch: a sharp entrance spike defined by hardcoded keyframes (ms -> scale).
// Sharp peak near 16ms, settling back to 1.0 by ~170ms. Piecewise-linear.
struct PunchKey { float ms; float scale; };
constexpr PunchKey kPunchKeys[] = {
    { 0.f,   1.00f },
    { 16.f,  1.65f },
    { 33.f,  2.80f },
    { 66.f,  2.55f },
    { 110.f, 1.12f },
    { 160.f, 0.98f },
    { 220.f, 1.00f },
};

float samplePunch(float ms) {
    if (ms <= kPunchKeys[0].ms) return kPunchKeys[0].scale;
    constexpr int n = static_cast<int>(sizeof(kPunchKeys) / sizeof(kPunchKeys[0]));
    if (ms >= kPunchKeys[n - 1].ms) return 1.f;        // after the window: no punch
    for (int i = 1; i < n; ++i) {
        if (ms <= kPunchKeys[i].ms) {
            const float t = (ms - kPunchKeys[i - 1].ms) / (kPunchKeys[i].ms - kPunchKeys[i - 1].ms);
            return lerp(kPunchKeys[i - 1].scale, kPunchKeys[i].scale, t);
        }
    }
    return 1.f;
}

// impactScale (punch strength) scales only the punch's deviation from 1.0,
// keeping it independent of base size and perspective scale.
float punchMul(float ms, float impactScale) {
    return 1.f + (samplePunch(ms) - 1.f) * impactScale;
}

}  // namespace

void DamageNumberSystem::init(const Texture* digitAtlas, const DamageNumberTuning& tuning) {
    atlas_   = digitAtlas;
    tuning_  = tuning;
    pool_.assign(kMaxActive, DamageNumber{});
    activeCount_ = 0;
}

void DamageNumberSystem::clear() {
    activeCount_ = 0;
}

int DamageNumberSystem::oldestIndex() const {
    int best = 0;
    float bestAge = -1.f;
    for (int i = 0; i < activeCount_; ++i) {
        if (pool_[i].age > bestAge) { bestAge = pool_[i].age; best = i; }
    }
    return best;
}

void DamageNumberSystem::spawn(mu::Vec3 worldPos, int amount, DamageKind kind, std::uint16_t targetId) {
    if (amount <= 0 || pool_.empty()) return;

    // Accumulate: if this target already has a recent number, merge into it and
    // re-pop so the player sees "hit again" without spawning a new glyph stack.
    for (int i = 0; i < activeCount_; ++i) {
        DamageNumber& dn = pool_[i];
        if (dn.targetId == targetId && dn.kind == kind && dn.popAge <= tuning_.mergeWindow) {
            dn.value += amount;
            dn.popAge = 0.f;            // re-pop only; keep age so it keeps floating
            dn.worldAnchor = worldPos;  // follow the latest hit point
            return;
        }
    }

    const int idx = (activeCount_ < kMaxActive) ? activeCount_++ : oldestIndex();
    DamageNumber& dn = pool_[idx];
    dn.worldAnchor  = worldPos;
    dn.value        = amount;
    dn.kind         = kind;
    dn.age          = 0.f;
    dn.popAge       = 0.f;
    dn.jitterX      = randRange(-tuning_.jitterXMax, tuning_.jitterXMax);
    dn.startYJitter = randRange(-tuning_.startYJitterMax, tuning_.startYJitterMax);
    dn.targetId     = targetId;
}

void DamageNumberSystem::update(float dtSec) {
    for (int i = 0; i < activeCount_; ) {
        DamageNumber& dn = pool_[i];
        dn.age    += dtSec;
        dn.popAge += dtSec;
        if (dn.age >= tuning_.lifetime) {
            pool_[i] = pool_[activeCount_ - 1];   // swap-remove (order irrelevant)
            --activeCount_;
        } else {
            ++i;
        }
    }
}

void DamageNumberSystem::render(GFX& gfx, const Camera& cam, float screenW, float screenH, float uiScale) {
    if (!atlas_ || activeCount_ == 0) return;

    const mu::Mat4x4 viewProj = cam.view() * cam.proj();
    const float lifetime      = tuning_.lifetime;

    for (int i = 0; i < activeCount_; ++i) {
        const DamageNumber& dn = pool_[i];

        // Inline projection (vs worldToScreen) to also obtain clip.w (view depth)
        // for perspective scaling.
        const mu::Vec4 clip = mu::Vec4(dn.worldAnchor, 1.f) * viewProj;
        if (clip.w() <= 0.f) continue;                                   // behind camera
        const float ndcX = clip.x() / clip.w();
        const float ndcY = clip.y() / clip.w();
        if (ndcX < -1.f || ndcX > 1.f || ndcY < -1.f || ndcY > 1.f) continue;  // off-screen
        const float sx = (ndcX + 1.f) * 0.5f * screenW;
        const float sy = (1.f - ndcY) * 0.5f * screenH;

        // Perspective: nearer (smaller clip.w) -> bigger; clamped to a sane range.
        const float perspScale = std::clamp(tuning_.perspRefDepth / clip.w(),
                                            tuning_.perspScaleMin, tuning_.perspScaleMax);

        const float ageN = std::clamp(dn.age / lifetime, 0.f, 1.f);

        // Float up (screen pixels), fast early then easing out.
        const float up = tuning_.floatUpPx * uiScale * easeOutCubic(ageN);

        // Trailing fade + slight shrink over the last fadeOutDuration seconds.
        float alpha    = 1.f;
        float endScale = 1.f;
        const float fadeStart = lifetime - tuning_.fadeOutDuration;
        if (dn.age > fadeStart && tuning_.fadeOutDuration > 0.f) {
            const float f = std::clamp((dn.age - fadeStart) / tuning_.fadeOutDuration, 0.f, 1.f);
            alpha    = 1.f - f;
            endScale = lerp(1.f, tuning_.scaleEnd, f);
        }

        // Scale punch: sharp entrance spike (keyframed), independent of base size
        // and perspective. popAge resets on merge, so the punch re-fires on each hit.
        const float punch = punchMul(dn.popAge * 1000.f, tuning_.impactScale);
        const float baseH = (dn.value >= tuning_.bigHitThreshold)
                                ? tuning_.glyphHeightBig : tuning_.glyphHeightSmall;
        const float scaledBaseH = baseH * uiScale;
        // Clamp only the steady base (size * perspective) for readability; the punch
        // then multiplies the clamped base IN FULL, so the spike is always visible
        // (the clamp must not swallow the punch).
        const float baseSize = std::clamp(scaledBaseH * perspScale,
                                          tuning_.glyphHeightMin * uiScale,
                                          tuning_.glyphHeightMax * uiScale);
        const float glyphH = baseSize * punch * endScale;

        // White-base glyphs tinted per kind; alpha carries the fade.
        XMFLOAT4 color{};
        switch (dn.kind) {
        case DamageKind::PlayerHit:
            color = XMFLOAT4{ 1.0f, 0.30f, 0.25f, alpha };   // red: incoming damage
            break;
        case DamageKind::StrongholdHit:
            color = XMFLOAT4{ 1.0f, 0.74f, 0.18f, alpha };   // gold: structure damage
            break;
        case DamageKind::EnemyHit:
        default:
            color = XMFLOAT4{ 0.35f, 0.72f, 1.0f, alpha };   // blue: monster damage
            break;
        }

        // Top-left-origin pixels; smaller Y is higher, so subtract the float-up.
        // jitter scales with perspective so near (big) numbers spread wider.
        const float cx = std::clamp(sx + dn.jitterX * uiScale * perspScale, 0.f, screenW);
        const float cy = std::clamp(sy + dn.startYJitter * uiScale - up, 0.f, screenH);

        DigitAtlas::emitNumber(gfx, atlas_, cx, cy, glyphH, screenH,
                               dn.value, color, DigitAtlas::Align::Center);
    }
}
