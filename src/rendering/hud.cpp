#include "hud.h"
#include "config/game_constants.h"
#include "../../include/systems/audioManager.h"
#include "keybindManager.h"
#include "fonts.h"
#include "player.h"
#include "animation.h"
#include "inventory.h"
#include "inv-bst-sort.h"
#include "effectQueue.h"
#include "propsbehavior.h"
#include "entities.h"
#include "worldgenenartion.h"
#include "combatTurn.h"
#include "raymath.h"
#include <cstdio>
#include <vector>
#include <string>
#include <cmath>

extern const int GameScreenWidth;
extern const int GameScreenHeight;
extern Camera2D camera;

// Drag & Drop State
/** @brief Slot asal drag */
static int dragSlot = -1;
/** @brief Item yang sedang di-drag */
static InventoryItem dragItem = {-1, 0};

// Split stack state
/** @brief Flag mode split stack */
static bool isDragSplit = false;
/** @brief Total item sebelum split */
static int splitTotalAmount = 0;
/** @brief Slot yang sudah diisi saat split */
static std::vector<int> splitVisitedSlots;

// Inventory panel textures
static Texture2D invBgTex = {0};
static Texture2D invSlotGridTex = {0};
static bool invTexLoaded = false;

// HUD player textures (loaded once from assets/textures/hudPlayer/)
static Texture2D hudBagIcon = {0};
static Texture2D hudSettingsIcon = {0};
static Texture2D hudKillCount = {0};
static bool hudTexLoaded = false;

/*==============================================================================
 * Internal Helpers
 *==============================================================================*/

/**
 * @brief Render ikon item dari spritesheet ke rectangle tujuan.
 * @param item Item yang akan di-render.
 * @param dest Rectangle tujuan render.
 */
static void DrawItemIcon(const InventoryItem &item, Rectangle dest)
{
    if (item.definitionId == -1)
        return;

    const ItemDefinition &def = itemDefs.GetById(item.definitionId);
    const Frame &frame = GetFrame(def.spriteKey);

    Rectangle src = {
        (float)(frame.positionX * (FRAME_SIZE + FRAME_GAP)),
        (float)(frame.positionY * (FRAME_SIZE + FRAME_GAP)),
        (float)(frame.width * FRAME_SIZE),
        (float)(frame.height * FRAME_SIZE)};

    if (def.spriteKey == "sword2" || def.spriteKey == "axe")
    {
        src.width = 39.0f;
    }
    else if (def.spriteKey == "spear")
    {
        src.width = 50.0f;
    }

    int maxDim = (src.width > src.height) ? src.width : src.height;
    float size = dest.width / maxDim;

    float renderWidth = src.width * size;
    float renderHeight = src.height * size;

    Vector2 position = {
        dest.x + (dest.width - renderWidth) / 2.0f,
        dest.y + (dest.height - renderHeight) / 2.0f};

    bool isMelee = false;
    if (def.category == ITEM_WEAPON)
    {
        const WeaponData *wd = std::get_if<WeaponData>(&def.data);
        if (wd && wd->attackType != ATTACK_PIERCE)
        {
            isMelee = true;
        }
    }

    Vector2 origin = isMelee ? Vector2{renderWidth / 2.0f, renderHeight / 2.0f} : Vector2{0.0f, 0.0f};

    Rectangle drawDest = {
        position.x + origin.x,
        position.y + origin.y,
        renderWidth,
        renderHeight};

    DrawTexturePro(textures[frame.texture], src, drawDest, origin, isMelee ? -45.0f : 0.0f, WHITE);
}

/**
 * @brief Render teks dengan background rounded rectangle.
 * @param text Teks yang akan ditampilkan.
 * @param x, y Posisi teks.
 * @param fontSize Ukuran font.
 * @param color Warna teks.
 */
// DrawTextHUD pakai GetOrLoad(FontId::HUD_PLAYER) dengan padding tetap 4px
static void DrawTextHUD(const char *text, int x, int y, int fontSize, Color color)
{
    Vector2 textSize = MeasureTextEx(GetOrLoad(FontId::HUD_PLAYER), text, fontSize, 0);
    float pad = 4.0f;
    DrawRectangleRounded(
        (Rectangle){(float)x - pad, (float)y - pad, textSize.x + pad * 2, (float)fontSize + pad * 2},
        0.3f, 8, ColorAlpha(BLACK, 0.8f));
    DrawTextEx(GetOrLoad(FontId::HUD_PLAYER), text, Vector2{(float)x, (float)y}, fontSize, 0, color);
}

/**
 * @brief Ambil referensi item berdasarkan slot index global.
 * @param index 0-20 = Bag, 21-24 = Hotbar.
 * @return Referensi ke InventoryItem, atau slot dummy jika index invalid.
 */
static InventoryItem &GetItemBySlotIndex(int index)
{
    if (index >= 0 && index < PlayerInstance.GetMaxBag())
        return PlayerInstance.GetBagItem(index);
    if (index < PlayerInstance.GetMaxInventory())
        return PlayerInstance.GetHotbarItem(index - PlayerInstance.GetMaxBag());
    static InventoryItem empty = {-1, 0};
    return empty;
}

/*==============================================================================
 * Inventory Actions
 *==============================================================================*/

/**
 * @brief Handle split stack via drag klik kanan.
 * @param slotIndex Index slot global yang diproses.
 * @param slotRect Rectangle slot untuk deteksi hover.
 * @param mousePos Posisi mouse saat ini.
 * @note Dipanggil per slot di setiap frame. Split hanya berlaku untuk slot kosong.
 *       Jumlah item didistribusi ulang secara merata tiap slot baru ditambah.
 */
static void HandleSplitDragSlot(int slotIndex, Rectangle slotRect, Vector2 mousePos)
{
    InventoryItem &item = GetItemBySlotIndex(slotIndex);
    bool isHovered = CheckCollisionPointRec(mousePos, slotRect);

    // Mulai drag split jika klik kanan di slot berisi item stackable
    if (isHovered && InputInstance.IsRightClickDown() && dragSlot == -1 && item.definitionId != -1)
    {
        const ItemDefinition &def = itemDefs.GetById(item.definitionId);
        if (def.isStackable && item.amount > 1)
        {
            splitTotalAmount = item.amount;
            dragItem = {item.definitionId, 0};
            dragSlot = slotIndex;
            isDragSplit = true;
            splitVisitedSlots.clear();
            splitVisitedSlots.push_back(slotIndex);
        }
    }

    // Proses hover — isi slot kosong yang dilewati
    if (!isDragSplit || !isHovered)
        return;

    bool alreadyVisited = std::find(splitVisitedSlots.begin(), splitVisitedSlots.end(), slotIndex) != splitVisitedSlots.end();
    if (alreadyVisited || item.definitionId != -1)
        return;

    // Stop jika item tidak cukup untuk dibagi ke slot berikutnya
    int nextSlotCount = (int)splitVisitedSlots.size() + 1;
    if (splitTotalAmount / nextSlotCount == 0)
        return;

    // Redistribusi merata ke semua visited slot, sisa ke slot sumber (index 0)
    splitVisitedSlots.push_back(slotIndex);
    int slotCount = (int)splitVisitedSlots.size();
    int base = splitTotalAmount / slotCount;
    int remainder = splitTotalAmount % slotCount;

    for (int j = 0; j < slotCount; j++)
    {
        InventoryItem &s = GetItemBySlotIndex(splitVisitedSlots[j]);
        s = {dragItem.definitionId, base + (j == 0 ? remainder : 0)};
    }
}

