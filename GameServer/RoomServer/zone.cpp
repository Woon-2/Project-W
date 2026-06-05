#include "rspch.hpp"
#include "zone.hpp"
#include "Room.hpp"
#include <cmath>
#include <unordered_map>

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
    zones_.reserve(defs.size());   // stable addresses for byId() after build
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

Zone* ZoneSystem::byId(int id) {
    for (auto& z : zones_)
        if (z.id() == id) return &z;
    return nullptr;
}

void ZoneSystem::update(Room& room, Seconds /*dt*/) {
    if (zones_.empty()) return;

    for (auto& zone : zones_) {
        // Disarmed zones stop detecting. Clearing occupancy means that on re-arm
        // any object still inside re-triggers Enter (correct for boss retries).
        if (!zone.armed()) {
            zone.occupants().clear();
            continue;
        }

        const auto it = handlers_.find(zone.tag());
        if (it == handlers_.end()) continue;   // no bound behavior on this side
        const TagHandlers& h = it->second;

        std::unordered_set<uint32>     newOcc;
        std::unordered_map<uint32, Object*> present;

        const auto consider = [&](Object* obj) {
            if (!obj) return;
            if (!zone.factionAllowed(obj->faction())) return;
            if (!zone.contains(obj->pos())) return;
            const uint32 oid = obj->getId();
            newOcc.insert(oid);
            present[oid] = obj;
        };

        if (zone.factionAllowed(Faction::Players)) {
            for (auto* s : room.getLivingPlayers())
                consider(s->player());
        }
        if (zone.factionAllowed(Faction::Monsters)) {
            for (auto& g : room.goblins())
                if (g.hp() > 0) consider(&g);
        }

        auto& occ = zone.occupants();

        // Enter (newly inside) + Stay (still inside)
        for (uint32 id : newOcc) {
            Object* obj = present[id];
            if (occ.find(id) == occ.end()) {
                if (h.enter) h.enter(room, zone, id, obj);
            } else {
                if (h.stay) h.stay(room, zone, id, obj);
            }
        }
        // Leave (was inside, no longer). obj may be null (death/disconnect).
        for (uint32 id : occ) {
            if (newOcc.find(id) == newOcc.end()) {
                if (h.leave) h.leave(room, zone, id, room.resolveObject(id));
            }
        }

        occ = std::move(newOcc);
    }
}
