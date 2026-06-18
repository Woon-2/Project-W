#include "pch.hpp"
#include "editorController.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace Editor {

namespace {
constexpr float kCasterAccel    = 40.f;   // m/s^2 (follow-mode WASD)
constexpr float kCasterMaxSpeed = 8.f;    // m/s
constexpr float kFreeBaseSpeed  = 6.f;    // m/s (free-fly)
constexpr float kFreeFastMult   = 3.f;    // shift multiplier
constexpr float kPitchLimit     = 1.5f;   // radians (~86 deg)

inline bool isDown(const BYTE* s, int k)            { return (s[k] & 0x80) != 0; }
inline bool isPressed(const BYTE* c, const BYTE* p, int k) {
    return (c[k] & 0x80) && !(p[k] & 0x80);
}

std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

std::wstring fmtVal(float v) {
    wchar_t buf[32];
    swprintf_s(buf, L"%.3f", v);
    return buf;
}
}   // namespace

// ---------------------------------------------------------------------------
// init / UI
// ---------------------------------------------------------------------------

void Controller::init(const InitRefs& refs) {
    skillSystem_ = refs.skillSystem;
    skillCtx_    = refs.skillCtx;
    camera_      = refs.camera;
    uiManager_   = refs.uiManager;
    debugBV_     = refs.debugBV;
    player_      = refs.player;
    goblin_      = refs.goblin;
    hwnd_        = refs.hwnd;
    terrainHeightAt_ = refs.terrainHeightAt;
    flushGpu_    = refs.flushGpu;

    if (player_) { playerSpawnPos_ = player_->pos(); playerSpawnOrient_ = player_->orient(); }
    if (goblin_) { goblinSpawnPos_ = goblin_->pos(); goblinSpawnOrient_ = goblin_->orient(); }

    buildUI();
    selectCharacter(0);

    gSharedLog << "[Editor] Skill editor ready.\n"
               << "  Space=play  LMB=pick hitbox  P=dump diff  R=reset\n"
               << "  Up/Down=field  Left/Right=nudge (Shift=coarse)\n"
               << "  [ ]=time scale  0=pause  F=free camera (WASD+RMB look, Q/E down/up)\n";
    dumpLog();
}

