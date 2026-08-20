#pragma once
#include <Windows.h>

namespace util {
template <typename T>
void writeMemoryForeign(void* address, const T& value) {
    DWORD oldProtect;

    VirtualProtect(address, sizeof(T), PAGE_READWRITE, &oldProtect);
    *reinterpret_cast<T*>(address) = value;
    VirtualProtect(address, sizeof(T), oldProtect, nullptr);
}
}  // namespace util