/**
 * @brief Akhiri sesi drag split saat klik kanan dilepas.
 * @note Sisa item yang belum tersebar sudah otomatis ada di slot sumber
 *       karena redistribusi dilakukan tiap hover, tidak perlu kembalikan manual.
 */
static void HandleSplitRelease()
{
    if (InputInstance.IsRightClickReleased() && isDragSplit)
    {
        // Reset semua state split
        isDragSplit = false;
        dragSlot = -1;
        dragItem = {-1, 0};
        splitTotalAmount = 0;
        splitVisitedSlots.clear();
    }
}

/**
 * @brief Gabungkan semua item sejenis yang tersebar ke satu slot target.
 * @param slotIndex Index slot target yang akan menerima item.
 * @note Hanya mengambil dari slot yang belum maxStack.
 *       Berhenti jika slot target sudah mencapai maxStack.
 */
static void HandleMergeStack(int slotIndex)
{
    InventoryItem &target = GetItemBySlotIndex(slotIndex);
    if (target.definitionId == -1)
        return;

    const ItemDefinition &def = itemDefs.GetById(target.definitionId);
    if (!def.isStackable)
        return;
    if (target.amount >= def.maxStack)
        return;

    // Kumpulkan dari bag
    for (int i = 0; i < PlayerInstance.GetMaxBag() && target.amount < def.maxStack; i++)
    {
        if (i == slotIndex)
            continue;
        InventoryItem &other = PlayerInstance.GetBagItem(i);
        if (other.definitionId != target.definitionId)
            continue;
        if (other.amount >= def.maxStack)
            continue;

        int space = def.maxStack - target.amount;
        int take = std::min(space, other.amount);
        target.amount += take;
        other.amount -= take;
        if (other.amount <= 0)
            other = {-1, 0};
    }

    // Kumpulkan dari hotbar
    for (int i = 0; i < PlayerInstance.GetMaxHotbar() && target.amount < def.maxStack; i++)
    {
        int globalIdx = PlayerInstance.GetMaxBag() + i;
        if (globalIdx == slotIndex)
            continue;
        InventoryItem &other = PlayerInstance.GetHotbarItem(i);
        if (other.definitionId != target.definitionId)
            continue;
        if (other.amount >= def.maxStack)
            continue;

        int space = def.maxStack - target.amount;
        int take = std::min(space, other.amount);
        target.amount += take;
        other.amount -= take;
        if (other.amount <= 0)
            other = {-1, 0};
    }
}

/**
 * @brief Handle drop item ke slot tujuan — merge jika sejenis, swap jika beda.
 * @param toSlot Index slot tujuan.
 * @note Jika slot tujuan sama dengan sumber, drag dibatalkan.
 *       Partial merge terjadi jika slot tujuan tidak cukup menampung semua item.
 */
static void HandleDrop(int toSlot)
{
    // Batalkan drag jika drop ke slot yang sama
    if (toSlot == dragSlot)
    {
        dragSlot = -1;
        dragItem = {-1, 0};
        return;
    }

    if (dragItem.definitionId == -1)
    {
        dragSlot = -1;
        return;
    }

    InventoryItem &src = GetItemBySlotIndex(dragSlot);
    InventoryItem &dst = GetItemBySlotIndex(toSlot);
    const ItemDefinition &def = itemDefs.GetById(dragItem.definitionId);

    bool canStack = dst.definitionId != -1 && dst.definitionId == dragItem.definitionId && def.isStackable;

    if (canStack)
    {
        int spaceLeft = def.maxStack - dst.amount;
        if (spaceLeft <= 0)
        {
            // Slot tujuan penuh — swap biasa
            InventoryItem temp = dst;
            dst = dragItem;
            src = temp;
        }
        else if (dragItem.amount <= spaceLeft)
        {
            // Semua muat — merge penuh, slot asal kosong
            dst.amount += dragItem.amount;
            src = {-1, 0};
        }
        else
        {
            // Sebagian muat — partial merge
            dst.amount += spaceLeft;
            src.amount = dragItem.amount - spaceLeft;
        }
    }
    else
    {
        // Beda item atau non-stackable — swap biasa
        InventoryItem temp = dst;
        dst = dragItem;
        src = temp;
    }

    dragSlot = -1;
    dragItem = {-1, 0};
}

/**
 * @brief Render ghost icon item yang sedang di-drag mengikuti cursor.
 * @param mousePos Posisi mouse saat ini.
 */
static void DrawDragGhost(Vector2 mousePos)
{
    if (dragSlot == -1 || dragItem.definitionId == -1)
        return;
    float ghostSize = 36.0f;
    Rectangle dest = {
        mousePos.x - ghostSize / 2.0f,
        mousePos.y - ghostSize / 2.0f,
        ghostSize, ghostSize};
    DrawItemIcon(dragItem, dest);
}

/*==============================================================================
 * Inventory & Hotbar Rendering
 *==============================================================================*/

/**
 * @brief Render layar inventory beserta logika drag & drop, split, dan merge.
 * @note Hanya aktif saat inventory terbuka. Drop item ke luar area inventory
 *       akan spawn item di dunia arah mouse sejauh GetInteractRange.
 */
