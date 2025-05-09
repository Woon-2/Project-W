#ifndef __IDPool_HPP
#define __IDPool_HPP

#include <cstdint>
#include <forward_list>
#include <optional>

class IDPool {
public:
    static void initList();

    static std::optional<std::uint16_t> allocID();

    static void deallocID(std::uint16_t id);
    
private:
    static std::forward_list<std::uint16_t> idList_;
};

#endif // __IDPool_HPP