void Controller::buildUI() {
    auto* root = uiManager_->root();

    characterDropdown_ = static_cast<UI::Dropdown*>(
        root->addChild(std::make_unique<UI::Dropdown>()));
    characterDropdown_->name    = "editorCharacterDropdown";
    characterDropdown_->anchor  = UI::Anchors::TopLeft;
    characterDropdown_->pivot   = UI::Pivots::TopLeft;
    characterDropdown_->offsetX = UI::DimValue::px(12.f);
    characterDropdown_->offsetY = UI::DimValue::px(12.f);
    characterDropdown_->width   = UI::DimValue::px(180.f);
    characterDropdown_->zOrder  = 100;
    {
        std::vector<std::string> labels;
        for (const auto& def : kCharacterSkillMap)
            labels.emplace_back(def.label);
        characterDropdown_->setup(std::move(labels));
    }
    characterDropdown_->onSelectionChanged = [this](int idx) { selectCharacter(idx); };

    // Skill dropdown sits to the RIGHT of the character dropdown (side-by-side).
    // (re)built per character; created in rebuildSkillDropdown().

    statusLabel_ = static_cast<UI::Label*>(root->addChild(std::make_unique<UI::Label>()));
    statusLabel_->name    = "editorStatus";
    statusLabel_->anchor  = UI::Anchors::TopLeft;
    statusLabel_->pivot   = UI::Pivots::TopLeft;
    statusLabel_->offsetX = UI::DimValue::px(12.f);
    statusLabel_->offsetY = UI::DimValue::px(52.f);
    statusLabel_->width   = UI::DimValue::px(560.f);
    statusLabel_->height  = UI::DimValue::px(22.f);
    statusLabel_->setFontSize(15.f);
    statusLabel_->setTextColor(0.6f, 1.f, 0.6f, 1.f);

    // Controls cheat-sheet (top-right, out of the way of the left-side panel).
    helpLabel_ = static_cast<UI::Label*>(root->addChild(std::make_unique<UI::Label>()));
    helpLabel_->name    = "editorHelp";
    helpLabel_->anchor  = UI::Anchors::TopRight;
    helpLabel_->pivot   = UI::Pivots::TopRight;
    helpLabel_->offsetX = UI::DimValue::px(-12.f);
    helpLabel_->offsetY = UI::DimValue::px(12.f);
    helpLabel_->width   = UI::DimValue::px(360.f);
    helpLabel_->height  = UI::DimValue::px(280.f);
    helpLabel_->setTextHAlign(UI::TextHAlign::Trailing);
    helpLabel_->setTextVAlign(UI::TextVAlign::Top);
    helpLabel_->setFontSize(14.f);
    helpLabel_->setTextColor(0.85f, 0.9f, 1.f, 1.f);
    helpLabel_->setText(
        L"[ Controls ]\n"
        L"Space  : play / restart skill\n"
        L"LMB    : pick hitbox (click near it)\n"
        L"Esc    : exit hitbox editing\n"
        L"Up/Down: select field\n"
        L"Left/Right: nudge value (Shift=coarse)\n"
        L"[ / ]  : time scale (slow-mo)\n"
        L"0      : pause toggle\n"
        L"F      : free camera toggle\n"
        L"  free: WASD move, RMB look, Q/E down/up\n"
        L"follow: WASD move caster, RMB orbit\n"
        L"P      : dump edit diff to console\n"
        L"R      : reset characters\n"
        L"C      : kill front particle (debug)\n"
        L"T      : put target in line of fire");

    panel_ = static_cast<UI::Panel*>(root->addChild(std::make_unique<UI::Panel>()));
    panel_->name    = "editorPanel";
    panel_->anchor  = UI::Anchors::TopLeft;
    panel_->pivot   = UI::Pivots::TopLeft;
    panel_->offsetX = UI::DimValue::px(12.f);
    panel_->offsetY = UI::DimValue::px(78.f);
    panel_->width   = UI::DimValue::px(420.f);
    panel_->height  = UI::DimValue::px(kMaxPanelLines * 18.f + 10.f);
    panel_->colorTint = { 0.05f, 0.05f, 0.08f, 0.78f };

    for (int i = 0; i < kMaxPanelLines; ++i) {
        auto* lbl = static_cast<UI::Label*>(panel_->addChild(std::make_unique<UI::Label>()));
        lbl->anchor  = UI::Anchors::TopLeft;
        lbl->pivot   = UI::Pivots::TopLeft;
        lbl->offsetX = UI::DimValue::px(8.f);
        lbl->offsetY = UI::DimValue::px(6.f + i * 18.f);
        lbl->width   = UI::DimValue::px(404.f);
        lbl->height  = UI::DimValue::px(18.f);
        lbl->setFontSize(14.f);
        lbl->setTextColor(1.f, 1.f, 1.f, 1.f);
        lbl->visible = false;
        panelLines_[i] = lbl;
    }
}

// ---------------------------------------------------------------------------
// selection / playback
// ---------------------------------------------------------------------------

std::shared_ptr<Object> Controller::casterObj() const {
    if (casterKind_ == CharacterKind::Player) return player_;
    return goblin_;
}

std::shared_ptr<Object> Controller::targetObj() const {
    if (casterKind_ == CharacterKind::Player) return goblin_;
    return player_;
}

