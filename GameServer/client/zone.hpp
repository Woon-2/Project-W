#ifndef __zone_HPP
#define __zone_HPP

#include "object.hpp"            // Faction, factionBit
#include "../common/zoneDef.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// Client-local trigger volume. Server-authoritative gameplay zones are handled
// on the server; the client runs only cosmetic zones (BGM, camera, post-fx)
// against its predicted local player. A Zone is the union of its volumes.
enum class ZoneEvent : u8t { Enter, Stay, Leave };

class Zone {
public:
    explicit Zone(const ZoneDef& def) : def_(def) {}

    bool MU_CALLCONV contains(mu::Vec3 p) const;
    bool factionAllowed(Faction f) const {
        return (def_.factionMask & factionBit(f)) != 0u;
    }

    int                id()  const { return def_.id; }
    const std::string& tag() const { return def_.tag; }

    bool inside() const { return inside_; }
    void setInside(bool v) { inside_ = v; }

private:
    ZoneDef def_;
    bool    inside_ = false;   // was the local player inside last tick?
};

// Cosmetic handler acting on global client state; binds via lambda capture.
using ZoneHandler = std::function<void(Zone&)>;

class ZoneSystem {
public:
    void build(const std::vector<ZoneDef>& defs);
    void on(const std::string& tag, ZoneEvent ev, ZoneHandler handler);

    // Tests the local player position against zones that have a bound handler.
    // Zones with no client handler (server-only gameplay zones) are skipped.
    void MU_CALLCONV update(mu::Vec3 playerPos);

    bool        empty() const { return zones_.empty(); }
    std::size_t size()  const { return zones_.size(); }

private:
    struct TagHandlers {
        ZoneHandler enter;
        ZoneHandler stay;
        ZoneHandler leave;
    };

    std::vector<Zone> zones_;
    std::unordered_map<std::string, TagHandlers> handlers_;
};

#endif // __zone_HPP