void DrawInventory()
{
    static bool wasOpen = false;
    bool isOpen = InputInstance.IsInventoryOpen();

    if (isOpen && !wasOpen)
    {
        Inventory::SortBagWithBst(PlayerInstance);
    }
    wasOpen = isOpen;

    if (!isOpen)
    {
        if (dragSlot != -1)
        {
            dragSlot = -1;
            dragItem = {-1, 0};
            isDragSplit = false;
            splitVisitedSlots.clear();
        }
        return;
    }

    if (!invTexLoaded)
    {
        Image img = LoadImage("assets/textures/inventory/inv-bg.png");
        invBgTex = LoadTextureFromImage(img);
        UnloadImage(img);
        img = LoadImage("assets/textures/inventory/inv-slot-fullgrid.png");
        invSlotGridTex = LoadTextureFromImage(img);
        UnloadImage(img);
        invTexLoaded = true;
    }

    const float bgW = 581.0f, bgH = 607.0f;
    const float gridW = 356.0f;
    const float slotSize = 77.0f, gap = 16.0f;

    const int bgX = (GameScreenWidth - (int)bgW) / 2;
    const int bgY = (GameScreenHeight - (int)bgH) / 2;
    const float gridX = (float)bgX + (bgW - gridW) / 2.0f;
    const float gridYOffset = 140.0f;
    const float gridY = (float)bgY + gridYOffset;

    DrawRectangle(0, 0, GameScreenWidth, GameScreenHeight, ColorAlpha(BLACK, 0.7f));
    DrawTextureV(invBgTex, {(float)bgX, (float)bgY}, WHITE);
    DrawTextureV(invSlotGridTex, {gridX, gridY}, WHITE);

    Vector2 mousePos = GetVirtualMousePosition(gState);
    bool mousePressed = InputInstance.IsLeftClickPressed();
    bool mouseReleased = InputInstance.IsLeftClickReleased();
    bool dragHandled = false;
    const int bagCols = 4;

    // === Bag grid (4x3) ===
    for (int i = 0; i < PlayerInstance.GetMaxBag(); i++)
    {
        int row = i / bagCols;
        int col = i % bagCols;
        Rectangle slotRect = {gridX + col * (slotSize + gap), gridY + row * (slotSize + gap), slotSize, slotSize};
        bool isHovered = CheckCollisionPointRec(mousePos, slotRect);
        bool isDragSource = (dragSlot == i);

        if (isHovered && !isDragSource)
            DrawRectangleRec(slotRect, ColorAlpha(WHITE, 0.15f));
        if (isDragSource)
            DrawRectangleRec(slotRect, ColorAlpha(GOLD, 0.25f));

        InventoryItem &item = PlayerInstance.GetBagItem(i);
        if (item.definitionId != -1 && !isDragSource)
        {
            float iconSize = 50.0f;
            Rectangle dest = {slotRect.x + (slotSize - iconSize) / 2.0f, slotRect.y + (slotSize - iconSize) / 2.0f, iconSize, iconSize};
            DrawItemIcon(item, dest);
            // stack amount bag: GetOrLoad(FontId::HUD_PLAYER) 18px dengan background rounded hitam
            if (item.amount > 1)
            {
                char buf[12];
                sprintf(buf, "%d", item.amount);
                Vector2 sz = MeasureTextEx(GetOrLoad(FontId::HUD_PLAYER), buf, 18, 0);
                float bx = slotRect.x + slotSize - 34;
                float by = slotRect.y + slotSize - 24;
                DrawRectangleRounded((Rectangle){bx - 4, by - 4, sz.x + 8, 18 + 8}, 0.3f, 8, ColorAlpha(BLACK, 0.8f));
                DrawTextEx(GetOrLoad(FontId::HUD_PLAYER), buf, Vector2{bx, by}, 18, 0, WHITE);
            }
        }

        if (isHovered && mousePressed && dragSlot == -1 && !isDragSplit && item.definitionId != -1)
        {
            if (InputInstance.IsCtrlDown())
                HandleMergeStack(i);
            else
            {
                dragSlot = i;
                dragItem = item;
            }
        }
        if (isHovered && mouseReleased && dragSlot != -1 && dragSlot != i && !isDragSplit)
        {
            HandleDrop(i);
            dragHandled = true;
        }
        if (isHovered && mouseReleased && dragSlot == i)
        {
            dragSlot = -1;
            dragItem = {-1, 0};
            dragHandled = true;
        }
        HandleSplitDragSlot(i, slotRect, mousePos);
    }

    // === Hotbar (4 slots) ===
    for (int i = 0; i < PlayerInstance.GetMaxHotbar(); i++)
    {
        int globalIdx = PlayerInstance.GetMaxBag() + i;
        Rectangle slotRect = {gridX + i * (slotSize + gap), gridY + 373.0f, slotSize, slotSize};
        bool isHovered = CheckCollisionPointRec(mousePos, slotRect);
        bool isDragSource = (dragSlot == globalIdx);

        if (isHovered && !isDragSource)
            DrawRectangleRec(slotRect, ColorAlpha(WHITE, 0.15f));
        if (isDragSource)
            DrawRectangleRec(slotRect, ColorAlpha(GOLD, 0.25f));

        InventoryItem &item = PlayerInstance.GetHotbarItem(i);
        if (item.definitionId != -1 && !isDragSource)
        {
            float iconSize = 50.0f;
            Rectangle dest = {slotRect.x + (slotSize - iconSize) / 2.0f, slotRect.y + (slotSize - iconSize) / 2.0f, iconSize, iconSize};
            DrawItemIcon(item, dest);
            // stack amount hotbar (inventory open): GetOrLoad(FontId::HUD_PLAYER) 18px
            if (item.amount > 1)
            {
                char buf[12];
                sprintf(buf, "%d", item.amount);
                Vector2 sz = MeasureTextEx(GetOrLoad(FontId::HUD_PLAYER), buf, 18, 0);
                float bx = slotRect.x + slotSize - 34;
                float by = slotRect.y + slotSize - 24;
                DrawRectangleRounded((Rectangle){bx - 4, by - 4, sz.x + 8, 18 + 8}, 0.3f, 8, ColorAlpha(BLACK, 0.8f));
                DrawTextEx(GetOrLoad(FontId::HUD_PLAYER), buf, Vector2{bx, by}, 18, 0, WHITE);
            }

            const ItemDefinition &def = itemDefs.GetById(item.definitionId);
            if (def.category == ITEM_POTION || def.category == ITEM_POISON)
            {
                const PotionData &potion = std::get<PotionData>(def.data);
                PotionCategory cat = potion.potionCategory;
                if (PlayerInstance.PotionCategoryCooldowns[cat] > 0.0f && PlayerInstance.PotionCategoryCooldownMax[cat] > 0.0f)
                {
                    float ratio = PlayerInstance.PotionCategoryCooldowns[cat] / PlayerInstance.PotionCategoryCooldownMax[cat];
                    float startAngle = 270.0f;
                    float endAngle = 270.0f - (360.0f * ratio);
                    Vector2 center = {
                        slotRect.x + slotRect.width / 2.0f,
                        slotRect.y + slotRect.height / 2.0f};
                    DrawCircleSector(center, iconSize / 2.0f + 4.0f, startAngle, endAngle, 36, ColorAlpha(BLACK, 0.65f));

                    char cdBuf[16];
                    float cd = PlayerInstance.PotionCategoryCooldowns[cat];
                    if (cd >= 1.0f)
                        snprintf(cdBuf, sizeof(cdBuf), "%d", (int)cd);
                    else
                        snprintf(cdBuf, sizeof(cdBuf), "%.1f", cd);
                    int cdFontSize = 26;
                    Vector2 cdSz = MeasureTextEx(GetOrLoad(FontId::HUD_PLAYER), cdBuf, cdFontSize, 0);
                    DrawTextEx(GetOrLoad(FontId::HUD_PLAYER), cdBuf, Vector2{slotRect.x + (slotRect.width - cdSz.x) / 2.0f, slotRect.y + (slotRect.height - cdSz.y) / 2.0f}, cdFontSize, 0, WHITE);
                }
            }
        }

        if (isHovered && mousePressed && dragSlot == -1 && !isDragSplit && item.definitionId != -1)
        {
            if (InputInstance.IsCtrlDown())
                HandleMergeStack(globalIdx);
            else
            {
                dragSlot = globalIdx;
                dragItem = item;
            }
        }
        if (isHovered && mouseReleased && dragSlot != -1 && dragSlot != globalIdx && !isDragSplit)
        {
            HandleDrop(globalIdx);
            dragHandled = true;
        }
        if (isHovered && mouseReleased && dragSlot == globalIdx)
        {
            dragSlot = -1;
            dragItem = {-1, 0};
            dragHandled = true;
        }
        HandleSplitDragSlot(globalIdx, slotRect, mousePos);
    }

    HandleSplitRelease();

    if (mouseReleased && dragSlot != -1 && !isDragSplit && !dragHandled)
    {
        InventoryItem &src = GetItemBySlotIndex(dragSlot);
        Vector2 playerCenter = PlayerInstance.GetCenter();
        Vector2 mouseWorld = GetScreenToWorld2D(mousePos, camera);
        Vector2 aimDir = Vector2Normalize(Vector2Subtract(mouseWorld, playerCenter));
        Vector2 facingDir = {0, 0};
        switch (PlayerInstance.Anim.direction)
        {
        case UP:
            facingDir = {0, -1};
            break;
        case DOWN:
            facingDir = {0, 1};
            break;
        case LEFT:
            facingDir = {-1, 0};
            break;
        case RIGHT:
            facingDir = {1, 0};
            break;
        }
        float dot = Vector2DotProduct(facingDir, aimDir);
        Vector2 dropDir = aimDir;
        if (dot < PlayerInstance.GetItemDropAngle())
        {
            float threshold = acosf(PlayerInstance.GetItemDropAngle());
            float cross = facingDir.x * aimDir.y - facingDir.y * aimDir.x;
            float sign = (cross >= 0) ? 1.0f : -1.0f;
            float cosA = cosf(threshold * sign);
            float sinA = sinf(threshold * sign);
            dropDir = {facingDir.x * cosA - facingDir.y * sinA, facingDir.x * sinA + facingDir.y * cosA};
        }
        Vector2 dropPos = {playerCenter.x + dropDir.x * PlayerInstance.GetInteractRange(), playerCenter.y + dropDir.y * PlayerInstance.GetInteractRange()};
        ItemSpawn dropped = itemData.CreateItem(dropPos, dragItem.definitionId);
        dropped.amount = dragItem.amount;
        dropped.dropImmunity = DROP_IMMUNITY_DURATION;
        itemData.activeItems.push_back(dropped);
        src = {-1, 0};
        dragSlot = -1;
        dragItem = {-1, 0};
    }

    // "Press 'I' to Close" di-center pake MeasureTextEx
    {
        Vector2 closeSz = MeasureTextEx(GetOrLoad(FontId::HUD_PLAYER), "Press 'I' to Close", 20, 0);
        DrawTextHUD("Press 'I' to Close", (int)(GameScreenWidth / 2.0f - closeSz.x / 2.0f), (int)(bgY + bgH + 15), 20, GRAY);
    }

    // keybind hints (Merge, Split, Arrange, Drop) di kanan atas pakai GetOrLoad(FontId::HUD_PLAYER)
    {
        const char *hints[] = {"[Left-Click Drag] Arrange", "[Ctrl+Click] Merge", "[Right-Click Drag] Split", "[Drop Outside Menu] Drop"};
        int hintCount = sizeof(hints) / sizeof(hints[0]);
        int hintFontSize = 25;
        float rightX = (float)GameScreenWidth - 20.0f;
        float hintY = 20.0f;
        float lineGap = 32.0f;

        for (int i = 0; i < hintCount; i++)
        {
            Vector2 hintSz = MeasureTextEx(GetOrLoad(FontId::HUD_PLAYER), hints[i], hintFontSize, 0);
            float hintX = rightX - hintSz.x;
            DrawRectangleRounded(
                (Rectangle){hintX - 4, hintY - 4, hintSz.x + 8, hintSz.y + 8},
                0.3f, 8, ColorAlpha(BLACK, 0.8f));
            DrawTextEx(GetOrLoad(FontId::HUD_PLAYER), hints[i], Vector2{hintX, hintY}, hintFontSize, 0, WHITE);
            hintY += lineGap;
        }
    }

    // nama item yang sedang di-hover
    {
        int hoveredId = -1;
        for (int i = 0; i < PlayerInstance.GetMaxBag(); i++)
        {
            Rectangle slotRect = {gridX + (i % bagCols) * (slotSize + gap), gridY + (i / bagCols) * (slotSize + gap), slotSize, slotSize};
            if (CheckCollisionPointRec(mousePos, slotRect) && PlayerInstance.GetBagItem(i).definitionId != -1)
            {
                hoveredId = PlayerInstance.GetBagItem(i).definitionId;
                break;
            }
        }
        if (hoveredId == -1)
        {
            for (int i = 0; i < PlayerInstance.GetMaxHotbar(); i++)
            {
                Rectangle slotRect = {gridX + i * (slotSize + gap), gridY + 373.0f, slotSize, slotSize};
                if (CheckCollisionPointRec(mousePos, slotRect) && PlayerInstance.GetHotbarItem(i).definitionId != -1)
                {
                    hoveredId = PlayerInstance.GetHotbarItem(i).definitionId;
                    break;
                }
            }
        }
        if (hoveredId != -1)
        {
            const char *itemName = itemDefs.GetById(hoveredId).name.c_str();
            Vector2 nameSz = MeasureTextEx(GetOrLoad(FontId::HUD_PLAYER), itemName, 22, 0);
            float nameX = mousePos.x - nameSz.x / 2.0f;
            float nameY = mousePos.y - 40.0f;
            DrawRectangleRounded(
                (Rectangle){nameX - 6, nameY - 4, nameSz.x + 12, nameSz.y + 8},
                0.3f, 8, ColorAlpha(BLACK, 0.85f));
            DrawTextEx(GetOrLoad(FontId::HUD_PLAYER), itemName, Vector2{nameX, nameY}, 22, 0, WHITE);
        }
    }
}

