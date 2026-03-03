#include "pch.hpp"

// referenced from
// https://stackoverflow.com/questions/150355/programmatically-find-the-number-of-cores-on-a-machine
size_t numberOfPhysicalCores() noexcept {
    DWORD length = 0;
    const BOOL result_first = GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
    assert(GetLastError() == ERROR_INSUFFICIENT_BUFFER);

    std::unique_ptr< std::byte[] > buffer(new std::byte[length]);
    const PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info =
        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.get());

    const BOOL result_second = GetLogicalProcessorInformationEx(RelationProcessorCore, info, &length);
    assert(result_second != FALSE);

    size_t nb_physical_cores = 0;
    size_t offset = 0;
    do {
        const PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX current_info =
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.get() + offset);
        offset += current_info->Size;
        ++nb_physical_cores;
    } while (offset < length);

    return nb_physical_cores;
}