void Controller::positionDummyInFront() {
    auto c = casterObj();
    auto t = targetObj();
    if (!c || !t) return;

    // Horizontal forward of the caster.
    mu::Vec3 fwd = c->forward();
    fwd = mu::Vec3(fwd.x(), 0.f, fwd.z());
    const float len = std::sqrt(fwd.x() * fwd.x() + fwd.z() * fwd.z());
    fwd = (len > 1e-4f) ? mu::Vec3(fwd.x() / len, 0.f, fwd.z() / len)
                        : mu::Vec3(0.f, 0.f, 1.f);

    constexpr float kDist = 3.5f;
    const mu::Vec3 base = c->pos();
    float px = base.x() + fwd.x() * kDist;
    float pz = base.z() + fwd.z() * kDist;
    float py = terrainHeightAt_ ? terrainHeightAt_(px, pz) : base.y();

    t->setVelocity({ 0.f, 0.f, 0.f });
    t->setOmega({ 0.f, 0.f, 0.f });
    t->setPos(mu::Vec3(px, py, pz));
    // Face the caster so the dummy looks at the attacker.
    t->setOrient(mu::NQuat(mu::Radian(0.f), mu::Radian(0.f),
                           mu::Radian(std::atan2(-fwd.x(), -fwd.z()))));
}

void Controller::killFrontParticle() {
    // Debug hook for the kill primitive: destroy the first live particle of any
    // active effect. A non-penetrating skill destroys its own particle on hit
    // through the same ParticleSystem::killParticle() path; this lets the
    // primitive (and any Death sub-emitter chain, e.g. an explosion) be observed
    // in isolation before the hitbox plumbing is exercised.
    if (!skillCtx_ || !skillCtx_->vfxById) return;
    for (int v = 0; v < skillCtx_->vfxByIdSize; ++v) {
        ParticleEffect* fx = skillCtx_->vfxById[v];
        if (!fx) continue;
        for (int s = 0; s < fx->systemCount(); ++s) {
            ParticleSystem& sys = fx->system(s);
            if (sys.activeCount() > 0) { sys.killParticle(0); return; }
        }
    }
}

void Controller::clearHitboxSelection() {
    selectedHitboxIdx_ = -1;
    selectedDefIdx_    = -1;
    fieldCursor_       = 0;
    if (draft_.valid()) draft_.buildFields(-1);
}

void Controller::selectCharacter(int idx) {
    if (idx < 0 || idx >= (int)kCharacterSkillMap.size()) return;
    const CharacterDef& def = kCharacterSkillMap[idx];
    casterKind_ = def.kind;

    if (casterKind_ == CharacterKind::Player) { casterId_ = 0; targetId_ = 1; }
    else                                      { casterId_ = 1; targetId_ = 0; }

    resetCharacters();

    // Follow camera tracks the caster.
    if (camera_) camera_->setTargetObject(casterObj());

    // Build the skill list from this character's mapping, keeping only skills
    // that actually exist in the registry.
    skillNames_.clear();
    std::vector<std::string> labels;
    for (std::string_view s : def.skills) {
        if (skillSystem_ && skillSystem_->findAsset(s)) {
            skillNames_.emplace_back(s);
            labels.emplace_back(s);
        }
    }
    rebuildSkillDropdown(labels);

    if (!skillNames_.empty()) selectSkill(0);
    else {
        draft_ = SkillDraft{};
        selectedDefIdx_ = selectedHitboxIdx_ = -1;
        fieldCursor_ = 0;
    }
}

void Controller::rebuildSkillDropdown(const std::vector<std::string>& items) {
    auto* root = uiManager_->root();
    if (skillDropdown_) {
        // The old dropdown's Labels own GPU text textures. removeChild() destroys them
        // immediately, but a prior frame's command list (submitted before this input was
        // processed) may still be reading those textures on the GPU. Freeing them now is a
        // GPU use-after-free → device removed (TDR) → crash that only shows in Release.
        // Drain submitted GPU work first so nothing in flight references these widgets.
        if (flushGpu_) flushGpu_();
        root->removeChild(skillDropdown_);
        skillDropdown_ = nullptr;
    }

    skillDropdown_ = static_cast<UI::Dropdown*>(
        root->addChild(std::make_unique<UI::Dropdown>()));
    skillDropdown_->name    = "editorSkillDropdown";
    skillDropdown_->anchor  = UI::Anchors::TopLeft;
    skillDropdown_->pivot   = UI::Pivots::TopLeft;
    skillDropdown_->offsetX = UI::DimValue::px(200.f);  // right of the character dropdown
    skillDropdown_->offsetY = UI::DimValue::px(12.f);
    skillDropdown_->width   = UI::DimValue::px(180.f);
    skillDropdown_->zOrder  = 99;
    skillDropdown_->setup(items.empty() ? std::vector<std::string>{ "(no skills)" } : items);
    skillDropdown_->onSelectionChanged = [this](int idx) { selectSkill(idx); };
}

