#ifndef __SAMPLE_SCENE_HPP
#define __SAMPLE_SCENE_HPP

#include "gfx.hpp"

namespace gfx {

class SampleScene : public IScene {
public:
    std::optional<const DrawInfo> getDrawInfo() const override {
        return {};
    }
};

} // namespace gfx

#endif // __SAMPLE_SCENE_HPP