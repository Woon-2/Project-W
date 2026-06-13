#include "rspch.hpp"
#include "skillCompiler.hpp"

static AttachType parseAttachType(std::string_view s) {
    if (s == "VFXParticle") return AttachType::VFXParticle;
    if (s == "Ground")      return AttachType::Ground;
    return AttachType::Bone;
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

    sol::optional<sol::table> vfxList = tbl["vfxNames"];
    if (vfxList) {
        vfxList->for_each([&](sol::object, sol::object val) {
            asset.vfxNames.push_back(val.as<std::string>());
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
            case SkillEventType::CameraShake:
            case SkillEventType::ModifyStat:
            case SkillEventType::PlayVFX:
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
        tbl.set_function("addEvent", [](sol::table self, int timeMs, std::string type,
                                        sol::table params) {
            sol::state_view sv{ self.lua_state() };
            sol::table ev = sv.create_table();
            ev["timeMs"] = timeMs;
            ev["type"]   = type;
            params.for_each([&](sol::object key, sol::object val) { ev[key] = val; });
            self["events"].get<sol::table>().add(ev);
        });
        tbl.set_function("addVFX", [](sol::table self, int idx, std::string path) {
            self["vfxNames"].get<sol::table>()[idx + 1] = path;
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

    for (const auto& entry : std::filesystem::directory_iterator(skillDir)) {
        if (entry.path().extension() != ".lua") continue;
        try {
            sol::protected_function_result result = lua_.safe_script_file(entry.path().string());
            if (!result.valid()) {
                sol::error err = result;
                std::cerr << "[ServerSkillCompiler] Error in " << entry.path() << ": " << err.what() << "\n";
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
