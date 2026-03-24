#include "esp.hpp"
#include "../core/globals.hpp"
#include "../sdk/offsets.hpp"

#include <imgui/imgui.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>

using namespace cs2_dumper::offsets::client_dll;
using namespace netvars;

namespace esp {
    static uintptr_t client_base = 0;

    struct Vector3 {
        float x, y, z;
    };
    struct ViewMatrix {
        float matrix[4][4];
    };

    template <typename T>
    bool SafeRead(uintptr_t address, T& out) {
        if (address < 0x100000 || address > 0x7FFFFFFEFFFF)
            return false;
        if (IsBadReadPtr(reinterpret_cast<const void*>(address), sizeof(T)))
            return false;
        out = *reinterpret_cast<T*>(address);
        return true;
    }

    bool WorldToScreen(const Vector3& pos, ImVec2& screen, const ViewMatrix& matrix,
                       ImVec2 windowSize) {
        float w = matrix.matrix[3][0] * pos.x + matrix.matrix[3][1] * pos.y +
                  matrix.matrix[3][2] * pos.z + matrix.matrix[3][3];
        if (w < 0.01f)
            return false;
        float x = (matrix.matrix[0][0] * pos.x + matrix.matrix[0][1] * pos.y +
                   matrix.matrix[0][2] * pos.z + matrix.matrix[0][3]) /
                  w;
        float y = (matrix.matrix[1][0] * pos.x + matrix.matrix[1][1] * pos.y +
                   matrix.matrix[1][2] * pos.z + matrix.matrix[1][3]) /
                  w;
        screen.x = (windowSize.x / 2.0f) * (x + 1.0f);
        screen.y = (windowSize.y / 2.0f) * (1.0f - y);
        return true;
    }

    void render() {
        if (!globals::esp_enabled)
            return;
        if (!client_base) {
            client_base = (uintptr_t)GetModuleHandleA("client.dll");
            if (!client_base)
                return;
        }

        // Zeichnen direkt auf den Hintergrund (kein Fenster = keine Abdunkelung)
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        ImVec2 screen_size = ImGui::GetIO().DisplaySize;

        ViewMatrix view_matrix;
        uintptr_t entity_list;
        if (!SafeRead(client_base + dwViewMatrix, view_matrix) ||
            !SafeRead(client_base + dwEntityList, entity_list))
            return;

        uintptr_t local_pawn = 0;
        int local_team = 0;
        SafeRead(client_base + dwLocalPlayerPawn, local_pawn);
        if (local_pawn)
            SafeRead(local_pawn + m_iTeamNum, local_team);

        // Scan durch die ersten 1024 Slots (reicht für alle Spieler/Bots)
        for (int i = 1; i < 1024; i++) {
            uintptr_t list_entry;
            if (!SafeRead(entity_list + (8 * (i >> 9) + 0x10), list_entry) || !list_entry)
                continue;

            uintptr_t pawn;
            if (!SafeRead(list_entry + 0x78 * (i & 0x1FF), pawn) || !pawn || pawn == local_pawn)
                continue;

            // --- TEMPLEWARE LOGIK: Validierung des Eintrags ---
            uint32_t entity_index;
            if (!SafeRead(pawn + 0x10, entity_index) || (entity_index & 0x7FFF) != i)
                continue;

            int health, team;
            if (!SafeRead(pawn + m_iHealth, health) || health <= 0 || health > 1000)
                continue;
            if (!SafeRead(pawn + m_iTeamNum, team))
                continue;

            // Position (m_vOldOrigin = 0x1588)
            Vector3 feet_pos;
            if (!SafeRead(pawn + m_vOldOrigin, feet_pos) || (feet_pos.x == 0 && feet_pos.y == 0))
                continue;

            ImVec2 feet_screen, head_screen;
            Vector3 head_pos = {feet_pos.x, feet_pos.y, feet_pos.z + 72.0f};

            if (WorldToScreen(feet_pos, feet_screen, view_matrix, screen_size) &&
                WorldToScreen(head_pos, head_screen, view_matrix, screen_size)) {
                float h = feet_screen.y - head_screen.y;
                float w = h / 2.0f;

                ImU32 color =
                    (team == local_team) ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
                draw_list->AddRect({head_screen.x - w / 2, head_screen.y},
                                   {head_screen.x + w / 2, feet_screen.y}, color);
            }
        }
    }
}  // namespace esp