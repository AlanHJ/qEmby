# Anime4K v4.0.1 GLSL shaders (bundled)

- Source repository: https://github.com/bloc97/Anime4K (MIT license, see `LICENSE`)
- Version: v4.0.1 (GitHub release tag `v4.0.1`, release asset `Anime4K_v4.0.zip`)
- Download date: 2026-10-28
- Upstream documentation used to compose the presets: `GLSL_Instructions.md` at tag v4.0.1

The `.glsl` files in this directory are copied verbatim from the official release
package (no content modifications; line endings normalized to LF). This is the
**full v4.0.1 filter set** — all 39 `.glsl` shaders shipped in the release
package are bundled so every filter can be selected in qEmby.

## Files in this directory (39 GLSL shaders, full v4.0.1 set)

| File | Size (bytes) | Used by presets |
| --- | --- | --- |
| `Anime4K_AutoDownscalePre_x2.glsl` | 1560 | — |
| `Anime4K_AutoDownscalePre_x4.glsl` | 1568 | — |
| `Anime4K_Clamp_Highlights.glsl` | 2795 | Fast, Balanced, Quality |
| `Anime4K_Darken_Fast.glsl` | 6378 | — |
| `Anime4K_Darken_HQ.glsl` | 5981 | — |
| `Anime4K_Darken_VeryFast.glsl` | 6442 | — |
| `Anime4K_Deblur_DoG.glsl` | 4552 | — |
| `Anime4K_Deblur_Original.glsl` | 6409 | — |
| `Anime4K_Denoise_Bilateral_Mean.glsl` | 2782 | — |
| `Anime4K_Denoise_Bilateral_Median.glsl` | 4221 | — |
| `Anime4K_Denoise_Bilateral_Mode.glsl` | 3821 | — |
| `Anime4K_Restore_CNN_S.glsl` | 17136 | Fast |
| `Anime4K_Restore_CNN_M.glsl` | 35916 | Balanced |
| `Anime4K_Restore_CNN_L.glsl` | 69921 | Quality |
| `Anime4K_Restore_CNN_VL.glsl` | 144075 | — |
| `Anime4K_Restore_CNN_UL.glsl` | 308660 | — |
| `Anime4K_Restore_CNN_Soft_S.glsl` | 17198 | — |
| `Anime4K_Restore_CNN_Soft_M.glsl` | 36016 | — |
| `Anime4K_Restore_CNN_Soft_L.glsl` | 70020 | — |
| `Anime4K_Restore_CNN_Soft_VL.glsl` | 144204 | — |
| `Anime4K_Restore_CNN_Soft_UL.glsl` | 308873 | — |
| `Anime4K_Thin_Fast.glsl` | 6248 | — |
| `Anime4K_Thin_HQ.glsl` | 5962 | — |
| `Anime4K_Thin_VeryFast.glsl` | 6335 | — |
| `Anime4K_Upscale_CNN_x2_S.glsl` | 18638 | Fast |
| `Anime4K_Upscale_CNN_x2_M.glsl` | 37685 | Balanced |
| `Anime4K_Upscale_CNN_x2_L.glsl` | 73443 | Quality |
| `Anime4K_Upscale_CNN_x2_VL.glsl` | 146743 | — |
| `Anime4K_Upscale_CNN_x2_UL.glsl` | 290257 | — |
| `Anime4K_Upscale_Deblur_DoG_x2.glsl` | 4830 | — |
| `Anime4K_Upscale_Deblur_Original_x2.glsl` | 6313 | — |
| `Anime4K_Upscale_Denoise_CNN_x2_S.glsl` | 18667 | — |
| `Anime4K_Upscale_Denoise_CNN_x2_M.glsl` | 37714 | — |
| `Anime4K_Upscale_Denoise_CNN_x2_L.glsl` | 73584 | — |
| `Anime4K_Upscale_Denoise_CNN_x2_VL.glsl` | 146811 | — |
| `Anime4K_Upscale_Denoise_CNN_x2_UL.glsl` | 290040 | — |
| `Anime4K_Upscale_DoG_x2.glsl` | 4016 | — |
| `Anime4K_Upscale_DTD_x2.glsl` | 15450 | — |
| `Anime4K_Upscale_Original_x2.glsl` | 6271 | — |
| `LICENSE` | 1063 | (license text) |
| `README.md` | — | (this file) |

Filters not used by the built-in presets ("—" above) are still bundled so users
can compose custom `glsl-shaders` chains from the complete upstream set.

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
    reserved by upstream for high-end GPUs and are bundled but not used by the
    presets (selectable manually in the custom chain UI).
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
