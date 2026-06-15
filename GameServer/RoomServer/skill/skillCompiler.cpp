#include "rspch.hpp"
#include "skillCompiler.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <unordered_map>

static AttachType parseAttachType(std::string_view s) {
    if (s == "VFXParticle") return AttachType::VFXParticle;
    if (s == "Ground")      return AttachType::Ground;
    return AttachType::Bone;
}

// Ordinals match ps::ParticleCollisionModule::Mode (None,GroundStop,GroundKill,GroundBounce).
static u8t parseParticleCollisionMode(std::string_view s) {
    if (s == "GroundStop")   return 1;
    if (s == "GroundKill")   return 2;
    if (s == "GroundBounce") return 3;
    return 0;
}

// Ordinals match ps::ShapeModule::GroundConform (None,SnapY,SnapAndAlign).
static u8t parseParticleConformMode(std::string_view s) {
    if (s == "SnapY")        return 1;
    if (s == "SnapAndAlign") return 2;
    return 0;
}

static pg::ShapeType parseShapeTypeName(std::string_view s) {
    if (s == "Edge")       return pg::ShapeType::Edge;
    if (s == "Cone")       return pg::ShapeType::Cone;
    if (s == "Sphere")     return pg::ShapeType::Sphere;
    if (s == "Hemisphere") return pg::ShapeType::Hemisphere;
    if (s == "Box")        return pg::ShapeType::Box;
    if (s == "Circle")     return pg::ShapeType::Circle;
    return pg::ShapeType::Point;
}

// Parses the optional gameplay overrides of one addVFX `systems` entry.
// SYNC: mirrors client/skill/skillCompiler.cpp (and the override list in
// pg::VfxSystemOverrides / pg::applyOverrides).
static void parseVfxSystemOverrides(const sol::table& t, pg::VfxSystemOverrides& o) {
    auto boolOf = [&](const char* key, bool& has, bool& out) {
        sol::optional<bool> v = t[key];
        if (v) { has = true; out = *v; }
    };
    auto floatOf = [&](const char* key, bool& has, float& out) {
        sol::optional<float> v = t[key];
        if (v) { has = true; out = *v; }
    };
    auto intOf = [&](const char* key, bool& has, int& out) {
        sol::optional<int> v = t[key];
        if (v) { has = true; out = *v; }
    };
    auto vec3Of = [&](const char* key, bool& has, mu::Vec3& out) {
        sol::optional<sol::table> v = t[key];
        if (!v) return;
        has = true;
        out = { v->get_or(1, 0.f), v->get_or(2, 0.f), v->get_or(3, 0.f) };
    };
    // number = constant; {min, max} table = range
    auto rangeOf = [&](const char* key, bool& has, float& lo, float& hi) {
        sol::optional<sol::object> v = t[key];
        if (!v) return;
        if (v->is<sol::table>()) {
            sol::table r = v->as<sol::table>();
            lo = r.get_or(1, 0.f);
            hi = r.get_or(2, lo);
        } else if (v->is<float>()) {
            lo = hi = v->as<float>();
        } else {
            return;
        }
        has = true;
    };

    boolOf("looping",        o.hasLooping,      o.looping);
    floatOf("duration",      o.hasDuration,     o.duration);
    rangeOf("speed",         o.hasSpeed,        o.speedMin,     o.speedMax);
    rangeOf("lifetime",      o.hasLifetime,     o.lifetimeMin,  o.lifetimeMax);
    rangeOf("startSize",     o.hasStartSize,    o.startSizeMin, o.startSizeMax);
    intOf("maxParticles",    o.hasMaxParticles, o.maxParticles);
    vec3Of("shapePosition",  o.hasShapePosition, o.shapePosition);
    vec3Of("shapeEuler",     o.hasShapeEulerDeg, o.shapeEulerDeg);
    vec3Of("meshEuler",      o.hasMeshEulerDeg,  o.meshEulerDeg);
    vec3Of("direction",      o.hasDirection,     o.direction);
    vec3Of("boxSize",        o.hasBoxSize,       o.boxSize);
    vec3Of("volLinear",      o.hasVolLinear,     o.volLinear);
    floatOf("coneRadius",    o.hasConeRadius,      o.coneRadius);
    floatOf("coneAngle",     o.hasConeAngleDeg,    o.coneAngleDeg);
    floatOf("radiusThickness", o.hasRadiusThickness, o.radiusThickness);
    floatOf("emitRate",      o.hasEmitRate,        o.emitRate);

    sol::optional<std::string> shapeType = t["shapeType"];
    if (shapeType) {
        o.hasShapeType = true;
        o.shapeType    = parseShapeTypeName(*shapeType);
    }

    sol::optional<sol::table> bursts = t["bursts"];
    if (bursts) {
        o.hasBursts = true;
        for (std::size_t bi = 1; bi <= bursts->size(); ++bi) {
            sol::optional<sol::table> b = (*bursts)[bi];
            if (!b) continue;
            pg::Burst burst{};
            burst.time           = b->get_or("time", 0.f);
            burst.cycleCount     = b->get_or("cycleCount", 1);
            burst.repeatInterval = b->get_or("interval", 0.01f);
            burst.probability    = b->get_or("probability", 1.f);
            const int count      = b->get_or("count", 1);
            burst.countMin = count;
            burst.countMax = count;
            o.bursts.push_back(burst);
        }
    }
}

