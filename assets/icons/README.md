# USB Tree application icon

Created for this project on 2026-09-15 using the built-in imagegen tool. The master artwork has genuine transparency outside the dark tile. No fwcom application icon or UsbTreeView artwork was copied.

- `usbtree.png`: original generated artwork, 1254 × 1254 RGBA.
- `usbtree.ico`: Windows executable icon, 16, 20, 24, 32, 40, 48, 64, 128 and 256 pixels.
- `usbtree.icns`: macOS app-bundle icon, including Retina representations up to 1024 pixels.
- `usbtree.bmp`: 256-pixel BMP v4 with alpha, loaded by SDL for the window icon.

The build consumes the committed files without Python or an image library at runtime. To re-export the native formats after replacing the master, install Pillow and run `python scripts/export-icons.py` from the repository. This only resizes and converts the artwork; it does not alter the design.

## Generation prompt

Use case: logo-brand. Asset type: the final application icon for USB Tree, a native professional desktop utility that explores USB controllers, hubs, devices, COM ports and drives. Create ONE polished production app icon, square 1024x1024 PNG, centered front-facing orthographic composition. Design: a bold distinctive USB trident fused with a clean branching topology/tree diagram, three clear branches ending in a circle, square and upward arrow, connected into one coherent symbol. The symbol is vivid warm red/coral (#ef4549 to #ba252b), on a deep dark slate rounded-square tile (#202229). Subtle premium bevel and restrained edge lighting provide depth, but shapes remain clean, thick, spare, precise and legible at 16-32 pixels. Calm engineered typography-free identity matching a dark slate and red desktop app. Icon tile takes about 88% of the canvas with generous, even transparent outer margins; symbol comfortably fills tile with balanced negative space. Transparent background outside the rounded tile, actual alpha, no white background, no checkerboard, no surrounding scene. One icon only, no previews, no mockup grid, no text, no letters, no watermarks, no USB connector illustration, no fine circuit traces, no tiny decoration, no floating parts.
