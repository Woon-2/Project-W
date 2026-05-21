#ifndef __skill_skillCompiler_HPP
#define __skill_skillCompiler_HPP

// Skill compiler: converts Lua skill definition files into SkillAsset instances.
//
// DEPENDENCY: requires sol2 (https://github.com/ThePhD/sol2), a header-only C++/Lua binding.
//   1. Download sol2 (single header: sol/sol.hpp or sol2/sol.hpp).
//   2. Place under GameServer/client/sol2/ or add to the include path.
//   3. Ensure Lua 5.4 is linked (lua54.dll on Windows).
//   4. This file is the ONLY place in the skill system that includes sol2.
//      skillSystem.hpp and skillSystem.cpp have zero Lua/sol2 dependencies.
//
// Usage (call once at startup before SkillSystem::update):
//   SkillCompiler compiler;
//   auto assets = compiler.compileAll("../resources/skills", pSkeleton);
//   skillSystem_.registerAssets(std::move(assets));

#include "skillTypes.hpp"
#include "../mesh.hpp"  // Skeleton, Bone -- for bone name resolution

// sol2 is included only here -- runtime code never includes sol2
#include <sol/sol.hpp>

class SkillCompiler {
public:
    // Compile all *.lua files under skillDir.
    // pSkeleton: if provided, bone names in hitbox defs are resolved to indices.
    // If null, bone name resolution falls back to runtime lookup in resolveAttach().
    std::vector<SkillAsset> compileAll(const std::filesystem::path& skillDir,
                                       const Skeleton*               pSkeleton = nullptr);

    // Compile a single skill Lua file.
    std::optional<SkillAsset> compileSingle(const std::filesystem::path& luaPath,
                                            const Skeleton*               pSkeleton = nullptr);

private:
    sol::state lua_;
    void       registerAPI();
    SkillAsset     tableToAsset(const sol::table& tbl, const Skeleton* pSkeleton);
    SkillHitboxDef tableToHitboxDef(const sol::table& tbl);
    OnHitDef       tableToOnHitDef(const sol::table& tbl);
};

#endif  // __skill_skillCompiler_HPP