static SkillEventType parseEventType(std::string_view s) {
    if (s == "SpawnHitbox")       return SkillEventType::SpawnHitbox;
    if (s == "DestroyHitbox")     return SkillEventType::DestroyHitbox;
    if (s == "PlayAnimation")     return SkillEventType::PlayAnimation;
    if (s == "PlayVFX")           return SkillEventType::PlayVFX;
    if (s == "ModifyStat")        return SkillEventType::ModifyStat;
    if (s == "ApplyImpulse")      return SkillEventType::ApplyImpulse;
    if (s == "CameraShake")       return SkillEventType::CameraShake;
    if (s == "SendGameplayEvent") return SkillEventType::SendGameplayEvent;
    if (s == "SpawnProjectile")   return SkillEventType::SpawnProjectile;
    if (s == "SetGroundAnchor")   return SkillEventType::SetGroundAnchor;
    return SkillEventType::SIZE;
}

OnHitDef ServerSkillCompiler::tableToOnHitDef(const sol::table& tbl) {
    OnHitDef oh{};
    oh.damage          = tbl.get_or("damage",          0);
    oh.hitVfxId        = static_cast<u8t>(tbl.get_or("vfxId",    (int)0xFF));
    oh.impulseStrength = tbl.get_or("impulseStrength", 0.f);

    sol::optional<sol::table> dir = tbl["impulseDir"];
    if (dir) {
        oh.impulseDirLocal = {
            (*dir).get_or(1, 0.f),
            (*dir).get_or(2, 0.f),
            (*dir).get_or(3, 1.f)
        };
    }
    return oh;
}