void Controller::selectSkill(int idx) {
    if (idx < 0 || idx >= (int)skillNames_.size()) return;
    const SkillAsset* asset = skillSystem_ ? skillSystem_->findAsset(skillNames_[idx]) : nullptr;
    if (!asset) return;

    draft_.load(*asset);
    selectedDefIdx_   = -1;
    selectedHitboxIdx_ = -1;
    fieldCursor_      = 0;
    draft_.buildFields(-1);
}

void Controller::play() {
    if (!skillSystem_ || !skillCtx_) return;
    skillSystem_->interruptAll(casterId_, *skillCtx_);
    selectedHitboxIdx_ = -1;
    if (draft_.valid())
        skillSystem_->startSkillAsset(&draft_.draft(), casterId_, *skillCtx_);
}

void Controller::resetCharacters() {
    auto resetOne = [](Object* o, mu::Vec3 pos, mu::NQuat orient) {
        if (!o) return;
        o->setVelocity({ 0.f, 0.f, 0.f });
        o->setOmega({ 0.f, 0.f, 0.f });
        o->setPos(pos);
        o->setOrient(orient);
        o->setHp(o->maxHp());
        o->setDead(false);
    };
    if (skillSystem_ && skillCtx_) {
        skillSystem_->interruptAll(0, *skillCtx_);
        skillSystem_->interruptAll(1, *skillCtx_);
    }
    resetOne(player_.get(), playerSpawnPos_, playerSpawnOrient_);
    resetOne(goblin_.get(), goblinSpawnPos_, goblinSpawnOrient_);
    if (goblin_) goblin_->setRagdollPendingActivation(false);
    selectedHitboxIdx_ = -1;

    // Drop the test dummy (the target) right in front of the caster, on the ground.
    positionDummyInFront();
}

// ---------------------------------------------------------------------------
// editing
// ---------------------------------------------------------------------------

void Controller::moveCursor(int dir) {
    if (!draft_.valid()) return;
    const int n = (int)draft_.fields().size();
    if (n == 0) return;
    fieldCursor_ = (fieldCursor_ + dir + n) % n;
}

void Controller::nudge(int dir, bool coarse) {
    if (!draft_.valid()) return;
    const auto& fields = draft_.fields();
    if (fieldCursor_ < 0 || fieldCursor_ >= (int)fields.size()) return;

    const auto& f   = fields[fieldCursor_];
    const float step = coarse ? f.coarseStep : f.step;
    bool resort = false;
    draft_.applyDelta(f, dir * step, resort);

    // Live-sync the selected active hitbox so transform/onHit edits appear at once.
    if (selectedHitboxIdx_ >= 0 && selectedDefIdx_ >= 0) {
        if (const SkillHitboxDef* def = draft_.draftHitboxDef(selectedDefIdx_)) {
            skillSystem_->setHitboxLocalOBBs(selectedHitboxIdx_, def->localOBBs);
            skillSystem_->setHitboxOnHit(selectedHitboxIdx_, def->onHit);
        }
    }

    if (resort) {
        draft_.buildFields(selectedDefIdx_);
        const int n = (int)draft_.fields().size();
        if (n > 0) fieldCursor_ = std::min(fieldCursor_, n - 1);
    }
}

