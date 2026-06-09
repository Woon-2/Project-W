#include "pch.hpp"
#include "skillCompiler.hpp"

// ---------------------------------------------------------------------------
// sol2-based Lua skill compiler (requires sol/sol.hpp and lua54.dll).
// skill_api.lua is executed before each individual skill file.
// ---------------------------------------------------------------------------

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
    return SkillEventType::SIZE;  // unknown
}

OnHitDef SkillCompiler::tableToOnHitDef(const sol::table& tbl) {
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

SkillHitboxDef SkillCompiler::tableToHitboxDef(const sol::table& tbl) {
    SkillHitboxDef def{};
    def.slot                = static_cast<u8t>(tbl.get_or("slot",              0));
    def.hitGroup            = static_cast<u8t>(tbl.get_or("hitGroup",          0));
    def.hitGroupCooldownMs  =                  tbl.get_or("hitGroupCooldownMs", 0.f);
    def.applyAttachRotation =                  tbl.get_or("applyAttachRotation", true);

    // localOBBs: array of { center={x,y,z}, halfExtents={x,y,z} }
    sol::optional<sol::table> obbList = tbl["localOBBs"];
    if (obbList) {
        obbList->for_each([&](sol::object /*key*/, sol::object val) {
            if (!val.is<sol::table>()) return;
            sol::table obbTbl = val.as<sol::table>();

            OBB obb{};  // zero-init (identity orient)
            mu::Vec3 eulerDeg{ 0.f, 0.f, 0.f };  // yaw, pitch, roll (authoring metadata)

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
            // orient: optional { yaw_deg, pitch_deg, roll_deg } (positional)
            sol::optional<sol::table> orient = obbTbl["orient"];
            if (orient) {
                float yaw   = (*orient).get_or(1, 0.f);
                float pitch = (*orient).get_or(2, 0.f);
                float roll  = (*orient).get_or(3, 0.f);
                // mu::NQuat(roll, pitch, yaw) -> XMQuaternionRotationRollPitchYaw(pitch, yaw, roll)
                obb.orient = mu::NQuat(mu::Degree{roll}, mu::Degree{pitch}, mu::Degree{yaw});
                eulerDeg   = { yaw, pitch, roll };
            }
            def.localOBBs.push_back(obb);
            def.localOBBEulerDeg.push_back(eulerDeg);
        });
    }

    // attach target
    sol::optional<sol::table> attach = tbl["attach"];
    if (attach) {
        std::string typeStr    = (*attach).get_or<std::string>("type", "Bone");
        def.attach.type        = parseAttachType(typeStr);
        def.attach.targetName  = (*attach).get_or<std::string>("name", "");
        def.attach.vfxId       = static_cast<u8t>((*attach).get_or("vfxId", 0));
        def.attach.particleSystemIdx = (*attach).get_or("systemIdx", 0);
        // Ground attach: tilt the planted OBBs to the terrain normal.
        def.attach.groundAlign = (*attach).get_or("align", false);
    }

    // onHit
    sol::optional<sol::table> onHit = tbl["onHit"];
    if (onHit)
        def.onHit = tableToOnHitDef(*onHit);

    // useParticleSize: VFXParticle only -- scale OBB halfExtents by each particle's current visual size
    def.useParticleSize = tbl.get_or("useParticleSize", false);

    return def;
}

SkillAsset SkillCompiler::tableToAsset(const sol::table& tbl, const Skeleton* pSkeleton) {
    SkillAsset asset{};
    asset.name         = tbl.get_or<std::string>("name", "UnnamedSkill");
    asset.totalDuration = Milliseconds{ static_cast<float>(tbl.get_or("totalDurationMs", 0)) };
    asset.interruptible = tbl.get_or("interruptible", true);

    // VFX name registry
    sol::optional<sol::table> vfxList = tbl["vfxNames"];
    if (vfxList) {
        vfxList->for_each([&](sol::object /*key*/, sol::object val) {
            asset.vfxNames.push_back(val.as<std::string>());
        });
    }

    // Timeline events
    sol::optional<sol::table> events = tbl["events"];
    if (events) {
        events->for_each([&](sol::object /*key*/, sol::object val) {
            if (!val.is<sol::table>()) return;
            sol::table evTbl = val.as<sol::table>();

            TimelineEvent ev{};
            ev.time = Milliseconds{ static_cast<float>(evTbl.get_or("timeMs", 0)) };
            std::string typeStr = evTbl.get_or<std::string>("type", "");
            ev.type = parseEventType(typeStr);
            if (ev.type == SkillEventType::SIZE) return;  // skip unknown

            switch (ev.type) {
            case SkillEventType::SpawnHitbox: {
                // Build SkillHitboxDef and store in asset.hitboxDefs
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
                float blendTime      = evTbl.get_or("blendTime", 0.1f);
                auto& p = ev.payload.playAnimation;
                std::strncpy(p.clipName, clipName.c_str(), sizeof(p.clipName) - 1);
                p.clipName[sizeof(p.clipName) - 1] = '\0';
                p.blendTime = blendTime;
                break;
            }
            case SkillEventType::PlayVFX: {
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

                // offset: attach-local position offset (right, up, forward). Zero if omitted.
                sol::optional<sol::table> offset = evTbl["offset"];
                if (offset) {
                    p.localOffset = mu::Vec3( (*offset).get_or(1, 0.f),
                            (*offset).get_or(2, 0.f),
                            (*offset).get_or(3, 0.f)
                    );
                }

                // orient: attach-local orientation offset { yaw, pitch, roll } degrees (same convention as OBB).
                // Aims the effect relative to the caster (e.g. a sector pointing 45 deg to the left).
                sol::optional<sol::table> vfxOrient = evTbl["orient"];
                if (vfxOrient) {
                    p.localEulerDeg = mu::Vec3( (*vfxOrient).get_or(1, 0.f),
                            (*vfxOrient).get_or(2, 0.f),
                            (*vfxOrient).get_or(3, 0.f)
                    );
                }

                // advance: attach-local particle travel direction. Omitted/zero -> derived from orientation.
                sol::optional<sol::table> advance = evTbl["advance"];
                if (advance) {
                    p.advanceForwardLocal = mu::Vec3( (*advance).get_or(1, 0.f),
                            (*advance).get_or(2, 0.f),
                            (*advance).get_or(3, 0.f)
                    );
                }

                // groundLock: place the effect on the ground plane (ignore caster pitch/roll).
                if (evTbl.get_or("groundLock", false))
                    p.flags |= kPlayVFXFlagYawOnly;
                // groundSnap: drop the effect onto the terrain surface at its XZ
                // (localOffset.y becomes the lift above ground). groundAlign tilts
                // the effect to the surface normal.
                if (evTbl.get_or("groundSnap", false))
                    p.flags |= kPlayVFXFlagGroundSnap;
                if (evTbl.get_or("groundAlign", false))
                    p.flags |= kPlayVFXFlagGroundAlign;

                // particleCollision / particleConform: drive the played effect's
                // particle behaviour (fall-and-die, slope conform) from the skill,
                // so no ground info is needed in the Unity-exported effect JSON.
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
            case SkillEventType::CameraShake: {
                auto& p    = ev.payload.cameraShake;
                p.magnitude = evTbl.get_or("magnitude", 0.f);
                p.duration  = Milliseconds{ static_cast<float>(evTbl.get_or("durationMs", 0)) };
                break;
            }
            case SkillEventType::ModifyStat: {
                auto& p        = ev.payload.modifyStat;
                p.hpDelta      = evTbl.get_or("hpDelta", 0);
                p.speedMultiplier = evTbl.get_or("speedMultiplier", 1.f);
                p.duration     = Milliseconds{ static_cast<float>(evTbl.get_or("durationMs", 0)) };
                break;
            }
            default:
                break;
            }

            asset.timeline.push_back(ev);
        });
    }

    // Sort timeline by time (ascending) for deterministic execution
    std::sort(asset.timeline.begin(), asset.timeline.end(),
              [](const TimelineEvent& a, const TimelineEvent& b) {
                  return a.time.count() < b.time.count();
              });

    return asset;
}

void SkillCompiler::registerAPI() {
    lua_.open_libraries(sol::lib::base, sol::lib::table, sol::lib::math, sol::lib::string);

    // Skill builder API -- returns a table the caller modifies then returns
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
            // Copy all params into ev
            params.for_each([&](sol::object key, sol::object val) {
                ev[key] = val;
            });
            self["events"].get<sol::table>().add(ev);
        });
        tbl.set_function("addVFX", [](sol::table self, int idx, std::string path) {
            self["vfxNames"].get<sol::table>()[idx + 1] = path;  // 1-indexed in Lua
        });
        return tbl;
    });
}

