#include "SampleScene.hpp"

#include "d3d12res.hpp"

namespace gfx {

void SampleDrawable::completeInit(d3d12::Core& core) {
    core.popTmpUpBuf("Gun_vb");
    core.popTmpUpBuf("Gun_ib");
}

std::optional<const DrawInfo> SampleScene::getDrawInfo() const {
    if (drawn_) {
        return {};
    }

    auto ret = DrawInfo();

    ret.set(meshIdx, &pDrawable_->mesh());

    drawn_ = true;

    return ret;
}

}   // namespace gfx