SkillHitboxDef ServerSkillCompiler::tableToHitboxDef(const sol::table& tbl) {
    SkillHitboxDef def{};
    def.slot                = static_cast<u8t>(tbl.get_or("slot",              0));
    def.hitGroup            = static_cast<u8t>(tbl.get_or("hitGroup",          0));
    def.hitGroupCooldownMs  =                  tbl.get_or("hitGroupCooldownMs", 0.f);
    def.applyAttachRotation =                  tbl.get_or("applyAttachRotation", true);

    sol::optional<sol::table> obbList = tbl["localOBBs"];
    if (obbList) {
        obbList->for_each([&](sol::object, sol::object val) {
            if (!val.is<sol::table>()) return;
            sol::table obbTbl = val.as<sol::table>();
            OBB obb{};

            sol::optional<sol::table> center = obbTbl["center"];
            if (center) {
                obb.center = {
                    (*center).get_or(1, 0.f),
                    (*center).get_or(2, 0.f),
                    (*center).get_or(3, 0.f)
                };
            }
            sol::optional<sol::table> he = obbTbl["halfExtents"];
            if (he) {
                obb.halfExtents = {
                    (*he).get_or(1, 0.5f),
                    (*he).get_or(2, 0.5f),
                    (*he).get_or(3, 0.5f)
                };
            }
            sol::optional<sol::table> orient = obbTbl["orient"];
            if (orient) {
                float yaw   = (*orient).get_or(1, 0.f);
                float pitch = (*orient).get_or(2, 0.f);
                float roll  = (*orient).get_or(3, 0.f);
                obb.orient = mu::NQuat(mu::Degree{roll}, mu::Degree{pitch}, mu::Degree{yaw});
            }
            def.localOBBs.push_back(obb);
        });
    }

    sol::optional<sol::table> attach = tbl["attach"];
    if (attach) {
        std::string typeStr    = (*attach).get_or<std::string>("type", "Bone");
        def.attach.type        = parseAttachType(typeStr);
        def.attach.targetName  = (*attach).get_or<std::string>("name", "");
        def.attach.vfxId       = static_cast<u8t>((*attach).get_or("vfxId", 0));
        def.attach.particleSystemIdx = (*attach).get_or("systemIdx", 0);
        def.attach.groundAlign = (*attach).get_or("align", false);
        def.attach.groundAnchorRef = (*attach).get_or("anchor", -1);
    }

    sol::optional<sol::table> onHit = tbl["onHit"];
    if (onHit)
        def.onHit = tableToOnHitDef(*onHit);

    def.useParticleSize = tbl.get_or("useParticleSize", false);
    return def;
}

