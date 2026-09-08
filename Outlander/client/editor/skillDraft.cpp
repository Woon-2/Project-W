#include "pch.hpp"
#include "skillDraft.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

namespace Editor {

// ---------------------------------------------------------------------------
// load
// ---------------------------------------------------------------------------

void SkillDraft::load(const SkillAsset& src) {
    original_ = src;
    draft_    = src;
    fields_.clear();
    selectedDefIdx_ = -1;
    loaded_ = true;

    draftEventOrig_.resize(draft_.timeline.size());
    std::iota(draftEventOrig_.begin(), draftEventOrig_.end(), 0);
}

// ---------------------------------------------------------------------------
// field reading
// ---------------------------------------------------------------------------

float SkillDraft::readField(const SkillAsset& a, const Field& f,
                            const std::vector<int>* eventMap) {
    using FT = FieldType;

    auto obbOf = [&]() -> const OBB& {
        return a.hitboxDefs[f.defIdx].localOBBs[f.obbIdx];
    };
    auto eulerOf = [&]() -> mu::Vec3 {
        const auto& v = a.hitboxDefs[f.defIdx].localOBBEulerDeg;
        return (f.obbIdx < (int)v.size()) ? v[f.obbIdx] : mu::Vec3{ 0.f, 0.f, 0.f };
    };
    auto onHitOf = [&]() -> const OnHitDef& {
        return a.hitboxDefs[f.defIdx].onHit;
    };
    auto eventTime = [&]() -> float {
        int idx = f.eventIdx;
        if (eventMap) {  // reading original_ through the draft->original map
            if (idx < 0 || idx >= (int)eventMap->size()) return 0.f;
            idx = (*eventMap)[idx];
        }
        if (idx < 0 || idx >= (int)a.timeline.size()) return 0.f;
        return a.timeline[idx].time.count();
    };

    switch (f.type) {
    case FT::CenterX: return obbOf().center.x();
    case FT::CenterY: return obbOf().center.y();
    case FT::CenterZ: return obbOf().center.z();
    case FT::HalfX:   return obbOf().halfExtents.x();
    case FT::HalfY:   return obbOf().halfExtents.y();
    case FT::HalfZ:   return obbOf().halfExtents.z();
    case FT::Yaw:     return eulerOf().x();
    case FT::Pitch:   return eulerOf().y();
    case FT::Roll:    return eulerOf().z();
    case FT::Damage:          return static_cast<float>(onHitOf().damage);
    case FT::ImpulseStrength: return onHitOf().impulseStrength;
    case FT::ImpulseDirX:     return onHitOf().impulseDirLocal.x();
    case FT::ImpulseDirY:     return onHitOf().impulseDirLocal.y();
    case FT::ImpulseDirZ:     return onHitOf().impulseDirLocal.z();
    case FT::SpawnTimeMs:     return eventTime();
    case FT::DestroyTimeMs:   return eventTime();
    case FT::TotalDurationMs: return a.totalDuration.count();
    }
    return 0.f;
}

float SkillDraft::readDraft(const Field& f) const {
    return readField(draft_, f, nullptr);
}

float SkillDraft::readOriginal(const Field& f) const {
    const bool timeField = (f.type == FieldType::SpawnTimeMs ||
                            f.type == FieldType::DestroyTimeMs);
    return readField(original_, f, timeField ? &draftEventOrig_ : nullptr);
}

// ---------------------------------------------------------------------------
// editing
// ---------------------------------------------------------------------------

void SkillDraft::rebuildOrient(int defIdx, int obbIdx) {
    auto& def = draft_.hitboxDefs[defIdx];
    if (obbIdx >= (int)def.localOBBEulerDeg.size()) return;
    const mu::Vec3 e = def.localOBBEulerDeg[obbIdx];  // yaw, pitch, roll (deg)
    def.localOBBs[obbIdx].orient =
        mu::NQuat(mu::Degree{ e.z() }, mu::Degree{ e.y() }, mu::Degree{ e.x() });
}

void SkillDraft::resortTimeline() {
    const int n = (int)draft_.timeline.size();
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return draft_.timeline[a].time.count() < draft_.timeline[b].time.count();
    });

    std::vector<TimelineEvent> newTl;
    std::vector<int>           newMap;
    newTl.reserve(n);
    newMap.reserve(n);
    for (int i = 0; i < n; ++i) {
        newTl.push_back(draft_.timeline[order[i]]);
        newMap.push_back(draftEventOrig_[order[i]]);
    }
    draft_.timeline = std::move(newTl);
    draftEventOrig_ = std::move(newMap);
}

