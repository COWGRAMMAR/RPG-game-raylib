#include "hud.h"
// include fonts.h untuk fontLoadingTitle
#include "fonts.h"
#include "player.h"
#include "animation.h"
#include "inventory.h"
#include "inv-bst-sort.h"
#include "effectQueue.h"
#include "propsbehavior.h"
#include "../lib/raylib/include/raymath.h"
#include <cstdio>
#include <vector>
#include <string>
#include <cmath>

extern const int GameScreenWidth;
extern const int GameScreenHeight;

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
        (float)(frame.height * FRAME_SIZE)
    };

    if (def.spriteKey == "sword2")
    {
        src.y += 25.0f;
        src.height -= 25.0f;
    }

    int maxDim = (src.width > src.height) ? src.width : src.height;
    float size = dest.width / maxDim;

    float renderWidth = src.width * size;
    float renderHeight = src.height * size;

    Vector2 position = {
        dest.x + (dest.width - renderWidth) / 2.0f,
        dest.y + (dest.height - renderHeight) / 2.0f
    };

    Rectangle drawDest = {
        position.x,
        position.y,
        renderWidth,
        renderHeight
    };

    DrawTexturePro(textures[frame.texture], src, drawDest, {0, 0}, 0.0f, WHITE);
}

/**
 * @brief Render teks dengan background rounded rectangle.
 * @param text Teks yang akan ditampilkan.
 * @param x, y Posisi teks.
 * @param fontSize Ukuran font.
 * @param color Warna teks.
 */
