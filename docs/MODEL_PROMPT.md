# Y'all-ARM — 3D Enclosure Design Prompt

Paste this into Zoo Design Studio (or any AI CAD tool) to generate the enclosure.

---

## Components to Fit

| Component             | Dimensions (L × W × H)             | Notes                          |
|-----------------------|-------------------------------------|--------------------------------|
| ESP32-S3 DevKitC-1    | 69 mm × 26 mm × 10 mm               | USB-C port on one short edge   |
| WS2812B LED strip     | 16 LEDs, 8 mm pitch → 128 mm total  | Flex strip; can be bent/folded |
| MAX98357A amp module  | 23 mm × 16 mm × 5 mm                | Nestled beside speaker         |
| Speaker               | 28 mm round, 5 mm deep              |                                |
| Tactile button        | 6 mm × 6 mm                         | Optional; top or side surface  |

---

## Front Face Layout

```
┌──────────────────────────────────────────┐
│                                          │
│   [O] [N]       [A] [I] [R]             │  ← 6 LED windows
│                                          │
│  ────────── divider ──────────────────   │
│                                          │
│  [▪] [▪] [▪] [▪] [▪] [▪] [▪] [▪] [▪] [▪]  │  ← 10 LED windows
│                                          │
└──────────────────────────────────────────┘
```

- **Top row**: 6 circular LED windows, 8 mm center-to-center, with a visible gap between the 2nd and 3rd window
- **Bottom row**: 10 circular LED windows in a single horizontal row, evenly spaced at 8 mm c/c
- **Divider**: ~6 mm recessed shelf separating the two rows
- **Apertures**: 5 mm diameter circles with 1 mm frosted diffuser lip
- **Total front face**: ~90 mm wide × 50 mm tall

---

## Enclosure Requirements

**Form factor:**
- Landscape orientation
- Minimize depth — target ≤ 22 mm total
- 5–8° forward tilt built into base so face angles up toward seated viewer
- Rubber-foot recesses at 4 bottom corners

**Internal cavity (snap-fit or friction-fit rear panel):**
- PCB mounts on 3 mm standoffs; USB-C cutout on one short edge
- LED strip runs horizontally behind front-face apertures; 3 mm air gap between LED faces and diffuser
- Speaker port on one side wall or front face bottom corner; vent holes (3 × 3 mm grid, ~40% open area)

**Construction:**
- FDM-printable; no overhangs > 45° without support
- Wall thickness ≥ 2 mm
- All exterior corners radiused at 2 mm
