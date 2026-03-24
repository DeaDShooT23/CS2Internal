#include "memory_utils.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif  // !WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>                 // WICHTIG: Für GetModuleInformation
#pragma comment(lib, "psapi.lib")  // Verhindert Linker-Fehler in Visual Studio

#include <vector>
#include <stdexcept>
#include <format>
#include <fstream>
#include <string>

namespace sdk {

    void LogMem(const std::string& message) {
        std::ofstream log_file("cs2_cheat_log.txt", std::ios::app);
        if (log_file.is_open()) {
            log_file << message << std::endl;
        }
    }

    static std::vector<int> ida_pattern_to_bytes(const char* pattern) {
        std::vector<int> bytes = {};
        char* start = const_cast<char*>(pattern);
        const char* end = const_cast<char*>(pattern) + std::strlen(pattern);

        for (char* current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;
                if (*current == '?') {
                    ++current;
                }
                bytes.push_back(-1);
            } else {
                bytes.push_back(std::strtoul(current, &current, 16));
            }
        }
        return bytes;
    }

    std::uint8_t* find_pattern(const char* module_name, const char* pattern) {
        LogMem(std::string("[SCAN] Suche gestartet für Modul: ") + module_name);

        const HMODULE module_handle = GetModuleHandleA(module_name);
        if (module_handle == nullptr) {
            LogMem("[SCAN-FEHLER] Modul ist im Spiel nicht geladen!");
            return nullptr;
        }

        MODULEINFO module_info;
        if (!K32GetModuleInformation(GetCurrentProcess(), module_handle, &module_info,
                                     sizeof(MODULEINFO))) {
            LogMem("[SCAN-FEHLER] GetModuleInformation fehlgeschlagen!");
            return nullptr;
        }

        auto* base_address = reinterpret_cast<std::uint8_t*>(module_info.lpBaseOfDll);
        const DWORD image_size = module_info.SizeOfImage;

        const std::vector<int> bytes = ida_pattern_to_bytes(pattern);
        const std::size_t pattern_size = bytes.size();
        const int* pattern_data = bytes.data();

        LogMem("[SCAN] Iteriere über Speicherbereiche mit VirtualQuery...");

        std::uint8_t* current_address = base_address;
        std::uint8_t* end_address = base_address + image_size;

        while (current_address < end_address) {
            MEMORY_BASIC_INFORMATION mbi;
            // Frage Windows nach den Rechten für diesen Speicherbereich
            if (!VirtualQuery(current_address, &mbi, sizeof(mbi))) {
                break;
            }

            // Nur Bereiche durchsuchen, die commited und lesbar sind
            bool is_readable = (mbi.Protect & PAGE_READONLY) || (mbi.Protect & PAGE_READWRITE) ||
                               (mbi.Protect & PAGE_EXECUTE_READ) ||
                               (mbi.Protect & PAGE_EXECUTE_READWRITE);

            if (mbi.State == MEM_COMMIT && is_readable) {
                std::uint8_t* region_start = reinterpret_cast<std::uint8_t*>(mbi.BaseAddress);
                std::size_t region_size = mbi.RegionSize;

                // Nicht über das Ende der DLL hinaus lesen
                if (region_start + region_size > end_address) {
                    region_size = end_address - region_start;
                }

                if (region_size >= pattern_size) {
                    for (std::size_t j = 0ul; j < region_size - pattern_size; ++j) {
                        bool found = true;
                        for (std::size_t k = 0ul; k < pattern_size; ++k) {
                            if (region_start[j + k] != pattern_data[k] && pattern_data[k] != -1) {
                                found = false;
                                break;
                            }
                        }
                        if (found) {
                            LogMem("[SCAN] ERFOLG: Pattern gefunden!");
                            return &region_start[j];
                        }
                    }
                }
            }
            // Zum nächsten Speicherblock springen
            current_address += mbi.RegionSize;
        }

        LogMem("[SCAN-FEHLER] Pattern im Speicher nicht gefunden.");
        return nullptr;
    }

    std::uint8_t* resolve_absolute_rip_address(std::uint8_t* instruction,
                                               std::size_t offset_to_displacement,
                                               std::size_t instruction_size) {
        auto displacement = *reinterpret_cast<const std::int32_t*>(
            reinterpret_cast<std::uintptr_t>(instruction) + offset_to_displacement);
        return instruction + instruction_size + displacement;
    }
}  // namespace sdk