std::optional<SkillAsset> SkillCompiler::compileSingle(const std::filesystem::path& luaPath,
                                                        const Skeleton*               pSkeleton) {
    if (!std::filesystem::exists(luaPath)) return std::nullopt;
    registerAPI();
    // Load skill_api.lua from <skillDir>/lua/ relative to this file
    auto apiPath = luaPath.parent_path() / "lua" / "skill_api.lua";
    if (std::filesystem::exists(apiPath))
        lua_.safe_script_file(apiPath.string());
    try {
        sol::protected_function_result result = lua_.safe_script_file(luaPath.string());
        if (!result.valid()) {
            sol::error err = result;
            std::cerr << "[SkillCompiler] Error in " << luaPath << ": " << err.what() << "\n";
            return std::nullopt;
        }
        if (!result.get<sol::object>().is<sol::table>()) return std::nullopt;
        return tableToAsset(result.get<sol::table>(), pSkeleton);
    } catch (const std::exception& ex) {
        std::cerr << "[SkillCompiler] Exception: " << ex.what() << "\n";
        return std::nullopt;
    }
}

std::vector<SkillAsset> SkillCompiler::compileAll(const std::filesystem::path& skillDir,
                                                   const Skeleton*               pSkeleton) {
    std::vector<SkillAsset> assets;
    if (!std::filesystem::exists(skillDir)) return assets;

    registerAPI();

    // Load skill_api.lua once before iterating individual skill files
    auto apiPath = skillDir / "lua" / "skill_api.lua";
    if (std::filesystem::exists(apiPath)) {
        auto r = lua_.safe_script_file(apiPath.string());
        if (!r.valid()) {
            sol::error err = r;
            std::cerr << "[SkillCompiler] Failed to load skill_api.lua: " << err.what() << "\n";
        }
    }

    for (const auto& entry : std::filesystem::directory_iterator(skillDir)) {
        if (entry.path().extension() != ".lua") continue;
        try {
            sol::protected_function_result result = lua_.safe_script_file(entry.path().string());
            if (!result.valid()) {
                sol::error err = result;
                std::cerr << "[SkillCompiler] Error in " << entry.path() << ": " << err.what() << "\n";
                continue;
            }
            if (!result.get<sol::object>().is<sol::table>()) continue;
            assets.push_back(tableToAsset(result.get<sol::table>(), pSkeleton));
        } catch (const std::exception& ex) {
            std::cerr << "[SkillCompiler] Exception: " << ex.what() << "\n";
        }
    }

    // Assign sequential IDs
    for (u32t i = 0; i < static_cast<u32t>(assets.size()); ++i)
        assets[i].id = i + 1;

    return assets;
}
