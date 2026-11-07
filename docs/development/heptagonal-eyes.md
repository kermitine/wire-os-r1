# Heptagonal eyes

This build uses a seven-sided procedural eye outline by default: a flat top,
deeply clipped upper corners, vertical sides, and a pointed base. The eyes are
15% wider and 30% shorter than the original dimensions for a horizontal,
visor-like profile. At the default scale, each eye sits four pixels farther
toward its screen edge, giving eight pixels more separation. Eyelid geometry
and lighting follow the outline's dimensions and position.
Existing eyelids, blinks, eye movement, transforms, lighting, and color overlays
still use the normal rendering pipeline. Animated corner-radius values apply
only to the original rounded style.

The style applies to every procedural eye keyframe, including the eyes during
emotional reactions and idle animations. Eyelid motion and curvature, eye
position, rotation, scaling, and blink timing continue to animate. Expressions
that rely on changing corner radii instead retain the fixed seven-sided
outline, so their silhouettes differ from the original design. Individual
frames can show fewer visible sides when the eyelids cover part of an eye or
the eyes close.

Sprite and image artwork, such as full-screen icons or an SDK-provided face
image, is not reshaped. Animations that combine artwork with procedural eyes
show the new shape on their eye layer. Body motion and audio are unchanged.

Build and deploy the updated animation process using the repository's normal
build and deployment instructions. On that build, `Face.ParameterizedFace` has
a `ProcFace_EyeShape` console setting:

- `0`: original rounded outlines
- `1`: heptagonal outlines (default)

The console variable is declared as `kProcFace_EyeShape`; the remote console
exposes it as `ProcFace_EyeShape`. Changes redraw the eyes immediately, even
while the face is stationary.

To save a choice across reboots, use the existing `consoleVarsAnim.ini` file in
the robot's persistent directory and preserve its other settings:

```ini
[Face.ParameterizedFace]
ProcFace_EyeShape = 1
```

An older firmware build does not have this setting. Rocket-Pod's existing eye
overlay controls change the color texture; the new outline requires this
updated renderer.

## Keeping heterochromia colors separate

Custom overlays now keep their left and right color halves attached to the
corresponding procedural eye. Previously the overlay stretched across the
combined face bounds, so an exaggerated blink, uneven eye scaling, or crossed
eyes could move one eye into the other half of the color texture.

The renderer records which eye supplied each visible pixel before merging the
eyes. Where the shapes overlap, the brighter source pixel retains its eye's
color; ties keep the left eye, matching the existing draw order. Scanline
distortion moves these labels with the eye pixels. Color and alpha sampling
then stay within that eye's half of the overlay, including anti-aliased edges
and clipped or off-screen shapes. This works with both rounded and heptagonal
outlines and does not change the selected colors or overlay opacity.

`Face.ParameterizedFace` exposes `ProcFace_LockEyeOverlayColors`, enabled by
default. Leave it enabled for heterochromia overlays with one color in each
image half. Set it to false for artwork intended to span freely across both
eyes. The C++ console variable is `kProcFace_LockEyeOverlayColors`. To persist
the setting, preserve the other entries in `consoleVarsAnim.ini` and add:

```ini
[Face.ParameterizedFace]
ProcFace_LockEyeOverlayColors = true
```

Rebuild and deploy the animation process for this renderer change. On Vector,
check wide blinks, strong sideways looks, head tilts, and uneven eye scaling
with the existing heterochromia overlay enabled. An overlay with blended colors
already present inside either source-image half will retain those pixels.

`ProceduralFaceOverlayTest` in `test/animProcess/proceduralFaceTests.cpp` uses a
synthetic blue/red overlay and separately rendered eye images to check wide
blinks, unequal scaling, crossed eyes, and face rotation. Run it in the native
animation test build with `--gtest_filter=ProceduralFaceOverlayTest.*`.