bool SkillDraft::applyDelta(const Field& f, float delta, bool& outResort) {
    using FT = FieldType;
    outResort = false;
    if (f.defIdx >= 0 && f.defIdx >= (int)draft_.hitboxDefs.size()) return false;

    auto setCenter = [&](int axis, float d) {
        auto& c = draft_.hitboxDefs[f.defIdx].localOBBs[f.obbIdx].center;
        mu::Vec3 nc(c.x(), c.y(), c.z());
        nc = mu::Vec3(axis == 0 ? nc.x() + d : nc.x(),
                      axis == 1 ? nc.y() + d : nc.y(),
                      axis == 2 ? nc.z() + d : nc.z());
        c = nc;
    };
    auto setHalf = [&](int axis, float d) {
        auto& h = draft_.hitboxDefs[f.defIdx].localOBBs[f.obbIdx].halfExtents;
        float x = std::max(0.f, axis == 0 ? h.x() + d : h.x());
        float y = std::max(0.f, axis == 1 ? h.y() + d : h.y());
        float z = std::max(0.f, axis == 2 ? h.z() + d : h.z());
        h = mu::Vec3(x, y, z);
    };
    auto setEuler = [&](int axis, float d) {
        auto& e = draft_.hitboxDefs[f.defIdx].localOBBEulerDeg[f.obbIdx];
        e = mu::Vec3(axis == 0 ? e.x() + d : e.x(),
                     axis == 1 ? e.y() + d : e.y(),
                     axis == 2 ? e.z() + d : e.z());
        rebuildOrient(f.defIdx, f.obbIdx);
    };
    auto setDir = [&](int axis, float d) {
        auto& dir = draft_.hitboxDefs[f.defIdx].onHit.impulseDirLocal;
        dir = mu::Vec3(axis == 0 ? dir.x() + d : dir.x(),
                       axis == 1 ? dir.y() + d : dir.y(),
                       axis == 2 ? dir.z() + d : dir.z());
    };
    auto setTime = [&](int idx, float d) {
        if (idx < 0 || idx >= (int)draft_.timeline.size()) return;
        float ms = std::max(0.f, draft_.timeline[idx].time.count() + d);
        draft_.timeline[idx].time = Milliseconds{ ms };
        resortTimeline();
        outResort = true;
    };

    switch (f.type) {
    case FT::CenterX: setCenter(0, delta); break;
    case FT::CenterY: setCenter(1, delta); break;
    case FT::CenterZ: setCenter(2, delta); break;
    case FT::HalfX:   setHalf(0, delta);   break;
    case FT::HalfY:   setHalf(1, delta);   break;
    case FT::HalfZ:   setHalf(2, delta);   break;
    case FT::Yaw:     setEuler(0, delta);  break;
    case FT::Pitch:   setEuler(1, delta);  break;
    case FT::Roll:    setEuler(2, delta);  break;
    case FT::Damage:
        draft_.hitboxDefs[f.defIdx].onHit.damage =
            std::max(0, draft_.hitboxDefs[f.defIdx].onHit.damage + (i32t)std::lround(delta));
        break;
    case FT::ImpulseStrength: {
        auto& s = draft_.hitboxDefs[f.defIdx].onHit.impulseStrength;
        s = std::max(0.f, s + delta);
        break;
    }
    case FT::ImpulseDirX: setDir(0, delta); break;
    case FT::ImpulseDirY: setDir(1, delta); break;
    case FT::ImpulseDirZ: setDir(2, delta); break;
    case FT::SpawnTimeMs:   setTime(f.eventIdx, delta); break;
    case FT::DestroyTimeMs: setTime(f.eventIdx, delta); break;
    case FT::TotalDurationMs: {
        float ms = std::max(0.f, draft_.totalDuration.count() + delta);
        draft_.totalDuration = Milliseconds{ ms };
        break;
    }
    }
    return true;
}

const SkillHitboxDef* SkillDraft::draftHitboxDef(int defIdx) const {
    if (defIdx < 0 || defIdx >= (int)draft_.hitboxDefs.size()) return nullptr;
    return &draft_.hitboxDefs[defIdx];
}

// ---------------------------------------------------------------------------
// field list
// ---------------------------------------------------------------------------