/**
 * @brief Render stat bar (health/mana) dengan nilai numerik di sampingnya.
 * @param pos Posisi kiri atas bar.
 * @param width Lebar bar.
 * @param height Tinggi bar.
 * @param ratio Rasio fill bar (0.0 - 1.0).
 * @param color Warna fill bar.
 * @param current Nilai numerik yang ditampilkan.
 */
static void DrawStatBar(Vector2 pos, float width, float height, float ratio, Color color, int current)
{
    DrawRectangleRounded((Rectangle){pos.x + 2, pos.y + 2, width, height}, 0.4f, 8, ColorAlpha(BLACK, 0.3f));
    DrawRectangleRounded((Rectangle){pos.x, pos.y, width, height}, 0.4f, 8, DARKGRAY);

    if (ratio > 0)
    {
        DrawRectangleRounded((Rectangle){pos.x, pos.y, width * ratio, height}, 0.4f, 8, color);
        DrawRectangleRounded((Rectangle){pos.x, pos.y, width * ratio, height * 0.4f}, 0.4f, 8, ColorAlpha(WHITE, 0.1f));
    }

    DrawRectangleRoundedLines((Rectangle){pos.x, pos.y, width, height}, 0.4f, 8, ColorAlpha(WHITE, 0.2f));

    char buffer[32];
    int percent = (int)(ratio * 100.0f);
    sprintf(buffer, "%d%%", percent);
    int fontSize = 22;
    float textX = pos.x + width + 15.0f;
    float textY = pos.y + (height - (float)fontSize) / 2.0f;
    DrawTextEx(GetOrLoad(FontId::HUD_PLAYER), buffer, Vector2{textX, textY}, fontSize, 0, WHITE);
}

/**
 * @brief Render hotbar beserta logika drag & drop dan split stack saat inventory terbuka.
 * @note Drag & drop dan split hanya aktif saat inventory terbuka.
 */
