#ifndef __StaticDepenetration_HPP
#define __StaticDepenetration_HPP

#include "rigidBody.hpp"   // pulls in ContactPoint via collision.hpp
#include <vector>

// ---------------------------------------------------------------------------
// Static-object depenetration
// ---------------------------------------------------------------------------
//
// Collisions that involve a Static body (Static-Dynamic, Static-Kinematic) are
// NOT resolved through the velocity-level ContactConstraint solver. A Static
// body has infinite mass, so feeding it through the sequential-impulse solver
// is wasteful and can produce wrong results: the offset contact point injects
// spurious spin via the r x n torque term, and the Baumgarte bias injects real
// separating velocity that launches the body off the surface.
//
// Instead, a Static contact is resolved the way an AAA character controller
// depenetrates against level geometry: push the movable body out along the
// contact normal by the penetration amount only (positional correction), and
// clamp the inward normal velocity component to zero. No rotation is applied,
// and no separating velocity is injected, so the body settles against the
// surface without jitter.
//
// Kinematic bodies report invMass() == 0, so the impulse solver cannot move
// them at all (the reason a Kinematic body currently tunnels through Static
// geometry). This path bypasses invMass entirely and writes pos()/linearVel()
// directly, which is legitimate because depenetration is allowed to override
// game-driven kinematic motion.

// One depenetration record: a movable body (Dynamic or Kinematic) overlapping a
// single Static body. Holds the contact manifold (up to 4 points). The contact
// normal points from the Static body toward the movable body (push-out
// direction), unlike ContactPoint's usual B->A convention.
struct StaticContact {
    RigidBody*   movable    = nullptr;  // Dynamic or Kinematic; moved directly
    RigidBody*   staticBody = nullptr;  // never moved
    ContactPoint contacts[4];
    int          count = 0;

    void addContact(const ContactPoint& cp) {
        if (count < 4)
            contacts[count++] = cp;
    }
};

namespace staticdepen {
    // Penetration slop: overlaps below this are left unresolved so the body
    // rests at a stable sub-slop depth instead of being pushed fully to the
    // surface and re-detected as separated next frame (the push/fall jitter).
    // Matches ContactConstraint::kSlop for consistency.
    inline constexpr float kSlop        = 0.005f;

    // Partial positional correction per sub-step. Removing the full penetration
    // in one shot overshoots and re-penetrates, producing a buzz; 0.8 converges
    // geometrically while damping oscillation (Baraff/Catto partial correction).
    inline constexpr float kCorrectFrac = 0.8f;

    // Clamp on a single sub-step's correction so a deep tunneling event eases
    // out over a few sub-steps instead of teleporting the body in one frame.
    inline constexpr float kMaxCorrect  = 0.2f;
}

// Resolve every static contact in one pass:
//   - Positional correction: push each movable body out along each contact
//     normal by kCorrectFrac * (depth - kSlop), clamped to kMaxCorrect. Per
//     movable body the correction is max-combined along each normal so two
//     near-orthogonal walls (a corner) each push along their own axis without
//     double-counting a shared axis. Applied once at the end as a direct
//     position write. Static bodies never move.
//   - Velocity clamp: remove only the inward (into-surface) normal component
//     (restitution 0). Tangential sliding is preserved; no separating velocity
//     is injected.
// Bodies whose position was changed are appended to outMoved so the caller can
// rebuild their world-space BVH (it was built against the pre-correction pose).
void resolveStaticPenetration(std::vector<StaticContact>& contacts,
                              std::vector<RigidBody*>& outMoved);

#endif // __StaticDepenetration_HPP