void Controller::pickHitbox() {
    if (!skillSystem_ || !camera_ || !uiManager_ || !hwnd_) return;

    POINT pt{};
    GetCursorPos(&pt);
    ScreenToClient(hwnd_, &pt);
    const float mx = static_cast<float>(pt.x);
    const float my = static_cast<float>(pt.y);
    const float sw = uiManager_->screenWidth();
    const float sh = uiManager_->screenHeight();

    mu::Vec3 origin{}, dir{};
    screenToRay(mx, my, sw, sh, camera_->view(), camera_->proj(), origin, dir);
    const Ray ray{ origin, mu::Vec3(mu::NVec3(dir)) };

    skillSystem_->collectActiveHitboxes(activeScratch_);

    // Primary: exact ray vs OBB (closest along the ray).
    // Fallback: nearest projected OBB-center to the cursor within a pixel radius
    //           -- thin/rotated hitboxes are hard to hit exactly, so "click near it" wins.
    constexpr float kPixelRadius = 70.f;

    int   bestRay = -1, bestRayDef = -1;  float bestT   = std::numeric_limits<float>::max();
    int   bestScr = -1, bestScrDef = -1;  float bestPix = kPixelRadius;

    const mu::Mat4x4 view = camera_->view();
    const mu::Mat4x4 proj = camera_->proj();

    for (const auto& ref : activeScratch_) {
        if (!ref.worldOBBs) continue;
        for (const OBB& obb : *ref.worldOBBs) {
            RayHit rh = RaycastOBB(obb, ray);
            if (rh.hit && rh.t >= 0.f && rh.t < bestT) {
                bestT = rh.t; bestRay = ref.hitboxIdx; bestRayDef = ref.defIdx;
            }
            float sx{}, sy{};
            if (worldToScreen(obb.center, view, proj, sw, sh, sx, sy)) {
                const float d = std::hypot(sx - mx, sy - my);
                if (d < bestPix) {
                    bestPix = d; bestScr = ref.hitboxIdx; bestScrDef = ref.defIdx;
                }
            }
        }
    }

    const int idx    = (bestRay >= 0) ? bestRay    : bestScr;
    const int defIdx = (bestRay >= 0) ? bestRayDef : bestScrDef;
    if (idx < 0) return;

    selectedHitboxIdx_ = idx;
    selectedDefIdx_    = defIdx;
    if (selectedDefIdx_ >= 0) {
        draft_.buildFields(selectedDefIdx_);
        fieldCursor_ = 0;
        gSharedLog << "[Editor] selected hitbox def " << selectedDefIdx_
                   << (bestRay >= 0 ? " (ray)\n" : " (proximity)\n");
        dumpLog();
    }
}

void Controller::revalidateSelection() {
    if (selectedHitboxIdx_ < 0 || !skillSystem_) return;
    skillSystem_->collectActiveHitboxes(activeScratch_);
    for (const auto& ref : activeScratch_)
        if (ref.hitboxIdx == selectedHitboxIdx_) return;  // still live
    selectedHitboxIdx_ = -1;  // skill ended / hitbox destroyed; keep def selection for editing
}

// ---------------------------------------------------------------------------
// input
// ---------------------------------------------------------------------------