void DrawHotbar()
{
    if (InputInstance.IsInventoryOpen())
        return;

    const float slotSize = 55.0f;
    const float padding = 8.0f;
    // 5 box layout (inv + 4 hotbar) centered
    const float total5W = (slotSize * 5) + (padding * 4);
    const float startX = ((float)GameScreenWidth - total5W) / 2.0f;
    const float startY = (float)GameScreenHeight - 44.0f - slotSize;

    int activeSlot = (int)InputInstance.GetActiveSlot();
    bool isInventoryOpen = InputInstance.IsInventoryOpen();

    Vector2 mousePos = GetVirtualMousePosition(gState);
    bool mousePressed = InputInstance.IsLeftClickPressed();
    bool mouseReleased = InputInstance.IsLeftClickReleased();

    int globalFontsize = 17;
    float globalPadding = 12.0f;

    // Inv box (bag icon) — first of 5 boxes
    {
        Rectangle r = {startX, startY, slotSize, slotSize};
        DrawRectangleRounded((Rectangle){r.x + 2, r.y + 2, r.width, r.height}, 0.2f, 8, ColorAlpha(BLACK, 0.4f));
        DrawRectangleRounded(r, 0.4f, 8, ColorAlpha(DARKGRAY, 0.6f));
        DrawRectangleRoundedLines(r, 0.4f, 8, ColorAlpha(WHITE, 0.3f));
        if (hudTexLoaded)
        {
            float iconDraw = 50.0f;
            float dx = r.x + (r.width - iconDraw) / 2.0f;
            float dy = r.y + (r.height - iconDraw) / 2.0f;
            DrawTexturePro(hudBagIcon, (Rectangle){0, 0, (float)hudBagIcon.width, (float)hudBagIcon.height},
                (Rectangle){dx, dy, iconDraw, iconDraw}, {0, 0}, 0, WHITE);
        }

        const char *invKey = keybindManager.GetKeyDisplayName(TOGGLE_INVENTORY);
        int ikFontSize = globalFontsize;
        Font ikFont = GetOrLoad(FontId::HUD_PLAYER);
        Vector2 ikSz = MeasureTextEx(ikFont, invKey, ikFontSize, 0);
        float ikX = r.x + (r.width - ikSz.x) / 2.0f;
        float ikY = r.y + r.height + globalPadding;
        DrawRectangleRounded(
            (Rectangle){ikX - 4, ikY - 2, ikSz.x + 8, ikSz.y + 4},
            0.3f, 8, ColorAlpha(BLACK, 0.8f));
        DrawTextEx(ikFont, invKey, Vector2{ikX, ikY}, ikFontSize, 0, WHITE);
    }

    for (int i = 0; i < PlayerInstance.GetMaxHotbar(); i++)
    {
        Rectangle slotRect = {startX + (i + 1) * (slotSize + padding), startY, slotSize, slotSize};
        bool isActive = (activeSlot == (int)(i + 1));
        int globalIdx = PlayerInstance.GetMaxBag() + i;
        bool isHovered = isInventoryOpen && CheckCollisionPointRec(mousePos, slotRect);
        bool isDragSource = (dragSlot == globalIdx);

        DrawRectangleRounded((Rectangle){slotRect.x + 2, slotRect.y + 2, slotRect.width, slotRect.height}, 0.2f, 8, ColorAlpha(BLACK, 0.4f));

        Color bgColor = isActive ? ColorAlpha(GOLD, 0.3f) : (isDragSource ? ColorAlpha(GOLD, 0.2f) : ColorAlpha(DARKGRAY, 0.6f));
        if (isHovered && !isDragSource)
            bgColor = ColorAlpha(GRAY, 0.7f);
        DrawRectangleRounded(slotRect, 0.4f, 8, bgColor);

        Color borderColor = (isActive || isDragSource) ? GOLD : ColorAlpha(WHITE, 0.3f);
        DrawRectangleRoundedLines(slotRect, 0.4f, 8, borderColor);

        InventoryItem item = PlayerInstance.GetHotbarItem(i);
        if (item.definitionId != -1 && !isDragSource)
        {
            float iconDrawSize = 44.0f;
            Rectangle dest = {
                slotRect.x + (slotRect.width - iconDrawSize) / 2.0f,
                slotRect.y + (slotRect.height - iconDrawSize) / 2.0f,
                iconDrawSize, iconDrawSize};
            DrawItemIcon(item, dest);

            // stack amount hotbar, cuma kalo >1 (non-stackable gak muncul)
            if (item.amount > 1)
            {
                char amtBuf[12];
                sprintf(amtBuf, "%d", item.amount);
                int fontSize = 20;
                Vector2 textSz = MeasureTextEx(GetOrLoad(FontId::HUD_PLAYER), amtBuf, fontSize, 0);
                float sx = slotRect.x + slotRect.width - textSz.x - 4;
                float sy = slotRect.y + slotRect.height - textSz.y - 2;
                DrawTextEx(GetOrLoad(FontId::HUD_PLAYER), amtBuf, Vector2{sx, sy}, fontSize, 0, WHITE);
            }

            const ItemDefinition &def = itemDefs.GetById(item.definitionId);
            if (def.category == ITEM_POTION || def.category == ITEM_POISON)
            {
                const PotionData &potion = std::get<PotionData>(def.data);
                PotionCategory cat = potion.potionCategory;
                if (PlayerInstance.PotionCategoryCooldowns[cat] > 0.0f && PlayerInstance.PotionCategoryCooldownMax[cat] > 0.0f)
                {
                    float ratio = PlayerInstance.PotionCategoryCooldowns[cat] / PlayerInstance.PotionCategoryCooldownMax[cat];
                    float startAngle = 270.0f;
                    float endAngle = 270.0f - (360.0f * ratio);
                    Vector2 center = {
                        slotRect.x + slotRect.width / 2.0f,
                        slotRect.y + slotRect.height / 2.0f};
                    DrawCircleSector(center, iconDrawSize / 2.0f + 4.0f, startAngle, endAngle, 36, ColorAlpha(BLACK, 0.65f));

                    // Teks sisa cooldown
                    char cdBuf[16];
                    float cd = PlayerInstance.PotionCategoryCooldowns[cat];
                    if (cd >= 1.0f)
                        snprintf(cdBuf, sizeof(cdBuf), "%d", (int)cd);
                    else
                        snprintf(cdBuf, sizeof(cdBuf), "%.1f", cd);
                    int cdFontSize = 22;
                    Vector2 cdSz = MeasureTextEx(GetOrLoad(FontId::HUD_PLAYER), cdBuf, cdFontSize, 0);
                    DrawTextEx(GetOrLoad(FontId::HUD_PLAYER), cdBuf, Vector2{slotRect.x + (slotRect.width - cdSz.x) / 2.0f, slotRect.y + (slotRect.height - cdSz.y) / 2.0f}, cdFontSize, 0, WHITE);
                }
            }
        }

        // Keybind number below each hotbar slot
        {
            const char *slotKey = keybindManager.GetKeyDisplayName((Action)(HOTBAR_SLOT_1 + i));
            int snFontSize = globalFontsize;
            Font font = GetOrLoad(FontId::HUD_PLAYER);
            Vector2 snSz = MeasureTextEx(font, slotKey, snFontSize, 0);
            float snX = slotRect.x + (slotRect.width - snSz.x) / 2.0f;
            float snY = slotRect.y + slotRect.height + globalPadding;
            DrawRectangleRounded(
                (Rectangle){snX - 4, snY - 2, snSz.x + 8, snSz.y + 4},
                0.3f, 8, ColorAlpha(BLACK, 0.8f));
            DrawTextEx(font, slotKey, Vector2{snX, snY}, snFontSize, 0, WHITE);
        }

        if (isInventoryOpen)
        {
            // Ctrl+klik = merge stack, klik biasa = mulai drag
            if (isHovered && mousePressed && dragSlot == -1 && !isDragSplit && item.definitionId != -1)
            {
                bool ctrlHeld = InputInstance.IsCtrlDown();
                if (ctrlHeld)
                    HandleMergeStack(globalIdx);
                else
                {
                    dragSlot = globalIdx;
                    dragItem = item;
                }
            }

            // Drop ke slot hotbar
            if (isHovered && mouseReleased && dragSlot != -1 && dragSlot != globalIdx && !isDragSplit)
                HandleDrop(globalIdx);

            HandleSplitDragSlot(globalIdx, slotRect, mousePos);
        }
    }
}

