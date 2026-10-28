# Anime4K v4.0.1 GLSL shaders (bundled)

- Source repository: https://github.com/bloc97/Anime4K (MIT license, see `LICENSE`)
- Version: v4.0.1 (GitHub release tag `v4.0.1`, release asset `Anime4K_v4.0.zip`)
- Download date: 2026-10-28
- Upstream documentation used to compose the presets: `GLSL_Instructions.md` at tag v4.0.1

The `.glsl` files in this directory are copied verbatim from the official release
package (no content modifications). Only the files required by the qEmby quality
presets are included.

## Files in this directory

| File | Size (bytes) | Used by presets |
| --- | --- | --- |
| `Anime4K_Clamp_Highlights.glsl` | 2884 | Fast, Balanced, Quality |
| `Anime4K_Restore_CNN_S.glsl` | 17136 | Fast |
| `Anime4K_Restore_CNN_M.glsl` | 35916 | Balanced |
| `Anime4K_Restore_CNN_L.glsl` | 69921 | Quality |
| `Anime4K_Upscale_CNN_x2_S.glsl` | 18638 | Fast |
| `Anime4K_Upscale_CNN_x2_M.glsl` | 37685 | Balanced |
| `Anime4K_Upscale_CNN_x2_L.glsl` | 73443 | Quality |
| `LICENSE` | 1063 | (license text) |
| `README.md` | — | (this file) |

## Preset shader chains (mpv `glsl-shaders`, in load/processing order)

| Preset | Chain |
| --- | --- |
| Fast | `Anime4K_Clamp_Highlights.glsl` -> `Anime4K_Restore_CNN_S.glsl` -> `Anime4K_Upscale_CNN_x2_S.glsl` |
| Balanced | `Anime4K_Clamp_Highlights.glsl` -> `Anime4K_Restore_CNN_M.glsl` -> `Anime4K_Upscale_CNN_x2_M.glsl` |
| Quality | `Anime4K_Clamp_Highlights.glsl` -> `Anime4K_Restore_CNN_L.glsl` -> `Anime4K_Upscale_CNN_x2_L.glsl` |

## Rationale (per upstream documentation)

All three presets use the upstream **Mode A** pipeline, defined as
`Restore -> Upscale`, which upstream describes as optimized for "most 1080p
anime", "high amounts of blur" and "smearing due to compression" — the most
representative mode for general anime playback.

- `Clamp_Highlights` is always placed first, per upstream *Best Practices*:
  "It is recommended to always include `Clamp_Highlights` at the beginning to
  prevent ringing in some anime". It computes image statistics where it is
  placed in the chain, then clamps highlights at the end after all shaders to
  prevent overshoot and reduce ringing.
- `Restore` runs before `Upscale`, per upstream: "Restores image, best used
  before upscaling".
- CNN variant (S/M/L) selection, per upstream: "You are free to choose the CNN
  variant (S, M, L, VL, UL) for better speed or quality. Each step in size for
  CNN shaders doubles the processing time."
  - **Fast** — the lightweight `S` pair for weaker GPUs / headroom.
  - **Balanced** — the `M` pair, the same main `Restore + Upscale` variant used
    by upstream's lower-end ("Fast") recommended combinations.
  - **Quality** — the `L` pair, one step above `M`; `VL`/`UL` variants are
    reserved by upstream for high-end GPUs and are not bundled here.
- Each chain performs a single x2 upscale. Without `AutoDownscalePre_*`
  shaders, mpv's default behaviour is to downscale to screen size after running
  all shaders, so no work above screen resolution is done.

## Notes

- Upstream constraint: each shader file may be used at most once per chain
  ("Using the same file two or more times causes buggy behaviour and loss of
  performance").
- These shaders target the mpv `glsl-shaders` option and need real file paths;
  qEmby embeds them as Qt resources and unpacks them to the app data directory
  at runtime (see `src/qEmbyApp/components/anime4kshadermanager.*`).
- On Unix-like systems the `glsl-shaders` path list separator is `:` (not `;`).