void Controller::handleInput(const BYTE* cur, const BYTE* prev,
                             long mouseDx, long mouseDy, Milliseconds realDt) {
    const float dtSec   = std::chrono::duration<float>(realDt).count();
    const bool  uiCursor = uiManager_ && uiManager_->needsCursor();
    const bool  shift    = isDown(cur, VK_SHIFT);

    // --- discrete actions ---
    if (isPressed(cur, prev, VK_SPACE))  play();
    if (isPressed(cur, prev, 'P'))       { if (draft_.valid()) draft_.dumpDiff(); }
    if (isPressed(cur, prev, 'R'))       resetCharacters();
    if (isPressed(cur, prev, 'C'))       killFrontParticle();
    if (isPressed(cur, prev, 'T'))       positionDummyInFront();
    if (isPressed(cur, prev, 'F'))       enableFreeCamera(!cameraFree_);
    if (isPressed(cur, prev, VK_ESCAPE)) clearHitboxSelection();

    if (isPressed(cur, prev, VK_UP))    moveCursor(-1);
    if (isPressed(cur, prev, VK_DOWN))  moveCursor(+1);
    if (isPressed(cur, prev, VK_LEFT))  nudge(-1, shift);
    if (isPressed(cur, prev, VK_RIGHT)) nudge(+1, shift);

    if (isPressed(cur, prev, VK_OEM_4)) timeScale_ = std::max(0.05f, timeScale_ * 0.5f);  // '['
    if (isPressed(cur, prev, VK_OEM_6)) timeScale_ = std::min(1.0f,  timeScale_ * 2.0f);  // ']'
    if (isPressed(cur, prev, '0')) {
        if (timeScale_ > 0.f) { prevTimeScale_ = timeScale_; timeScale_ = 0.f; }
        else                  { timeScale_ = (prevTimeScale_ > 0.f) ? prevTimeScale_ : 1.f; }
    }

    // --- mouse pick (left click, not over UI, not while RMB-looking) ---
    const bool rmb = isDown(cur, VK_RBUTTON);
    if (!uiCursor && !rmb && isPressed(cur, prev, VK_LBUTTON)) pickHitbox();

    // --- camera + movement ---
    const float sens = (uiManager_ && uiManager_->screenWidth() > 0.f)
                     ? (mu::pi * 2.f / uiManager_->screenWidth()) : 0.005f;

    if (cameraFree_) {
        if (rmb) {
            freeYaw_   += mouseDx * sens;
            freePitch_  = std::clamp(freePitch_ - mouseDy * sens, -kPitchLimit, kPitchLimit);
        }
        const mu::Vec3 dir{ std::sin(freeYaw_) * std::cos(freePitch_),
                            std::sin(freePitch_),
                            std::cos(freeYaw_) * std::cos(freePitch_) };
        const mu::Vec3 right{ std::cos(freeYaw_), 0.f, -std::sin(freeYaw_) };
        const float spd = kFreeBaseSpeed * (shift ? kFreeFastMult : 1.f) * dtSec;

        mu::Vec3 move{ 0.f, 0.f, 0.f };
        if (isDown(cur, 'W')) move = move + dir;
        if (isDown(cur, 'S')) move = move - dir;
        if (isDown(cur, 'D')) move = move + right;
        if (isDown(cur, 'A')) move = move - right;
        if (isDown(cur, 'E')) move = move + mu::Vec3{ 0.f, 1.f, 0.f };
        if (isDown(cur, 'Q')) move = move - mu::Vec3{ 0.f, 1.f, 0.f };
        freeEye_ = freeEye_ + move * spd;
    }
    else if (!uiCursor) {
        // Follow mode: RMB-drag orbits the camera; WASD drives the caster.
        if (rmb) {
            camYaw_   = mu::Radian(static_cast<float>(camYaw_) + mouseDx * sens);
            camPitch_ = mu::Radian(std::clamp(
                static_cast<float>(camPitch_) + mouseDy * sens, -mu::pi * 0.16f, mu::pi * 0.3f));
        }

        auto caster = casterObj();
        if (caster && !caster->isDead()) {
            const int mx = (isDown(cur, 'D') ? 1 : 0) - (isDown(cur, 'A') ? 1 : 0);
            const int mz = (isDown(cur, 'W') ? 1 : 0) - (isDown(cur, 'S') ? 1 : 0);
            if (mx || mz) {
                const mu::Vec3 mdir = mu::Vec3(mu::NVec3(
                    static_cast<float>(mx) * caster->right() +
                    static_cast<float>(mz) * caster->forward()));
                const mu::Vec3 v = caster->velocity();
                float nx = v.x() + mdir.x() * kCasterAccel * dtSec;
                float nz = v.z() + mdir.z() * kCasterAccel * dtSec;
                const float h2 = nx * nx + nz * nz;
                if (h2 > kCasterMaxSpeed * kCasterMaxSpeed) {
                    const float s = kCasterMaxSpeed / std::sqrt(h2);
                    nx *= s; nz *= s;
                }
                caster->setVelocity(mu::Vec3(nx, v.y(), nz));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// camera
// ---------------------------------------------------------------------------

void Controller::enableFreeCamera(bool enable) {
    if (enable == cameraFree_) return;
    cameraFree_ = enable;
    if (!camera_) return;
    if (enable) {
        freeEye_ = camera_->eye();
        const mu::Vec3 d = mu::Vec3(mu::NVec3(camera_->at() - camera_->eye()));
        freeYaw_   = std::atan2(d.x(), d.z());
        freePitch_ = std::asin(std::clamp(d.y(), -1.f, 1.f));
    }
}

void Controller::updateCamera(Milliseconds realDt) {
    if (!camera_) return;
    if (cameraFree_) {
        const mu::Vec3 dir{ std::sin(freeYaw_) * std::cos(freePitch_),
                            std::sin(freePitch_),
                            std::cos(freeYaw_) * std::cos(freePitch_) };
        camera_->setView(freeEye_, freeEye_ + dir);
    }
    else {
        camera_->setOffsetFromTargetPreRotation(
            mu::NQuat(mu::Radian(0.f), camPitch_, camYaw_));
        camera_->update(realDt);
    }
}

// ---------------------------------------------------------------------------
// per-frame refresh
// ---------------------------------------------------------------------------

void Controller::refresh() {
    revalidateSelection();
    if (skillSystem_ && debugBV_)
        skillSystem_->renderDebugHitboxes(*debugBV_, selectedHitboxIdx_);
    refreshPanel();
}

void Controller::refreshPanel() {
    if (statusLabel_) {
        std::wstring s = L"Skill: ";
        s += draft_.valid() ? widen(draft_.name()) : L"(none)";
        wchar_t buf[200];
        auto t = targetObj();
        const int thp  = t ? t->hp()    : 0;
        const int tmax = t ? t->maxHp() : 0;
        const unsigned destroyed = skillSystem_ ? skillSystem_->debugStats().particlesDestroyedOnHit : 0u;
        swprintf_s(buf, L"  |  scale %.2f%s  |  cam:%s  |  target HP %d/%d  |  destroyed %u  |  %s",
                   timeScale_,
                   (timeScale_ <= 0.f ? L" PAUSED" : L""),
                   (cameraFree_ ? L"FREE" : L"FOLLOW"),
                   thp, tmax, destroyed,
                   (selectedDefIdx_ >= 0 ? L"editing def (Esc to exit)" : L"click a hitbox to edit"));
        s += buf;
        statusLabel_->setText(s);
    }

    static const std::vector<SkillDraft::Field> kNoFields;
    const std::vector<SkillDraft::Field>& fields = draft_.valid() ? draft_.fields() : kNoFields;
    for (int i = 0; i < kMaxPanelLines; ++i) {
        UI::Label* lbl = panelLines_[i];
        if (!lbl) continue;
        if (i < (int)fields.size()) {
            const auto& f = fields[i];
            const float cur = draft_.readDraft(f);
            const float org = draft_.readOriginal(f);
            std::wstring line = (i == fieldCursor_ ? L"> " : L"  ");
            line += widen(f.label) + L": " + fmtVal(cur);
            if (cur != org) line += L"  (" + fmtVal(org) + L" d " + fmtVal(cur - org) + L")";
            lbl->setText(line);
            if (i == fieldCursor_)      lbl->setTextColor(1.f, 0.85f, 0.1f, 1.f);
            else if (cur != org)        lbl->setTextColor(0.6f, 0.85f, 1.f, 1.f);
            else                        lbl->setTextColor(1.f, 1.f, 1.f, 1.f);
            lbl->visible = true;
        }
        else lbl->visible = false;
    }
}

}   // namespace Editor