void SkillDraft::buildFields(int defIdx) {
    selectedDefIdx_ = defIdx;
    fields_.clear();

    auto add = [&](std::string label, FieldType t, int obbIdx, int eventIdx,
                   float step, float coarse) {
        Field f;
        f.label      = std::move(label);
        f.type       = t;
        f.defIdx     = defIdx;
        f.obbIdx     = obbIdx;
        f.eventIdx   = eventIdx;
        f.step       = step;
        f.coarseStep = coarse;
        fields_.push_back(std::move(f));
    };

    if (defIdx >= 0 && defIdx < (int)draft_.hitboxDefs.size()) {
        const SkillHitboxDef& def = draft_.hitboxDefs[defIdx];
        const int obbCount = (int)def.localOBBs.size();

        for (int o = 0; o < obbCount; ++o) {
            const std::string p = (obbCount > 1) ? ("OBB" + std::to_string(o) + " ") : "";
            add(p + "center.x", FieldType::CenterX, o, -1, 0.05f, 0.5f);
            add(p + "center.y", FieldType::CenterY, o, -1, 0.05f, 0.5f);
            add(p + "center.z", FieldType::CenterZ, o, -1, 0.05f, 0.5f);
            add(p + "half.x",   FieldType::HalfX,   o, -1, 0.05f, 0.5f);
            add(p + "half.y",   FieldType::HalfY,   o, -1, 0.05f, 0.5f);
            add(p + "half.z",   FieldType::HalfZ,   o, -1, 0.05f, 0.5f);
            add(p + "yaw",      FieldType::Yaw,     o, -1, 1.0f,  5.0f);
            add(p + "pitch",    FieldType::Pitch,   o, -1, 1.0f,  5.0f);
            add(p + "roll",     FieldType::Roll,    o, -1, 1.0f,  5.0f);
        }

        add("onHit.damage",     FieldType::Damage,          -1, -1, 1.0f,  5.0f);
        add("onHit.impulse",    FieldType::ImpulseStrength, -1, -1, 10.0f, 50.0f);
        add("onHit.dir.x",      FieldType::ImpulseDirX,     -1, -1, 0.05f, 0.2f);
        add("onHit.dir.y",      FieldType::ImpulseDirY,     -1, -1, 0.05f, 0.2f);
        add("onHit.dir.z",      FieldType::ImpulseDirZ,     -1, -1, 0.05f, 0.2f);

        // Spawn / Destroy event times for this def's slot.
        for (int i = 0; i < (int)draft_.timeline.size(); ++i) {
            const TimelineEvent& ev = draft_.timeline[i];
            if (ev.type == SkillEventType::SpawnHitbox &&
                ev.payload.spawnHitbox.defIdx == (u8t)defIdx) {
                add("spawn (ms)", FieldType::SpawnTimeMs, -1, i, 5.0f, 20.0f);
            } else if (ev.type == SkillEventType::DestroyHitbox &&
                       ev.payload.destroyHitbox.slot == def.slot) {
                add("destroy (ms)", FieldType::DestroyTimeMs, -1, i, 5.0f, 20.0f);
            }
        }
    }

    add("skill duration (ms)", FieldType::TotalDurationMs, -1, -1, 10.0f, 50.0f);
}

// ---------------------------------------------------------------------------
// diff dump
// ---------------------------------------------------------------------------