// DrawTextHUD pakai fontLoadingTitle dengan padding tetap 4px
static void DrawTextHUD(const char *text, int x, int y, int fontSize, Color color)
{
    Vector2 textSize = MeasureTextEx(fontLoadingTitle, text, fontSize, 0);
    float pad = 4.0f;
    DrawRectangleRounded(
        (Rectangle){(float)x - pad, (float)y - pad, textSize.x + pad * 2, (float)fontSize + pad * 2},
        0.3f, 8, ColorAlpha(BLACK, 0.8f));
    DrawTextEx(fontLoadingTitle, text, Vector2{(float)x, (float)y}, fontSize, 0, color);
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

        if (isHovered && !isDragSource) DrawRectangleRec(slotRect, ColorAlpha(WHITE, 0.15f));
        if (isDragSource) DrawRectangleRec(slotRect, ColorAlpha(GOLD, 0.25f));

        InventoryItem &item = PlayerInstance.GetBagItem(i);
        if (item.definitionId != -1 && !isDragSource)
        {
            float iconSize = 50.0f;
            Rectangle dest = {slotRect.x + (slotSize - iconSize) / 2.0f, slotRect.y + (slotSize - iconSize) / 2.0f, iconSize, iconSize};
            DrawItemIcon(item, dest);
            // stack amount bag: fontLoadingTitle 18px dengan background rounded hitam
            if (item.amount > 1)
            {
                char buf[12]; sprintf(buf, "%d", item.amount);
                Vector2 sz = MeasureTextEx(fontLoadingTitle, buf, 18, 0);
                float bx = slotRect.x + slotSize - 34;
                float by = slotRect.y + slotSize - 24;
                DrawRectangleRounded((Rectangle){bx - 4, by - 4, sz.x + 8, 18 + 8}, 0.3f, 8, ColorAlpha(BLACK, 0.8f));
                DrawTextEx(fontLoadingTitle, buf, Vector2{bx, by}, 18, 0, WHITE);
            }
        }

        if (isHovered && mousePressed && dragSlot == -1 && !isDragSplit && item.definitionId != -1)
        {
            if (InputInstance.IsCtrlDown()) HandleMergeStack(i);
            else { dragSlot = i; dragItem = item; }
        }
        if (isHovered && mouseReleased && dragSlot != -1 && dragSlot != i && !isDragSplit)
        { HandleDrop(i); dragHandled = true; }
        if (isHovered && mouseReleased && dragSlot == i)
        { dragSlot = -1; dragItem = {-1, 0}; dragHandled = true; }
        HandleSplitDragSlot(i, slotRect, mousePos);
    }

    // === Hotbar (4 slots) ===
    for (int i = 0; i < PlayerInstance.GetMaxHotbar(); i++)
    {
        int globalIdx = PlayerInstance.GetMaxBag() + i;
        Rectangle slotRect = {gridX + i * (slotSize + gap), gridY + 373.0f, slotSize, slotSize};
        bool isHovered = CheckCollisionPointRec(mousePos, slotRect);
        bool isDragSource = (dragSlot == globalIdx);

        if (isHovered && !isDragSource) DrawRectangleRec(slotRect, ColorAlpha(WHITE, 0.15f));
        if (isDragSource) DrawRectangleRec(slotRect, ColorAlpha(GOLD, 0.25f));

        InventoryItem &item = PlayerInstance.GetHotbarItem(i);
        if (item.definitionId != -1 && !isDragSource)
        {
            float iconSize = 50.0f;
            Rectangle dest = {slotRect.x + (slotSize - iconSize) / 2.0f, slotRect.y + (slotSize - iconSize) / 2.0f, iconSize, iconSize};
            DrawItemIcon(item, dest);
            // stack amount hotbar (inventory open): fontLoadingTitle 18px
            if (item.amount > 1)
            {
                char buf[12]; sprintf(buf, "%d", item.amount);
                Vector2 sz = MeasureTextEx(fontLoadingTitle, buf, 18, 0);
                float bx = slotRect.x + slotSize - 34;
                float by = slotRect.y + slotSize - 24;
                DrawRectangleRounded((Rectangle){bx - 4, by - 4, sz.x + 8, 18 + 8}, 0.3f, 8, ColorAlpha(BLACK, 0.8f));
                DrawTextEx(fontLoadingTitle, buf, Vector2{bx, by}, 18, 0, WHITE);
            }
        }

        if (isHovered && mousePressed && dragSlot == -1 && !isDragSplit && item.definitionId != -1)
        {
            if (InputInstance.IsCtrlDown()) HandleMergeStack(globalIdx);
            else { dragSlot = globalIdx; dragItem = item; }
        }
        if (isHovered && mouseReleased && dragSlot != -1 && dragSlot != globalIdx && !isDragSplit)
        { HandleDrop(globalIdx); dragHandled = true; }
        if (isHovered && mouseReleased && dragSlot == globalIdx)
        { dragSlot = -1; dragItem = {-1, 0}; dragHandled = true; }
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
        case UP:    facingDir = {0, -1}; break;
        case DOWN:  facingDir = {0, 1};  break;
        case LEFT:  facingDir = {-1, 0}; break;
        case RIGHT: facingDir = {1, 0};  break;
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
        itemData.activeItems.push_back(dropped);
        src = {-1, 0};
        dragSlot = -1;
        dragItem = {-1, 0};
    }

    // "Press 'I' to Close" di-center pake MeasureTextEx
    {
        Vector2 closeSz = MeasureTextEx(fontLoadingTitle, "Press 'I' to Close", 20, 0);
        DrawTextHUD("Press 'I' to Close", (int)(GameScreenWidth / 2.0f - closeSz.x / 2.0f), (int)(bgY + bgH + 15), 20, GRAY);
    }

    // keybind hints (Merge, Split, Arrange, Drop) di kanan atas pakai fontLoadingTitle
    {
        const char* hints[] = {"[Left-Click Drag] Arrange", "[Ctrl+Click] Merge", "[Right-Click Drag] Split", "[Drop Outside Menu] Drop"};
        int hintCount = sizeof(hints) / sizeof(hints[0]);
        int hintFontSize = 25;
        float rightX = (float)GameScreenWidth - 20.0f;
        float hintY = 20.0f;
        float lineGap = 32.0f;

        for (int i = 0; i < hintCount; i++)
        {
            Vector2 hintSz = MeasureTextEx(fontLoadingTitle, hints[i], hintFontSize, 0);
            float hintX = rightX - hintSz.x;
            DrawRectangleRounded(
                (Rectangle){hintX - 4, hintY - 4, hintSz.x + 8, hintSz.y + 8},
                0.3f, 8, ColorAlpha(BLACK, 0.8f));
            DrawTextEx(fontLoadingTitle, hints[i], Vector2{hintX, hintY}, hintFontSize, 0, WHITE);
            hintY += lineGap;
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
    sprintf(buffer, "%d", current);
    int fontSize = 18;
    int textX = (int)(pos.x + width + 15);
    int textY = (int)(pos.y + (height - fontSize) / 2.0f);
    DrawTextHUD(buffer, textX, textY, fontSize, WHITE);
}

/**
 * @brief Render hotbar beserta logika drag & drop dan split stack saat inventory terbuka.
 * @note Drag & drop dan split hanya aktif saat inventory terbuka.
 */
void DrawHotbar()
{
    if (InputInstance.IsInventoryOpen()) return;

    const float slotSize = 55.0f;
    const float padding = 10.0f;
    const float screenPadding = 30.0f;
    const float totalWidth = (slotSize * 4) + (padding * 3);
    const float startX = (float)GameScreenWidth - screenPadding - totalWidth;
    const float startY = (float)GameScreenHeight - 30.0f - slotSize;

    int activeSlot = (int)InputInstance.GetActiveSlot();
    bool isInventoryOpen = InputInstance.IsInventoryOpen();

    Vector2 mousePos = GetVirtualMousePosition(gState);
    bool mousePressed = InputInstance.IsLeftClickPressed();
    bool mouseReleased = InputInstance.IsLeftClickReleased();

    for (int i = 0; i < PlayerInstance.GetMaxHotbar(); i++)
    {
        Rectangle slotRect = {startX + (i * (slotSize + padding)), startY, slotSize, slotSize};
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
            float iconDrawSize = 42.0f;
            Rectangle dest = {
                slotRect.x + (slotRect.width - iconDrawSize) / 2.0f,
                slotRect.y + (slotRect.height - iconDrawSize) / 2.0f,
                iconDrawSize, iconDrawSize};
            DrawItemIcon(item, dest);

            // stack amount hotbar (tertutup): fontLoadingTitle 16px
            if (item.amount > 0)
            {
                char amtBuf[12];
                sprintf(amtBuf, "%d", item.amount);
                int fontSize = 16;
                Vector2 textSz = MeasureTextEx(fontLoadingTitle, amtBuf, fontSize, 0);
                DrawTextHUD(amtBuf, (int)(slotRect.x + slotRect.width - textSz.x - 4), (int)(slotRect.y + slotRect.height - textSz.y - 2), fontSize, WHITE);
            }
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

/**
 * @brief Entry point render semua elemen HUD player (stat bar, hotbar, inventory).
 */
void DrawPlayerHUD()
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
    const float avatarSize = 80.0f;
    const float avatarPadding = 18.0f;

    Vector2 avatarPos = {padding + avatarSize / 2.0f, (float)GameScreenHeight - padding - avatarSize / 2.0f};
    float radius = avatarSize / 2.0f;

    DrawCircleV({avatarPos.x + 2, avatarPos.y + 2}, radius + 2, ColorAlpha(BLACK, 0.4f));
    DrawCircleV(avatarPos, radius, DARKGRAY);

    float spriteSize = avatarSize - 10.0f;
    Rectangle knightDest = {
        (avatarPos.x - spriteSize / 2.0f) + 1.0f,
        avatarPos.y - spriteSize / 2.0f,
        spriteSize, spriteSize};
    Frame avatarFrame = { SPRITESHEET_KNIGHT, 0, 2, 1, 1 };
    Display avatarDisplay;
    avatarDisplay.position = {knightDest.x, knightDest.y};
    avatarDisplay.size = (int)knightDest.width;
    DrawFrame(avatarFrame, avatarDisplay);

    DrawCircleLinesV(avatarPos, radius, ColorAlpha(GOLD, 0.6f));
    DrawCircleLinesV(avatarPos, radius + 1, ColorAlpha(GOLD, 0.3f));

    float barsX = padding + avatarSize + avatarPadding;
    Vector2 healthPos = {barsX, (float)GameScreenHeight - padding - (barHeight * 2) - gap};
    Vector2 manaPos = {barsX, (float)GameScreenHeight - padding - barHeight};

    DrawTextHUD(PlayerInstance.GetName(), (int)healthPos.x + 7, (int)healthPos.y - 35, 20, WHITE);
    DrawStatBar(healthPos, barWidth, barHeight, healthRatio, RED, (int)health);
    DrawStatBar(manaPos, barWidth, barHeight, manaRatio, GOLD, (int)mana);

    DrawHotbar();
    DrawInventory();
    if (InputInstance.IsInventoryOpen())
        DrawDragGhost(GetVirtualMousePosition(gState));
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
        GameScreenHeight * 0.3f
    };
    DrawRectangleRounded(box, 0.15f, 8, ColorAlpha(DARKGRAY, 0.95f));
    DrawRectangleRoundedLines(box, 0.15f, 8, WHITE);

    const auto &lines = signManager.GetActiveDialogLines();
    float lineY = box.y + 16;
    for (const auto &line : lines)
    {
        DrawText(line.c_str(), (int)box.x + 16, (int)lineY, 16, WHITE);
        lineY += 22;
    }

    DrawText("[Klik kiri] untuk tutup", (int)box.x + (int)box.width - 140, (int)box.y + (int)box.height - 20, 10, GRAY);
}
