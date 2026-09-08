#include "pch.hpp"
#include "zone.hpp"
#include <cmath>

bool MU_CALLCONV Zone::contains(mu::Vec3 p) const {
    for (const auto& v : def_.volumes) {
        if (v.shape == ZoneShape::Sphere) {
            const mu::Vec3 d = p - v.center;
            if (d.len2() <= v.radius * v.radius) return true;
        } else {  // Box (OBB): bring point into the box's local frame
            const mu::Vec3 local = (~v.orient).rotate(p - v.center);
            if (std::fabs(local.x()) <= v.halfExtents.x() &&
                std::fabs(local.y()) <= v.halfExtents.y() &&
                std::fabs(local.z()) <= v.halfExtents.z())
                return true;
        }
    }
    return false;
}

void ZoneSystem::build(const std::vector<ZoneDef>& defs) {
    zones_.clear();
    zones_.reserve(defs.size());
    for (const auto& d : defs) zones_.emplace_back(d);
}

void ZoneSystem::on(const std::string& tag, ZoneEvent ev, ZoneHandler handler) {
    auto& h = handlers_[tag];
    switch (ev) {
    case ZoneEvent::Enter: h.enter = std::move(handler); break;
    case ZoneEvent::Stay:  h.stay  = std::move(handler); break;
    case ZoneEvent::Leave: h.leave = std::move(handler); break;
    }
}

void MU_CALLCONV ZoneSystem::update(mu::Vec3 playerPos) {
    if (zones_.empty()) return;

    for (auto& zone : zones_) {
        const auto it = handlers_.find(zone.tag());
        if (it == handlers_.end()) continue;   // server-only zone: no client behavior
        const TagHandlers& h = it->second;

        const bool nowInside =
            zone.factionAllowed(Faction::Players) && zone.contains(playerPos);
        const bool wasInside = zone.inside();

        if (nowInside && !wasInside) { if (h.enter) h.enter(zone); }
        else if (nowInside && wasInside) { if (h.stay) h.stay(zone); }
        else if (!nowInside && wasInside) { if (h.leave) h.leave(zone); }

        zone.setInside(nowInside);
    }
}
