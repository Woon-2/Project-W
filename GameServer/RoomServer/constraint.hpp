#ifndef room_server_constraint_hpp
#define room_server_constraint_hpp

// Abstract base for all velocity-level + position-level constraints.
// Concrete types: ContactConstraint.
//
// The PhysicsWorld::step() pipeline calls these in order:
//   1. prepare(dt)     - cache effective masses, compute biases
//   2. solveVelocity() - PGS velocity-level impulse (called N times)
//   3. solvePosition() - position-level correction (called once)
class Constraint {
public:
    virtual ~Constraint() = default;

    virtual void prepare(Seconds dt) = 0;
    virtual void solveVelocity() = 0;
    virtual void solvePosition() = 0;
};

#endif // room_server_constraint_hpp
