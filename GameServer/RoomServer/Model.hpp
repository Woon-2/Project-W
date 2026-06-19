#ifndef room_server_model_hpp
#define room_server_model_hpp

#include "collision.hpp"

// Minimal bone descriptor for server-side animation.
// Stores the bind-pose transform (bone-local -> model space) and parent index.
struct ServerBone {
	std::string  name;
	int          parentIdx = -1;   // -1 for root
	mu::Mat4x4   toDress;          // bind pose: bone-local -> model (dress) space
};

// Skeleton used by the server to resolve BVH node bone indices and provide
// bind-pose transforms for animated hit detection.
struct ServerSkeleton {
	std::string name;
	std::vector<ServerBone>                  bones;
	std::unordered_map<std::string, int>     nameToIdx;

	bool empty() const { return bones.empty(); }
	int  boneCount() const { return static_cast<int>(bones.size()); }
};

// Server-side model: collision geometry + skeleton for animated hit detection.
struct Model {
	std::string    name;
	mu::Vec3       baseScale{ 1.f, 1.f, 1.f };   // Unity root localScale; applied at runtime via body scale
	BVH            bvh;
	ServerSkeleton skeleton;
};

// Reads a model from its binary file.
// * When modifying: the Unity extraction script must stay symmetric.
Model loadModelFromFile(const std::filesystem::path& path);

#endif // room_server_model_hpp