/*==============================================================================
 * DrawBuffIndicators — Menampilkan indikator buff aktif (progress bar + sprite)
 *==============================================================================*/

// Stacked progress bars di atas health bar
// Format per entry: [sprite 18px] ▓▓▓▓▓▓▓▓▓▓░░░  5s
static void DrawBuffIndicators()
{
    const float barsX = 30.0f;
    const float barHeight = 22.0f;
    const float padding = 30.0f;
    const float gap = 8.0f;
    const float dashBarHeight = 6.0f;

    float dashPosY = (float)GameScreenHeight - padding - dashBarHeight;
    float manaPosY = dashPosY - gap - barHeight;
    float healthPosY = manaPosY - gap - barHeight;

    struct
    {
        const char *spriteKey;
        float *timer;
        float *maxTimer;
        Color barColor;
    } buffs[] = {
        {"damagePotionMedium", &PlayerInstance.BuffDamageTimer, &PlayerInstance.BuffDamageTimerMax, Color{255, 106, 0, 255}},
        {"speedPotionMedium", &PlayerInstance.BuffSpeedTimer, &PlayerInstance.BuffSpeedTimerMax, Color{0, 255, 233, 255}},
        {"invincibilityPotionMedium", &PlayerInstance.InvincibilityTimer, &PlayerInstance.InvincibilityTimerMax, Color{142, 167, 178, 255}},
    };

    // Hitung jumlah buff aktif
    int activeCount = 0;
    for (int i = 0; i < 3; i++)
    {
        if (*buffs[i].timer > 0.0f)
            activeCount++;
    }
    if (activeCount == 0)
        return;

    // Ukuran entry
    const float buffBarWidth = 150.0f;
    const float buffBarHeight = 18.0f;
    const float buffGap = 4.0f;
    const float spriteSize = 30.0f;
    const float entryHeight = buffBarHeight + buffGap;
    const float totalHeight = activeCount * entryHeight - buffGap;

    float y = healthPosY - gap - totalHeight;

    for (int i = 0; i < 3; i++)
    {
        if (*buffs[i].timer <= 0.0f)
            continue;

        float x = barsX;

        // Sprite
        const Frame &frame = GetFrame(buffs[i].spriteKey);
        Rectangle src = {
            (float)(frame.positionX * (FRAME_SIZE + FRAME_GAP)),
            (float)(frame.positionY * (FRAME_SIZE + FRAME_GAP)),
            (float)(frame.width * FRAME_SIZE),
            (float)(frame.height * FRAME_SIZE)};
        int maxDim = (src.width > src.height) ? (int)src.width : (int)src.height;
        float scale = spriteSize / maxDim;
        float rw = src.width * scale;
        float rh = src.height * scale;
        Rectangle dest = {
            x + (spriteSize - rw) / 2.0f,
            y + (buffBarHeight - rh) / 2.0f, rw, rh};
        DrawTexturePro(textures[frame.texture], src, dest, {0, 0}, 0, WHITE);

        float barX = x + spriteSize + 6.0f;

        // Bar background
        DrawRectangleRounded((Rectangle){barX, y, buffBarWidth, buffBarHeight}, 0.5f, 8, DARKGRAY);

        // Bar fill
        float ratio = (*buffs[i].maxTimer > 0.0f) ? *buffs[i].timer / *buffs[i].maxTimer : 0.0f;
        if (ratio > 0.0f)
        {
            DrawRectangleRounded((Rectangle){barX, y, buffBarWidth * ratio, buffBarHeight}, 0.5f, 8, buffs[i].barColor);
            DrawRectangleRounded((Rectangle){barX, y, buffBarWidth * ratio, buffBarHeight * 0.4f}, 0.5f, 8, ColorAlpha(WHITE, 0.15f));
        }

        // Timer text di samping bar
        char cdBuf[16];
        if (*buffs[i].timer >= 1.0f)
            snprintf(cdBuf, sizeof(cdBuf), "%ds", (int)*buffs[i].timer);
        else
            snprintf(cdBuf, sizeof(cdBuf), "%.1fs", *buffs[i].timer);
        int fontSize = 22;
        Vector2 cdSz = MeasureTextEx(GetOrLoad(FontId::HUD_PLAYER), cdBuf, fontSize, 0);
        DrawTextEx(GetOrLoad(FontId::HUD_PLAYER), cdBuf, Vector2{barX + buffBarWidth + 8.0f, y + (buffBarHeight - cdSz.y) / 2.0f}, fontSize, 0, WHITE);

        y += entryHeight;
    }
}

/*==============================================================================
 * Extracted HUD legend helpers
 *==============================================================================*/

/**
 * @brief Render keycap Pause di pojok kiri atas.
 */
static void DrawPauseKeycap()
{
    const float bx = 15.0f, by = 15.0f;
    const float iconSize =55.0f;   // ukuran settingsIcon.png

    if (hudTexLoaded)
        DrawTexturePro(hudSettingsIcon, (Rectangle){0, 0, (float)hudSettingsIcon.width, (float)hudSettingsIcon.height},
            (Rectangle){bx, by, iconSize, iconSize}, {0, 0}, 0, WHITE);

    const int lfSize = 18;
    Font font = GetOrLoad(FontId::HUD_PLAYER);
    std::string t = "[Esc]";
    Vector2 sz = MeasureTextEx(font, t.c_str(), lfSize, 0);
    float iconCenterX = bx + iconSize / 2.0f;
    float tx = iconCenterX - sz.x / 2.0f;
    float ty = by + iconSize + 4.0f;
    DrawRectangleRounded(
        (Rectangle){tx - 4, ty - 4, sz.x + 8, sz.y + 8},
        0.3f, 8, ColorAlpha(BLACK, 0.8f));
    DrawTextEx(font, t.c_str(), Vector2{tx, ty}, lfSize, 0, WHITE);
}

/**
 * @brief Render keycap Interact di tengah layar, hanya jika bisa interaksi.
 */
static void DrawInteractKeycap()
{
    if (!PlayerInstance.canInteract)
        return;
    const int lfSize = 20;
    Font font = GetOrLoad(FontId::HUD_PLAYER);
    const char *k = keybindManager.GetKeyDisplayName(INTERACT);
    std::string t = std::string(k) + " Interaksi";
    Vector2 sz = MeasureTextEx(font, t.c_str(), lfSize, 0);


    // Posisi berdasarkan hitbox player, offsetY bisa disesuaikan
    const float interactOffsetY = -55.0f;
    Vector2 playerRef = {
        PlayerInstance.GetPosition().x + PlayerInstance.GetHitboxOffsetX() + PlayerInstance.GetHitboxWidth() / 2.0f,
        PlayerInstance.GetPosition().y + PlayerInstance.GetHitboxOffsetY()
    };
    Vector2 screenPos = GetWorldToScreen2D(playerRef, camera);
    float bx = screenPos.x - sz.x / 2.0f;
    float by = screenPos.y + interactOffsetY;

    DrawRectangleRounded(
        (Rectangle){bx - 4, by - 4, sz.x + 8, sz.y + 8},
        0.3f, 8, ColorAlpha(BLACK, 0.8f));
    DrawTextEx(font, t.c_str(), Vector2{bx, by}, lfSize, 0, WHITE);
}