SkillAsset ServerSkillCompiler::tableToAsset(const sol::table& tbl) {
    SkillAsset asset{};
    asset.name          = tbl.get_or<std::string>("name", "UnnamedSkill");
    asset.totalDuration = Milliseconds{ static_cast<float>(tbl.get_or("totalDurationMs", 0)) };
    asset.interruptible = tbl.get_or("interruptible", true);

    // Stack-charge / loadout metadata (optional; absent fields keep defaults).
    asset.weaponType = [&]() -> u8t {
        const std::string w = tbl.get_or<std::string>("weapon", "");
        if (w == "sword") return 0u;  // Katana
        if (w == "spear") return 1u;  // SpearHook
        if (w == "wand")  return 2u;  // CrystalWand
        if (w == "bow")   return 3u;  // HeavyArrow
        return 0xFFu;
    }();
    asset.loadoutSlot = tbl.get_or("dialSlot", -1);
    asset.isBasic     = tbl.get_or("isBasic", false);
    asset.chargeCost  = static_cast<float>(tbl.get_or("chargeCost", 0.0));
    asset.cooldown    = Milliseconds{ static_cast<float>(tbl.get_or("cooldownMs", 0)) };

    sol::optional<sol::table> vfxList = tbl["vfxNames"];
    if (vfxList) {
        vfxList->for_each([&](sol::object, sol::object val) {
            asset.vfxNames.push_back(val.as<std::string>());
        });
    }

    // VFX composition registry (addVFX 3rd arg). Keyed by vfxId + 1.
    sol::optional<sol::table> vfxDefList = tbl["vfxDefs"];
    if (vfxDefList) {
        vfxDefList->for_each([&](sol::object key, sol::object val) {
            if (!key.is<int>() || !val.is<sol::table>()) return;
            const int vfxId = key.as<int>() - 1;
            if (vfxId < 0 || vfxId > 255) return;
            sol::table defTbl = val.as<sol::table>();

            if (static_cast<int>(asset.vfxDefs.size()) <= vfxId)
                asset.vfxDefs.resize(static_cast<std::size_t>(vfxId) + 1);
            SkillAsset::VfxDef& def = asset.vfxDefs[static_cast<std::size_t>(vfxId)];
            def.path = defTbl.get_or<std::string>("path", "");

            sol::optional<sol::table> systems = defTbl["systems"];
            if (!systems) return;
            // Ordered iteration: index == ParticleEffect::addSystem order.
            for (std::size_t si = 1; si <= systems->size(); ++si) {
                sol::optional<sol::table> sysTbl = (*systems)[si];
                if (!sysTbl) continue;
                SkillAsset::VfxSystemDef sys{};
                sys.name = sysTbl->get_or<std::string>("name", "");
                const std::string mode = sysTbl->get_or<std::string>("mode", "Emit");
                sys.playMode = (mode == "Continuous") ? 1 : 0;
                sol::optional<int> parent = (*sysTbl)["parent"];
                if (parent) {
                    sys.chainParent  = *parent;
                    sys.chainOnBirth = sysTbl->get_or<std::string>("parentEvent", "Death")
                                       == "Birth";
                }
                parseVfxSystemOverrides(*sysTbl, sys.overrides);
                def.systems.push_back(std::move(sys));
            }
        });
    }

    sol::optional<sol::table> events = tbl["events"];
    if (events) {
        events->for_each([&](sol::object, sol::object val) {
            if (!val.is<sol::table>()) return;
            sol::table evTbl = val.as<sol::table>();

            TimelineEvent ev{};
            ev.time = Milliseconds{ static_cast<float>(evTbl.get_or("timeMs", 0)) };
            std::string typeStr = evTbl.get_or<std::string>("type", "");
            ev.type = parseEventType(typeStr);
            if (ev.type == SkillEventType::SIZE) return;

            switch (ev.type) {
            case SkillEventType::SpawnHitbox: {
                SkillHitboxDef def = tableToHitboxDef(evTbl);
                ev.payload.spawnHitbox.defIdx = static_cast<u8t>(asset.hitboxDefs.size());
                asset.hitboxDefs.push_back(std::move(def));
                break;
            }
            case SkillEventType::DestroyHitbox:
                ev.payload.destroyHitbox.slot = static_cast<u8t>(evTbl.get_or("slot", 0));
                break;
            case SkillEventType::PlayAnimation: {
                std::string clipName = evTbl.get_or<std::string>("clipName", "");
                auto& p = ev.payload.playAnimation;
                std::strncpy(p.clipName, clipName.c_str(), sizeof(p.clipName) - 1);
                p.clipName[sizeof(p.clipName) - 1] = '\0';
                p.blendTime = evTbl.get_or("blendTime", 0.1f);
                break;
            }
            case SkillEventType::ApplyImpulse: {
                auto& p   = ev.payload.applyImpulse;
                p.strength = evTbl.get_or("strength", 0.f);
                sol::optional<sol::table> dir = evTbl["dir"];
                if (dir) {
                    p.dirLocal = {
                        (*dir).get_or(1, 0.f),
                        (*dir).get_or(2, 0.f),
                        (*dir).get_or(3, 1.f)
                    };
                }
                break;
            }
            case SkillEventType::SetGroundAnchor: {
                // Authoritative on the server (drives Ground hitbox placement), so parse it.
                auto& p     = ev.payload.setGroundAnchor;
                p.anchorId  = static_cast<u8t>(evTbl.get_or("id", 0));
                p.flags     = 0;
                if (evTbl.get_or("align", false))
                    p.flags |= kGroundAnchorFlagAlign;
                sol::optional<sol::table> off = evTbl["offset"];
                if (off) {
                    p.localOffset = mu::Vec3( (*off).get_or(1, 0.f),
                            (*off).get_or(2, 0.f),
                            (*off).get_or(3, 0.f)
                    );
                }
                break;
            }
            case SkillEventType::PlayVFX: {
                // Parsed on the server since 2026-06-11: the deterministic
                // VFXParticle hitbox sampler needs the effect anchor
                // (vfxId, offset, orient, ground flags, conform override).
                // Mirrors the client compiler exactly.
                auto& p = ev.payload.playVFX;
                p.vfxId = static_cast<u8t>(evTbl.get_or("vfxId", 0));

                sol::optional<sol::table> attach = evTbl["attach"];
                if (attach) {
                    std::string typeStr2 = (*attach).get_or<std::string>("type", "Bone");
                    p.attachType = static_cast<u8t>(parseAttachType(typeStr2));
                    std::string tgtName = (*attach).get_or<std::string>("name", "");
                    std::strncpy(p.attachTargetName, tgtName.c_str(), sizeof(p.attachTargetName) - 1);
                    p.attachTargetName[sizeof(p.attachTargetName) - 1] = '\0';
                    p.attachVfxId = static_cast<u8t>((*attach).get_or("vfxId", 0));
                }

                sol::optional<sol::table> offset = evTbl["offset"];
                if (offset) {
                    p.localOffset = mu::Vec3( (*offset).get_or(1, 0.f),
                            (*offset).get_or(2, 0.f),
                            (*offset).get_or(3, 0.f)
                    );
                }

                sol::optional<sol::table> vfxOrient = evTbl["orient"];
                if (vfxOrient) {
                    p.localEulerDeg = mu::Vec3( (*vfxOrient).get_or(1, 0.f),
                            (*vfxOrient).get_or(2, 0.f),
                            (*vfxOrient).get_or(3, 0.f)
                    );
                }

                sol::optional<sol::table> advance = evTbl["advance"];
                if (advance) {
                    p.advanceForwardLocal = mu::Vec3( (*advance).get_or(1, 0.f),
                            (*advance).get_or(2, 0.f),
                            (*advance).get_or(3, 0.f)
                    );
                }

                if (evTbl.get_or("groundLock", false))
                    p.flags |= kPlayVFXFlagYawOnly;
                if (evTbl.get_or("groundSnap", false))
                    p.flags |= kPlayVFXFlagGroundSnap;
                if (evTbl.get_or("groundAlign", false))
                    p.flags |= kPlayVFXFlagGroundAlign;

                {
                    const u8t col = parseParticleCollisionMode(
                        evTbl.get_or<std::string>("particleCollision", ""));
                    p.flags |= (col << kPlayVFXParticleCollisionShift) & kPlayVFXParticleCollisionMask;
                    const u8t conf = parseParticleConformMode(
                        evTbl.get_or<std::string>("particleConform", ""));
                    p.flags |= (conf << kPlayVFXParticleConformShift) & kPlayVFXParticleConformMask;
                }
                break;
            }
            case SkillEventType::CameraShake:
            case SkillEventType::ModifyStat:
            default:
                break;
            }

            asset.timeline.push_back(ev);
        });
    }

    std::sort(asset.timeline.begin(), asset.timeline.end(),
              [](const TimelineEvent& a, const TimelineEvent& b) {
                  return a.time.count() < b.time.count();
              });

    return asset;
}

