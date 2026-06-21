# Plan: Minimap — Fixed Viewport + Camera Follow + Drag Pan

## Konsep

Mirip sistem viewport 3D: ada area tetap (viewRect) yang jadi "jendela" ke dalam grid.
Grid di-render di belakang viewport ini, dan panOffset menentukan area mana yang kelihatan.

```
┌─────────────────────────────┐
│  panelRect (bg)             │
│  ┌───────────────────────┐  │
│  │  viewRect (FIXED)     │  │
│  │  ┌───┬───┬───┐       │  │
│  │  │ G │ G │ G │ ...   │  │  ← grid texture (lebih besar dari viewRect)
│  │  ├───┼───┼───┤       │  │
│  │  │ G │ P │ G │       │  │  ← P = player
│  │  └───┴───┴───┘       │  │
│  └───────────────────────┘  │
└─────────────────────────────┘
```

- viewRect = **fixed size** (tidak tergantung ukuran map)
- Grid di-draw lewat `DrawTexturePro` dengan dst = viewRect + panOffset
- ScissorMode motong area di luar viewRect
- PanOffset otomatis follow player tiap frame (kecuali user lagi drag)
- Marker player di posisi yang sesuai dalam viewport

## Perubahan

### 1. Constants (`include/map/minimap.h`)

**Hapus:**
- `MINIMAP_PANEL_SCALE` (scale per grid pixel → pindah ke CalculateLayout dinamis)

**Tambah:**
- `MINIMAP_VIEWPORT_WIDTH` — lebar viewport tetap (200px)
- `MINIMAP_VIEWPORT_HEIGHT` — tinggi viewport tetap (180px)

**Simpan:**
- `MINIMAP_PANEL_PADDING` = 10
- `MINIMAP_TILE_TO_PX` = 1
- `MINIMAP_REVEAL_RADIUS` = 10
- `MINIMAP_FOG_EXPLORED_ALPHA` = 0.6f

### 2. Layout Calculation (`minimap.cpp::CalculateLayout`)

Ubah dari:

```cpp
// OLD: viewRect = contentW × contentH (tergantung ukuran map)
int contentW = gw * MINIMAP_PANEL_SCALE;
int contentH = gh * MINIMAP_PANEL_SCALE;
int panelW = contentW + MINIMAP_PANEL_PADDING * 2;
int panelH = contentH + MINIMAP_PANEL_PADDING * 2;
panelRect = center based on panelW/H;
viewRect = panelRect minus padding;
```

Menjadi:

```cpp
// NEW: viewRect = FIXED size
int panelW = MINIMAP_VIEWPORT_WIDTH + MINIMAP_PANEL_PADDING * 2;
int panelH = MINIMAP_VIEWPORT_HEIGHT + MINIMAP_PANEL_PADDING * 2;
panelRect = center based on panelW/H;
viewRect = {panelRect.x + padding, panelRect.y + padding,
            MINIMAP_VIEWPORT_WIDTH, MINIMAP_VIEWPORT_HEIGHT};

// Scale dinamis: seberapa besar grid pixel ditampilkan
// Ini dipake untuk DrawTexturePro dst size
// Hitung scale yang bikin grid muat dengan aspect ratio pas
if (gw > 0 && gh > 0)
{
    float scaleX = MINIMAP_VIEWPORT_WIDTH / (float)gw;
    float scaleY = MINIMAP_VIEWPORT_HEIGHT / (float)gh;
    minimapScale = fminf(scaleX, scaleY);  // fit dalam viewport
    // Tapi kalo grid kecil banget, clamp biar gak terlalu kecil
    minimapScale = fmaxf(minimapScale, 1.0f);
}
```

Wait, ini terlalu kompleks. Simplify: **PANEL_SCALE tetap ada sebagai konstanta** (1 tile = 3 pixel), viewRect tetap di scale*persis kayak sekarang, tapi viewRect gak boleh lebih dari batas maksimum.

Actually, konsep yang lebih sederhana:

- **MINIMAP_PANEL_SCALE** tetap (saat ini 15, bisa diturunin ke 4-5)
- **viewRect** = FIXED size (misal 200×150)
- Grid di-draw dengan dst = scaled size, ScissorMode motong ke viewRect
- Kalo grid > viewRect → pan mengikuti player
- Kalo grid <= viewRect → center di viewport

Ini lebih simple — minimum perubahan kode.

### 3. UpdateView (`minimap.cpp`)

Sekarang:

```cpp
scaledW = gw * scale;  // scaledW == viewRect.width (karena viewRect = content size)
if (scaledW > viewRect.width) → never true
```

Ubah jadi:

```cpp
scaledW = gw * scale;
scaledH = gh * scale;

if (scaledW > viewRect.width)
{
    // Grid lebih lebar dari viewport → follow player
    panOffset.x = (viewRect.width / 2.0f)
        - (playerGridX * scale) - (scale / 2.0f);
    panOffset.x = clamp(panOffset.x, viewRect.width - scaledW, 0.0f);
}
else
{
    // Grid lebih kecil dari viewport → center
    panOffset.x = (viewRect.width - scaledW) / 2.0f;
}
```

Dengan viewRect FIXED (misal 200px), scaledW untuk worldgen (164 * 3 = 492) > 200 → follow player aktif.
ScaledW untuk main_hub (30 * 3 = 90) < 200 → grid di-center.

### 4. Draw (gak banyak berubah)

```cpp
Rectangle dst = {viewRect.x + panOffset.x,
                 viewRect.y + panOffset.y,
                 scaledW, scaledH};
DrawTexturePro(gridTexture, srcGrid, dst, ..., WHITE);
DrawFogLayer();  // sama — pake panOffset juga
DrawPlayerMarker();
```

ScissorMode udah bener — motong area viewRect.

### 5. Player Marker

Biar proporsional sama viewport size:

```cpp
// Alih-alih hardcoded radius 3
float markerRadius = fmaxf(scale * 0.5f, 3.0f);  // minimal 3px
float glowRadius  = markerRadius * 1.5f;
```

Atau konstanta `MINIMAP_MARKER_RADIUS` = 5.

### 6. Flow Update (main.cpp)

Gak berubah — tetap:

```cpp
UpdateLogicAll();           // player gerak
MinimapSystem::Update();    // update view follow player
DrawRenderTexture();        // render
```

### 7. HandlePan — drag override

Kalo user lagi drag, jangan auto follow player. Flag `isDragging = true` pas mouse click di viewRect, reset pas release. Tapi kalo user toggle minimap off/on, drag state di-reset.

## Files Changed

| File | Perubahan |
|---|---|
| `include/map/minimap.h` | Tambah MINIMAP_VIEWPORT_WIDTH/HEIGHT constants |
| `src/map/minimap.cpp` | CalculateLayout → fixed viewRect, UpdateView → real follow logic, DrawPlayerMarker → scaled radius, fog+grid draw pake scaledW/H |

## Testing

1. main_hub (30×30) → grid muat di viewport, center, player dot visible
2. Worldgen map (164×164) → grid > viewport, kamera follow player
3. Drag → override auto follow, scroll area lain
4. Marker visible dan gak terlalu kecil

---

**ACC?**