/**
 * @brief Render keycap Inv + Drop/DropAll di area bawah, align kiri hotbar.
 */
static void DrawInvDropKeycaps()
{
    const int lfSize = 18;
    Font font = GetOrLoad(FontId::HUD_PLAYER);

    const char *dropKey = keybindManager.GetKeyDisplayName(DROP_ITEM);
    const char *dropAllMod = keybindManager.GetKeyDisplayName(DROP_ALL);
    std::string dropT = std::string("[") + dropKey + "] Drop";
    std::string dropAllT = std::string("[") + dropAllMod + "+" + dropKey + "] All";
    Vector2 dropSz = MeasureTextEx(font, dropT.c_str(), lfSize, 0);
    Vector2 dropAllSz = MeasureTextEx(font, dropAllT.c_str(), lfSize, 0);

    const float dropRightPad = 12.0f;
    float rightEdge = (float)GameScreenWidth - dropRightPad;
    float baseY = (float)GameScreenHeight - 30.0f;
    float dropAllX = rightEdge - dropAllSz.x;
    float dropAllY = baseY - dropAllSz.y;
    float dropX = rightEdge - dropSz.x;
    float dropY = dropAllY - 4.0f - dropSz.y;

    DrawRectangleRounded(
        (Rectangle){dropX - 4, dropY - 4, dropSz.x + 8, dropSz.y + 8},
        0.3f, 8, ColorAlpha(BLACK, 0.8f));
    DrawTextEx(font, dropT.c_str(), Vector2{dropX, dropY}, lfSize, 0, WHITE);
    DrawRectangleRounded(
        (Rectangle){dropAllX - 4, dropAllY - 4, dropAllSz.x + 8, dropAllSz.y + 8},
        0.3f, 8, ColorAlpha(BLACK, 0.8f));
    DrawTextEx(font, dropAllT.c_str(), Vector2{dropAllX, dropAllY}, lfSize, 0, WHITE);
}

/**
 * @brief Render kill count di pojok kanan atas dari EnemyRegistry.
 */
static void DrawKillCount()
{
    if (hudTexLoaded)
    {
        const float iconSize = 50.0f;        // ukuran icon (resize di sini)
        const float offsetX = 470.0f;          // geser icon kiri (-)/kanan (+) dari tengah
        const float by = 15.0f;
        float ix = ((float)GameScreenWidth - iconSize) / 2.0f + offsetX;
        DrawTexturePro(hudKillCount, (Rectangle){0, 0, (float)hudKillCount.width, (float)hudKillCount.height},
            (Rectangle){ix, by, iconSize, iconSize}, {0, 0}, 0, WHITE);

        const int lfSize = 22;
        Font font = GetOrLoad(FontId::HUD_PLAYER);
        int totalEnemies = (int)Entities::EnemyRegistry.size();
        int deadEnemies = 0;
        for (auto *e : Entities::EnemyRegistry)
            if (!e->IsActive) deadEnemies++;
        char killBuf[16];
        snprintf(killBuf, sizeof(killBuf), "%d/%d", deadEnemies, totalEnemies);
        float tx = ix + iconSize + 6.0f;
        float ty = by + (iconSize - (float)lfSize) / 2.0f;
        DrawTextEx(font, killBuf, Vector2{tx, ty}, lfSize, 0, ColorAlpha(WHITE, 0.9f));
    }
}

/**
 * @brief Render stat bars (health, mana, dash cooldown) di kiri bawah.
 */
static void DrawStatBars()
{
    float health = PlayerInstance.GetHealth();
    float maxHealth = PlayerInstance.GetMaxHealth();
    float healthRatio = (maxHealth > 0) ? health / maxHealth : 0;

    float mana = PlayerInstance.GetMana();
    float maxMana = PlayerInstance.GetMaxMana();
    float manaRatio = (maxMana > 0) ? mana / maxMana : 0;

    const float barWidth = 220.0f;
    const float barHeight = 22.0f;
    const float padding = 30.0f;
    const float gap = 8.0f;

    float barsX = padding;
    const float dashBarHeight = 6.0f;

    Vector2 dashPos = {barsX, (float)GameScreenHeight - padding - dashBarHeight};
    Vector2 manaPos = {barsX, dashPos.y - gap - barHeight};
    Vector2 healthPos = {barsX, manaPos.y - gap - barHeight};

    DrawStatBar(healthPos, barWidth, barHeight, healthRatio, RED, (int)health);
    DrawStatBar(manaPos, barWidth, barHeight, manaRatio, GOLD, (int)mana);

    float dashCooldownRatio = 1.0f;
    if (PlayerInstance.DashCooldownMax > 0.0f)
    {
        float currentCd = PlayerInstance.DashCooldown < 0.0f ? 0.0f : PlayerInstance.DashCooldown;
        dashCooldownRatio = 1.0f - (currentCd / PlayerInstance.DashCooldownMax);
    }

    DrawRectangleRounded((Rectangle){dashPos.x, dashPos.y, barWidth, dashBarHeight}, 0.5f, 8, DARKGRAY);
    if (dashCooldownRatio > 0.0f)
        DrawRectangleRounded((Rectangle){dashPos.x, dashPos.y, barWidth * dashCooldownRatio, dashBarHeight}, 0.5f, 8, SKYBLUE);
}

/**
 * @brief Entry point render semua elemen HUD player.
 * Orchestrates: stat bars → buff indicators → hotbar → legend → inventory.
 */
void DrawPlayerHUD()
{
    int initialDragSlot = dragSlot;

    // 0. Load HUD textures once
    if (!hudTexLoaded)
    {
        Image img = LoadImage("assets/textures/hudPlayer/bagIcon.png");
        hudBagIcon = LoadTextureFromImage(img);
        UnloadImage(img);
        img = LoadImage("assets/textures/hudPlayer/settingsIcon.png");
        hudSettingsIcon = LoadTextureFromImage(img);
        UnloadImage(img);
        img = LoadImage("assets/textures/hudPlayer/killCount.png");
        hudKillCount = LoadTextureFromImage(img);
        UnloadImage(img);
        hudTexLoaded = true;
    }

    // 1. Stat bars
    DrawStatBars();

    // 2. Buff indicators
    DrawBuffIndicators();

    // 3. Hotbar + slot numbers
    DrawHotbar();

    // 4-7. Legend elements (only when inventory closed)
    if (!InputInstance.IsInventoryOpen())
    {
        DrawInvDropKeycaps();
        DrawInteractKeycap();
        DrawPauseKeycap();
        DrawKillCount();
    }

    // 8. Inventory overlay
    DrawInventory();
    if (InputInstance.IsInventoryOpen())
        DrawDragGhost(GetVirtualMousePosition(gState));

    // 9. Audio SFX for drag
    if (initialDragSlot == -1 && dragSlot != -1)
        AudioManager::PlaySFX("inventori");
    else if (initialDragSlot != -1 && dragSlot == -1)
        AudioManager::PlaySFX("inventori");
}

