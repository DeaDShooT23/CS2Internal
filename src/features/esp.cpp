#include "esp.hpp"
#include "../core/globals.hpp"
#include "../sdk/offsets.hpp"

#include <imgui/imgui.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <string>

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

        float x = matrix.matrix[0][0] * pos.x + matrix.matrix[0][1] * pos.y +
                  matrix.matrix[0][2] * pos.z + matrix.matrix[0][3];
        float y = matrix.matrix[1][0] * pos.x + matrix.matrix[1][1] * pos.y +
                  matrix.matrix[1][2] * pos.z + matrix.matrix[1][3];

        x /= w;
        y /= w;
        screen.x = (windowSize.x / 2.0f) * (x + 1.0f);
        screen.y = (windowSize.y / 2.0f) * (1.0f - y);
        return true;
    }

    void render() {
        if (!globals::esp_enabled)
            return;

        if (!client_base) {
            client_base = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
            if (!client_base)
                return;
        }

        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        ImVec2 screen_size = ImGui::GetIO().DisplaySize;

        ViewMatrix view_matrix;
        uintptr_t entity_list;

        uintptr_t local_pawn = 0;
        uint8_t local_team = 0;
        if (SafeRead(client_base + dwLocalPlayerPawn, local_pawn) && local_pawn) {
            SafeRead(local_pawn + m_iTeamNum, local_team);
        }

        int actual_players_drawn = 0;

        if (SafeRead(client_base + dwViewMatrix, view_matrix) &&
            SafeRead(client_base + dwEntityList, entity_list) && entity_list) {
            // Wir scannen weit genug für alle Offline-Bots (bis 16.384)
            for (int i = 1; i < 16384; ++i) {
                uintptr_t list_entry;
                if (!SafeRead(entity_list + (8 * (i >> 9) + 0x10), list_entry) || !list_entry)
                    continue;

                // 1. Die Adresse des "CEntityIdentity" Blocks berechnen
                uintptr_t identity_address = list_entry + 0x78 * (i & 0x1FF);

                // 2. TEMPLEWARE MAGIC CHECK: Liegt hier wirklich die ID, die wir suchen?
                uint32_t handle;
                if (!SafeRead(identity_address + 0x10, handle) || (handle & 0x7FFF) != i)
                    continue;

                // 3. Den Pawn (Spielfigur) aus Offset 0 des Identity-Blocks lesen
                uintptr_t pawn;
                if (!SafeRead(identity_address, pawn) || !pawn)
                    continue;

                if (pawn == local_pawn)
                    continue;

                // --- AB HIER SIND DIE DATEN 100% SAUBER ---

                int health = 0;
                if (!SafeRead(pawn + m_iHealth, health) || health <= 0 || health > 200)
                    continue;

                // WICHTIG: Team als uint8_t auslesen, sonst gibt es Datenmüll
                uint8_t team = 0;
                if (!SafeRead(pawn + m_iTeamNum, team) || team < 2)
                    continue;  // T = 2, CT = 3

                Vector3 feet_pos;
                if (!SafeRead(pawn + m_vOldOrigin, feet_pos))
                    continue;
                if (feet_pos.x == 0.0f && feet_pos.y == 0.0f)
                    continue;

                ImVec2 feet_screen, head_screen;
                Vector3 head_pos = {feet_pos.x, feet_pos.y, feet_pos.z + 72.0f};

                if (WorldToScreen(feet_pos, feet_screen, view_matrix, screen_size) &&
                    WorldToScreen(head_pos, head_screen, view_matrix, screen_size)) {
                    actual_players_drawn++;

                    float h = feet_screen.y - head_screen.y;
                    float w = h / 2.0f;

                    ImU32 color = (team == local_team && local_team != 0)
                                      ? IM_COL32(0, 255, 0, 255)
                                      : IM_COL32(255, 0, 0, 255);

                    draw_list->AddRect({head_screen.x - w / 2, head_screen.y},
                                       {head_screen.x + w / 2, feet_screen.y}, color, 0.0f, 0,
                                       1.5f);
                }
            }
        }

        std::string stats = "Real Players Drawn: " + std::to_string(actual_players_drawn);
        draw_list->AddText({15, 15}, IM_COL32(0, 255, 255, 255), stats.c_str());
    }
}  // namespace esp
