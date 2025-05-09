#include "IDPool.hpp"

#include <numeric>

void IDPool::initList() {
    idList_.resize(0xFFFF);
    std::iota(idList_.begin(), idList_.end(), 0u);
}

std::optional<std::uint16_t> IDPool::allocID() {
    if(idList_.empty()){
        return std::nullopt;
    }

    auto id = idList_.front();
    idList_.pop_front();

    return id;
}

void IDPool::deallocID(std::uint16_t id) {
    idList_.push_front(id);
}

std::forward_list<std::uint16_t> IDPool::idList_;