#pragma once
#include <cstdint>  // for std::uint8_t
#include <cstddef>  // for std::size_t
#include <string>   // für std::string

namespace sdk {
    // HIER IST DIE FEHLENDE ZEILE:
    // Macht die LogMem-Funktion im gesamten Projekt verfügbar
    void LogMem(const std::string& message);

    template <std::size_t Size>
    class padding {
    private:
        std::uint8_t m_pad[Size];
    };

    template <class Function = void*, std::size_t Index>
    Function virtual_function_get(void* vmt) {
        return (*static_cast<Function**>(vmt))[Index];
    }

    std::uint8_t* find_pattern(const char* module_name, const char* pattern);

    std::uint8_t* resolve_absolute_rip_address(std::uint8_t* instruction,
                                               std::size_t offset_to_displacemnt,
                                               std::size_t instruction_size);

}  // namespace sdk