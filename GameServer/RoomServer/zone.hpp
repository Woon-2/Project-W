#ifndef room_server_zone_hpp
#define room_server_zone_hpp

#include "object.hpp"            // Faction, factionBit, Object
#include "../common/zoneDef.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Room;

// Lifecycle phase derived from per-tick occupancy diffing.
enum class ZoneEvent : uint8 { Enter, Stay, Leave };

// Runtime instance of a trigger volume. A Zone is the union of its primitive
// volumes; an object is "inside" when its position lies in any volume AND its
// faction is allowed by the zone's factionMask. Zones are pure query volumes:
// they are NOT registered with PhysicsWorld and never generate contacts.
class Zone {
public:
    explicit Zone(const ZoneDef& def) : def_(def) {}

    // True if world point p lies within any of this zone's volumes.
    bool MU_CALLCONV contains(mu::Vec3 p) const;

    bool factionAllowed(Faction f) const {
        return (def_.factionMask & factionBit(f)) != 0u;
    }

    int                id()  const { return def_.id; }
    const std::string& tag() const { return def_.tag; }

    bool armed() const { return armed_; }
    void setArmed(bool v) { armed_ = v; }

    std::unordered_set<uint32>&       occupants()       { return occupants_; }
    const std::unordered_set<uint32>& occupants() const { return occupants_; }

private:
    ZoneDef def_;
    bool    armed_ = true;
    std::unordered_set<uint32> occupants_;   // object ids currently inside
};

// Handler invoked on a zone lifecycle event. obj may be null for Leave events
// when the object has been removed (death/disconnect) between ticks.
using ZoneHandler = std::function<void(Room&, Zone&, uint32 /*objId*/, Object* /*obj*/)>;

// Owns the room's zones and dispatches Enter/Stay/Leave to per-tag handlers.
// Handlers are bound in C++ (Room::bindZoneHandlers) keyed by the zone tag
// authored in Unity, so multiple zones can share one behavior.
class ZoneSystem {
public:
    void build(const std::vector<ZoneDef>& defs);
    void on(const std::string& tag, ZoneEvent ev, ZoneHandler handler);

    // Gathers faction-filtered candidates from the room, recomputes occupancy,
    // and fires the bound handlers. Call once per tick after physicsWorld_.step().
    void update(Room& room, Seconds dt);

    Zone*       byId(int id);
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

#endif // room_server_zone_hpp
