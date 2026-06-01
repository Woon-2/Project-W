#ifndef __rs_skill_skillCompiler_HPP
#define __rs_skill_skillCompiler_HPP

// Server-side Lua skill compiler.
// Mirrors client/skill/skillCompiler.hpp but without the Skeleton dependency
// (server has no animation data; bone names remain as strings for approximation).

#include "skillTypes.hpp"
#include <sol/sol.hpp>

class ServerSkillCompiler {
public:
    std::vector<SkillAsset> compileAll(const std::filesystem::path& skillDir);

private:
    sol::state lua_;
    void       registerAPI();
    SkillAsset     tableToAsset(const sol::table& tbl);
    SkillHitboxDef tableToHitboxDef(const sol::table& tbl);
    OnHitDef       tableToOnHitDef(const sol::table& tbl);
};

#endif  // __rs_skill_skillCompiler_HPP
