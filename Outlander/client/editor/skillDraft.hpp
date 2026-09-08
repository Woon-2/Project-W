#ifndef __editor_skillDraft_HPP
#define __editor_skillDraft_HPP

// SkillDraft -- an editable working copy of a compiled SkillAsset.
//
// The editor never rewrites the .lua source directly. Instead it keeps two deep
// copies of the selected skill: `original_` (pristine, as compiled) and `draft_`
// (mutated by the editor). The draft is what the SkillSystem plays back, so edits
// are reflected live. dumpDiff() prints original / current / delta for every
// changed field plus a lua-ready hint, as a manual edit guide.
//
// Editing is driven by a flat list of scalar Fields built for a selected hitbox
// definition: per-OBB transform (center/halfExtents/euler), the def's onHit
// response, its spawn/destroy timeline event times, and the global skill duration.

#include "../skill/skillTypes.hpp"

#include <string>
#include <vector>

namespace Editor {

class SkillDraft {
public:
    enum class FieldType {
        CenterX, CenterY, CenterZ,
        HalfX,   HalfY,   HalfZ,
        Yaw,     Pitch,   Roll,
        Damage,  ImpulseStrength,
        ImpulseDirX, ImpulseDirY, ImpulseDirZ,
        SpawnTimeMs, DestroyTimeMs,
        TotalDurationMs,
    };

    struct Field {
        std::string label;
        FieldType   type;
        int         defIdx     = -1;  // hitboxDefs index (hitbox + onHit fields)
        int         obbIdx     = -1;  // localOBBs index (transform fields)
        int         eventIdx   = -1;  // draft timeline index (time fields)
        float       step       = 0.1f;
        float       coarseStep = 1.0f;
    };

    bool valid() const { return loaded_; }

    // Deep-copy the source asset into both original_ and draft_.
    void load(const SkillAsset& src);

    const SkillAsset& draft() const { return draft_; }
    const std::string& name() const { return draft_.name; }

    // Build the editable field list for the given hitbox def. Pass defIdx < 0 to
    // list only the global fields (total duration).
    void buildFields(int defIdx);
    const std::vector<Field>& fields() const { return fields_; }
    int   selectedDefIdx() const { return selectedDefIdx_; }

    float readDraft   (const Field& f) const;
    float readOriginal(const Field& f) const;

    // Apply a signed delta to a draft field. outResort is set true when a timeline
    // re-sort happened (time edit) -- the caller must rebuild the field list because
    // event indices changed.
    bool  applyDelta(const Field& f, float delta, bool& outResort);

    // True if any field of this def's OBBs / onHit was changed vs original.
    // Used to know whether a live hitbox needs syncing.
    const SkillHitboxDef* draftHitboxDef(int defIdx) const;

    // Print the full diff (original / current / delta + lua hint) to the log.
    void dumpDiff() const;

private:
    bool       loaded_         = false;
    int        selectedDefIdx_ = -1;
    SkillAsset original_;
    SkillAsset draft_;
    std::vector<Field> fields_;

    // Maps each draft timeline slot to its index in original_.timeline, so time
    // diffs stay correct even after the draft timeline is re-sorted.
    std::vector<int> draftEventOrig_;

    static float readField(const SkillAsset& a, const Field& f, const std::vector<int>* eventMap);
    void rebuildOrient(int defIdx, int obbIdx);
    void resortTimeline();
};

}   // namespace Editor

#endif  // __editor_skillDraft_HPP