void SkillDraft::dumpDiff() const {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os.precision(3);

    os << "\n========== Skill '" << draft_.name << "' edit guide ==========\n";

    bool anyChange = false;
    const int defCount = (int)std::min(draft_.hitboxDefs.size(), original_.hitboxDefs.size());

    for (int d = 0; d < defCount; ++d) {
        const SkillHitboxDef& dd = draft_.hitboxDefs[d];
        const SkillHitboxDef& od = original_.hitboxDefs[d];

        const int obbCount = (int)std::min(dd.localOBBs.size(), od.localOBBs.size());
        for (int o = 0; o < obbCount; ++o) {
            const OBB& db = dd.localOBBs[o];
            const OBB& ob = od.localOBBs[o];
            const mu::Vec3 de = (o < (int)dd.localOBBEulerDeg.size()) ? dd.localOBBEulerDeg[o] : mu::Vec3{};
            const mu::Vec3 oe = (o < (int)od.localOBBEulerDeg.size()) ? od.localOBBEulerDeg[o] : mu::Vec3{};

            const bool changed =
                db.center.x() != ob.center.x() || db.center.y() != ob.center.y() || db.center.z() != ob.center.z() ||
                db.halfExtents.x() != ob.halfExtents.x() || db.halfExtents.y() != ob.halfExtents.y() || db.halfExtents.z() != ob.halfExtents.z() ||
                de.x() != oe.x() || de.y() != oe.y() || de.z() != oe.z();
            if (!changed) continue;
            anyChange = true;

            os << "[def " << d << " obb " << o << "]\n";
            os << "  OBB(" << db.center.x() << ", " << db.center.y() << ", " << db.center.z() << ", "
               << db.halfExtents.x() << ", " << db.halfExtents.y() << ", " << db.halfExtents.z() << ", "
               << de.x() << ", " << de.y() << ", " << de.z() << ")\n";
            os << "  -- was OBB(" << ob.center.x() << ", " << ob.center.y() << ", " << ob.center.z() << ", "
               << ob.halfExtents.x() << ", " << ob.halfExtents.y() << ", " << ob.halfExtents.z() << ", "
               << oe.x() << ", " << oe.y() << ", " << oe.z() << ")\n";
            os << "  center  d(" << db.center.x() - ob.center.x() << ", "
               << db.center.y() - ob.center.y() << ", " << db.center.z() - ob.center.z() << ")"
               << "  half d(" << db.halfExtents.x() - ob.halfExtents.x() << ", "
               << db.halfExtents.y() - ob.halfExtents.y() << ", " << db.halfExtents.z() - ob.halfExtents.z() << ")"
               << "  ypr d(" << de.x() - oe.x() << ", " << de.y() - oe.y() << ", " << de.z() - oe.z() << ")\n";
        }

        // onHit
        const OnHitDef& doh = dd.onHit;
        const OnHitDef& ooh = od.onHit;
        if (doh.damage != ooh.damage || doh.impulseStrength != ooh.impulseStrength ||
            doh.impulseDirLocal.x() != ooh.impulseDirLocal.x() ||
            doh.impulseDirLocal.y() != ooh.impulseDirLocal.y() ||
            doh.impulseDirLocal.z() != ooh.impulseDirLocal.z()) {
            anyChange = true;
            os << "[def " << d << " onHit]\n";
            os << "  damage   " << ooh.damage << " -> " << doh.damage
               << " (d " << (doh.damage - ooh.damage) << ")\n";
            os << "  impulse  " << ooh.impulseStrength << " -> " << doh.impulseStrength
               << " (d " << (doh.impulseStrength - ooh.impulseStrength) << ")\n";
            os << "  dir      (" << ooh.impulseDirLocal.x() << ", " << ooh.impulseDirLocal.y() << ", " << ooh.impulseDirLocal.z()
               << ") -> (" << doh.impulseDirLocal.x() << ", " << doh.impulseDirLocal.y() << ", " << doh.impulseDirLocal.z() << ")\n";
        }
    }

    // timeline times
    for (int i = 0; i < (int)draft_.timeline.size(); ++i) {
        const int oi = (i < (int)draftEventOrig_.size()) ? draftEventOrig_[i] : -1;
        if (oi < 0 || oi >= (int)original_.timeline.size()) continue;
        const float dms = draft_.timeline[i].time.count();
        const float oms = original_.timeline[oi].time.count();
        if (dms == oms) continue;
        anyChange = true;

        const TimelineEvent& ev = draft_.timeline[i];
        const char* kind = "event";
        if (ev.type == SkillEventType::SpawnHitbox)        kind = "SpawnHitbox";
        else if (ev.type == SkillEventType::DestroyHitbox) kind = "DestroyHitbox";
        else if (ev.type == SkillEventType::PlayVFX)       kind = "PlayVFX";
        else if (ev.type == SkillEventType::PlayAnimation) kind = "PlayAnimation";
        os << "[timeline] " << kind << " time " << oms << " -> " << dms
           << " (d " << (dms - oms) << ")  -- update addEvent(" << (int)dms << ", ...)\n";
    }

    if (draft_.totalDuration.count() != original_.totalDuration.count()) {
        anyChange = true;
        os << "[skill] totalDurationMs " << original_.totalDuration.count()
           << " -> " << draft_.totalDuration.count()
           << " (d " << (draft_.totalDuration.count() - original_.totalDuration.count()) << ")\n";
    }

    if (!anyChange)
        os << "  (no changes vs original)\n";
    os << "================================================\n";

    gSharedLog << os.str();
    dumpLog();
}

}   // namespace Editor