/**
 * @brief Render dialog sign overlay
 *
 * Tampilkan screen dim, dialog box di bawah, teks baris, dan hint dismiss.
 * Hanya render kalo ada dialog aktif.
 */
void DrawSignDialog()
{
    if (!signManager.IsDialogActive())
        return;

    DrawRectangle(0, 0, GameScreenWidth, GameScreenHeight, ColorAlpha(BLACK, 0.4f));

    Rectangle box = {
        GameScreenWidth * 0.1f,
        GameScreenHeight * 0.6f,
        GameScreenWidth * 0.8f,
        GameScreenHeight * 0.3f};
    DrawRectangleRounded(box, 0.15f, 8, ColorAlpha(DARKGRAY, 0.95f));
    DrawRectangleRoundedLines(box, 0.15f, 8, WHITE);

    const auto &lines = signManager.GetActiveDialogLines();
    float lineY = box.y + 16;
    for (const auto &line : lines)
    {
        DrawDefaultText(line.c_str(), (int)box.x + 16, (int)lineY, 16, WHITE);
        lineY += 22;
    }

    DrawDefaultText("[Klik kiri] untuk tutup", (int)box.x + (int)box.width - 140, (int)box.y + (int)box.height - 20, 10, GRAY);
}

/*==============================================================================
 * Boss Music (Ambient)
 *==============================================================================*/

void UpdateBossMusic()
{
    static bool s_BossMusicActive = false;

    // Player mati — clear flag, biar AudioManager::Update() handle auto-switch screen
    if (PlayerInstance.Anim.isDead)
    {
        s_BossMusicActive = false;
        return;
    }

    // Clear block kalo combat udah selesai — biar auto-switch normal kembali
    if (!TurnCombat::IsActive())
        AudioManager::UnblockAutoSwitch();

    auto &enemyReg = Entities::GetEnemyRegistry();
    Enemy *boss = nullptr;
    for (auto *enemy : enemyReg)
    {
        if (enemy->IsActive && enemy->rank == ENEMY_BOSS && enemy->Health > 0)
        {
            boss = enemy;
            break;
        }
    }

    if (!boss)
    {
        if (s_BossMusicActive)
        {
            // Jangan reset kalo lagi VICTORY phase — biar WinTheme jalan
            bool inVictory = TurnCombat::IsActive() && TurnCombat::GetPhase() == TurnPhase::VICTORY;
            if (inVictory)
            {
                AudioManager::BlockAutoSwitch();
            }
            else
            {
                AudioManager::ResetToScreenTrack();
            }
            s_BossMusicActive = false;
        }
        return;
    }

    bool inRange = Vector2Distance(boss->GetCenter(), PlayerInstance.GetCenter()) <= boss->DetectionRange;
    // Cek overlap dengan Tiled object boss_music — trigger area boss
    bool inBossArea = false;
    auto musicAreas = TiledHelper::GetObjectsByType("boss_music");
    for (auto *area : musicAreas)
    {
        if (CheckCollisionRecs(PlayerInstance.GetHitbox(), area->bounds))
        {
            inBossArea = true;
            break;
        }
    }
    bool inCombat = TurnCombat::IsActive();
    bool shouldPlay = inRange || inBossArea || inCombat;

    if (shouldPlay && !s_BossMusicActive)
    {
        AudioManager::PlayTrack("Boss");
        s_BossMusicActive = true;
    }
    else if (!shouldPlay && s_BossMusicActive)
    {
        AudioManager::ResetToScreenTrack();
        s_BossMusicActive = false;
    }
}

/*==============================================================================
 * Boss HP Bar
 *==============================================================================*/

/**
 * @brief Render boss HP bar di tengah bawah layar.
 * Cari enemy aktif dengan rank ENEMY_BOSS, tampilkan nama + bar + HP%.
 */
void DrawBossHPBar()
{
    auto &enemyReg = Entities::GetEnemyRegistry();
    Enemy *boss = nullptr;
    for (auto *enemy : enemyReg)
    {
        if (enemy->IsActive && enemy->rank == ENEMY_BOSS && enemy->Health > 0)
        {
            boss = enemy;
            break;
        }
    }
    if (!boss)
        return;

    // Kalo turn combat lagi aktif, jangan ditimpa — UI combat sendiri yang handle
    if (TurnCombat::IsActive())
        return;

    // Trigger: nongol kalo player dalam detection range boss ATAU overlap area boss_music
    bool inRange = Vector2Distance(boss->GetCenter(), PlayerInstance.GetCenter()) <= boss->DetectionRange;
    bool inBossArea = false;
    auto bossAreas = TiledHelper::GetObjectsByType("boss_music");
    for (auto *area : bossAreas)
    {
        if (CheckCollisionRecs(PlayerInstance.GetHitbox(), area->bounds))
        {
            inBossArea = true;
            break;
        }
    }
    if (!inRange && !inBossArea)
        return;

    const float barWidth = 400.0f;
    const float barHeight = 18.0f;
    const float centerX = (float)GameScreenWidth / 2.0f;
    const float barY = 75.0f;

    float maxHealth = boss->MaxHealth;
    float ratio = (maxHealth > 0) ? boss->Health / maxHealth : 0;

    // Nama boss di atas bar
    const char *bossName = boss->Name.c_str();
    int nameFontSize = 28;
    Vector2 nameSz = MeasureTextEx(GetOrLoad(FontId::HUD_PLAYER), bossName, nameFontSize, 0);
    DrawTextEx(GetOrLoad(FontId::HUD_PLAYER), bossName,
               Vector2{centerX - nameSz.x / 2.0f, barY - nameSz.y - 6.0f},
               nameFontSize, 0, WHITE);

    // Background bar
    Rectangle barBg = {centerX - barWidth / 2.0f, barY, barWidth, barHeight};
    DrawRectangleRounded(barBg, 0.4f, 8, DARKGRAY);

    // Fill bar (dark red)
    if (ratio > 0)
    {
        Rectangle barFill = {centerX - barWidth / 2.0f, barY, barWidth * ratio, barHeight};
        DrawRectangleRounded(barFill, 0.4f, 8, (Color){125, 15, 15, 255});
        DrawRectangleRounded((Rectangle){centerX - barWidth / 2.0f, barY, barWidth * ratio, barHeight * 0.4f},
                             0.4f, 8, ColorAlpha(WHITE, 0.1f));
    }
    DrawRectangleRoundedLines(barBg, 0.4f, 8, ColorAlpha(WHITE, 0.2f));

    // HP% counter di kanan bar
    char hpBuf[16];
    int percent = (int)(ratio * 100.0f);
    snprintf(hpBuf, sizeof(hpBuf), "%d%%", percent);
    int hpFontSize = 24;
    Vector2 hpSz = MeasureTextEx(GetOrLoad(FontId::HUD_PLAYER), hpBuf, hpFontSize, 0);
    DrawTextEx(GetOrLoad(FontId::HUD_PLAYER), hpBuf,
               Vector2{centerX + barWidth / 2.0f + 12.0f, barY + (barHeight - (float)hpFontSize) / 2.0f},
               hpFontSize, 0, WHITE);
}