void ServerSkillCompiler::registerAPI() {
    lua_.open_libraries(sol::lib::base, sol::lib::table, sol::lib::math, sol::lib::string);

    lua_.set_function("Skill", [](sol::this_state s) {
        sol::state_view lua{ s };
        sol::table tbl = lua.create_table();
        tbl["events"]   = lua.create_table();
        tbl["vfxNames"] = lua.create_table();
        tbl["vfxDefs"]  = lua.create_table();
        tbl.set_function("addEvent", [](sol::table self, int timeMs, std::string type,
                                        sol::table params) {
            sol::state_view sv{ self.lua_state() };
            sol::table ev = sv.create_table();
            ev["timeMs"] = timeMs;
            ev["type"]   = type;
            params.for_each([&](sol::object key, sol::object val) { ev[key] = val; });
            self["events"].get<sol::table>().add(ev);
        });
        tbl.set_function("addVFX", [](sol::table self, int idx, std::string path,
                                      sol::optional<sol::table> opts) {
            self["vfxNames"].get<sol::table>()[idx + 1] = path;
            sol::state_view sv{ self.lua_state() };
            sol::table def = sv.create_table();
            def["path"] = path;
            if (opts) {
                sol::optional<sol::table> systems = (*opts)["systems"];
                if (systems) def["systems"] = *systems;
            }
            self["vfxDefs"].get<sol::table>()[idx + 1] = def;
        });
        return tbl;
    });
}

std::vector<SkillAsset> ServerSkillCompiler::compileAll(const std::filesystem::path& skillDir) {
    std::vector<SkillAsset> assets;
    if (!std::filesystem::exists(skillDir)) {
        std::cerr << "[ServerSkillCompiler] skillDir not found: " << skillDir << "\n";
        return assets;
    }

    registerAPI();

    auto apiPath = skillDir / "lua" / "skill_api.lua";
    if (std::filesystem::exists(apiPath)) {
        auto r = lua_.safe_script_file(apiPath.string());
        if (!r.valid()) {
            sol::error err = r;
            std::cerr << "[ServerSkillCompiler] Failed to load skill_api.lua: " << err.what() << "\n";
        }
    }

    // Collect then SORT by filename: sequential asset IDs ride on this order
    // and must match the client compiler (C_SkillStart sends the numeric id).
    // directory_iterator order is filesystem-dependent, so never rely on it.
    std::vector<std::filesystem::path> luaFiles;
    for (const auto& entry : std::filesystem::directory_iterator(skillDir)) {
        if (entry.path().extension() != ".lua") continue;
        luaFiles.push_back(entry.path());
    }
    std::sort(luaFiles.begin(), luaFiles.end(),
              [](const std::filesystem::path& a, const std::filesystem::path& b) {
                  return a.filename().string() < b.filename().string();
              });

    for (const auto& luaPath : luaFiles) {
        try {
            sol::protected_function_result result = lua_.safe_script_file(luaPath.string());
            if (!result.valid()) {
                sol::error err = result;
                std::cerr << "[ServerSkillCompiler] Error in " << luaPath << ": " << err.what() << "\n";
                continue;
            }
            if (!result.get<sol::object>().is<sol::table>()) continue;
            SkillAsset asset = tableToAsset(result.get<sol::table>());
            std::cout << "[ServerSkillCompiler] Loaded: " << asset.name << "\n";
            assets.push_back(std::move(asset));
        } catch (const std::exception& ex) {
            std::cerr << "[ServerSkillCompiler] Exception: " << ex.what() << "\n";
        }
    }

    for (u32t i = 0; i < static_cast<u32t>(assets.size()); ++i)
        assets[i].id = i + 1;

    return assets;
}

// SYNC: mirrors client/skill/skillCompiler.cpp buildVfxGameplayConfigs.
void buildVfxGameplayConfigs(std::vector<SkillAsset>& assets,
                             const std::filesystem::path& resourceRoot) {
    // path -> parsed effect DOM (nullptr = load failed; negative-cached)
    std::unordered_map<std::string, std::unique_ptr<json::Value>> domCache;

    for (SkillAsset& asset : assets) {
        for (SkillAsset::VfxDef& vdef : asset.vfxDefs) {
            for (SkillAsset::VfxSystemDef& sys : vdef.systems) {
                // Entries without a JSON source nor overrides are placeholders
                // (index alignment only) and get no gameplay config.
                if (sys.name.empty() && !sys.overrides.any())
                    continue;

                auto cfg = std::make_shared<pg::GameplayConfig>();
                if (!sys.name.empty()) {
                    auto it = domCache.find(vdef.path);
                    if (it == domCache.end()) {
                        auto dom = std::make_unique<json::Value>();
                        std::string err;
                        if (!pg::loadEffectJson(resourceRoot / vdef.path, *dom, &err)) {
                            std::cerr << "[ServerSkillCompiler] " << err << "\n";
                            dom.reset();
                        }
                        it = domCache.emplace(vdef.path, std::move(dom)).first;
                    }
                    if (!it->second)
                        continue;
                    std::string err;
                    if (!pg::importGameplayConfig(*it->second, sys.name, *cfg, &err)) {
                        std::cerr << "[ServerSkillCompiler] " << asset.name << ": " << err << "\n";
                        continue;
                    }
                }
                pg::applyOverrides(*cfg, sys.overrides);
                if (const std::string unsupported = cfg->unsupportedSummary();
                    !unsupported.empty()) {
                    std::cerr << "[ServerSkillCompiler] " << asset.name << " vfx system '"
                              << sys.name << "' has unsupported gameplay features: "
                              << unsupported << "\n";
                }
                sys.gameplayCfg = std::move(cfg);
            }
        }
    }
}
