# Tests

## Wavefront OBJ loading

The OBJ loader supports vertices declared before groups, global and relative
position/texture/normal indices, optional texture coordinates, and concave
polygon faces. Faces larger than a quad and concave quads are triangulated for
the converter's four-corner polygon format.

Run the focused loader tests from the workspace root in PowerShell:

```powershell
& 'C:\Program Files\CodeBlocks\MinGW\bin\g++.exe' -std=gnu++14 -Wall -static -ffunction-sections -fdata-sections tests/obj_tests.cpp SRC/ZTP/load.cpp '-Wl,--gc-sections' -o obj/Debug/obj_tests.exe
if ($LASTEXITCODE -eq 0) { & .\obj\Debug\obj_tests.exe }
```

The tests load `Brute1.obj` and cover pre-group vertices, grouped global index
remapping for base meshes and keyframes, relative face indices, omitted texture
coordinates, normals, concave pentagons and quads, and rejection of index zero.

## ZTP loading

The ZTP reader supports identified `ZTP_Z01` / `ZTP_S01` containers and legacy
unidentified non-SGL/SGL payloads, including textures, animations and multi-LOD
directories. Every export/save writes the modern identified format.
Exact offsets and engine migration rules are in [ZTP_FORMAT.md](ZTP_FORMAT.md).
Working models opened as ZTP expose File > Save and Save as. Save writes to
the loaded file; Save as chooses a destination and makes it the next Save target.
Neither command adds a `_TEST` suffix. ZTP models are no longer exported
automatically on exit. Closing, replacing or reloading a ZTP prompts Yes/No/Cancel
until the first successful save, and again after surface edits or LOD generation.
No continues without saving; Cancel (or a failed/cancelled save) keeps the model open.
Camera, selection, playback, reference and LOD preview selection do not dirty the file.
Saving uses the original base pose, not the current interpolated preview pose.
Writes use a temporary file in the destination directory and replace the target
only after successful writing. Files loaded as references cannot be overwritten.
The writer preserves the loaded Z/S mode, polygon colors and normals instead of
rebuilding them from shared textures or animated geometry. Compressed normal
indices are retained for the base mesh and every animation frame.
OBJ imports retain their existing `OUT/` export directory.

Run the focused reader/writer tests from the workspace root in PowerShell:

```powershell
& 'C:\Program Files\CodeBlocks\MinGW\bin\g++.exe' -std=gnu++14 -Wall -static -ffunction-sections -fdata-sections tests/ztp_tests.cpp SRC/ZTP/load.cpp SRC/ZTP/writeZTP.cpp SRC/ZTP/tools.cpp SRC/ZTE/MATH/matrix.cpp SRC/ZTP/lod.cpp third_party/meshoptimizer/simplifier.cpp '-Wl,--gc-sections' -o obj/Debug/ztp_tests.exe
if ($LASTEXITCODE -eq 0) { & .\obj\Debug\ztp_tests.exe }
```

The tests cover both texture depths, polygon attributes and indices, keyframe
positions, and truncated input. Round-trip tests verify the 32-byte migration overhead,
textures, geometry, bounds, scale, frames and per-polygon attributes for static,
animated and multi-mesh models, even if the global SGL mode initially equals 1.
Save tests also check replacement, reloading and failure on a read-only target
without damaging the existing contents.
Passing a ZTP path to the test executable also checks that real model's
round-trip in an isolated temporary directory without writing its source.

## Automatic LOD prototype

The native side panel provides a reduction target (0-99%), Generate LOD,
Show LOD, a level selector and Delete LOD. Generate LOD offers Add level or
replacement of an explicitly chosen existing level. A reduction already used
by another level cannot be duplicated. Levels are sorted from finer to coarser,
so changing a replacement's reduction can change its displayed level number.
Delete LOD removes the selected added level after confirmation; base level 0
cannot be deleted. Replacement and deletion mark loaded ZTP files as unsaved.
All levels are saved with the base model in one ZTP; loaded levels can be previewed.
Generation switches to the LOD preview; unchecking Show LOD restores
the original. Moving the slider applies to the next generation. The status
displays achieved face counts and the number of distinct referenced points.

The generator uses meshoptimizer 1.3 to simplify triangulated source geometry.
The target is relative to the original polygon count, not the temporary triangle
count, so replacing a quad with two triangles does not count as a reduction.
The target is at least four triangles per submesh, unless the source is smaller.
A zero-percent target preserves the original geometry. If simplification would
increase the face count, the original faces are kept for that submesh.

Generated triangles use three distinct original vertex indices; the fourth
polygon index repeats the third for compatibility with the existing structure.
Positions and keyframes are shared with the original and are never modified.
The reference geometry is keyframe zero when animated. Vertex color attributes
and motion sampled from up to nine frames influence the simplification metric.
All keyframes and the 25%, 50% and 75% points of consecutive transitions,
including the loop, are checked for finite, nonzero triangle area. If this fails,
the simplifier retries with a larger budget, falling back to original faces.

Each face is rendered with a solid, alpha-weighted mean texture color and flat
lighting. Transparent pixels do not contribute to the average. Missing textures
use the source face color when available, otherwise neutral gray. Generated
triangles inherit the color and surface attributes of the source face with the
nearest centroid in the reference pose. Texture animation uses its first image.

LOD faces are exported as solid RGB polygons in the modern format. Saving
reorders a copy of every mesh and animation frame into shared point prefixes,
without modifying the working model. They are not connected to collision or
self-shadowing tests, which still use the base geometry. Preview interpolation still
updates the full point array; the point count reports references, not a smaller
keyframe allocation or a reduced interpolation loop. The generator does not
guarantee volume preservation, a geometric error bound, absence of intersections
or inversions during animation, or validity at every possible interpolated pose.
Generation is synchronous and may take time on large assets.

Uses meshoptimizer. Copyright (c) 2016-2026, Arseny Kapoulkine.
Upstream and MIT license are documented in third_party/meshoptimizer.

Run the focused standalone tests from the workspace root in PowerShell:

```powershell
& 'C:\Program Files\CodeBlocks\MinGW\bin\g++.exe' -std=gnu++14 -Wall -static tests/lod_tests.cpp SRC/ZTP/lod.cpp third_party/meshoptimizer/simplifier.cpp -o obj/Debug/lod_tests.exe
if ($LASTEXITCODE -eq 0) { & .\obj\Debug\lod_tests.exe }
```

Tests cover static and animated face/point reduction, distinct triangle indices,
alpha-weighted texture colors, original data preservation, keyframe offsets
across submeshes, existing triangles, reset, model identity, and invalid input.
The test executable is separate from the GUI build.

## MTL loading

Current support, limitations and the staged material-feature integration plan
are documented in [MTL.md](MTL.md).

Run the focused tests from the workspace root in PowerShell:

```powershell
& 'C:\Program Files\CodeBlocks\MinGW\bin\g++.exe' -std=gnu++14 -Wall -static tests/mtl_tests.cpp SRC/ZTP/textures.cpp SRC/ZTP/lodepng.cpp -o obj/Debug/mtl_tests.exe
if ($LASTEXITCODE -eq 0) { & .\obj\Debug\mtl_tests.exe }
```

Tests cover material names, relative/absolute paths, comments, map options,
missing libraries, stream rewind, duplicate definitions, actual TGA/PNG
loading in both UV modes, PNG alpha, legacy naming attributes, missing and
unsupported diffuse maps, and the final placeholder. Image fixtures are
created in the system temporary directory and removed after a successful run.

Triangle texture deduplication tests cover cyclic rotations, mirrored winding,
existing flip flags, UV tolerance, material isolation, and triangle/quad
separation. Reuse requires matching all four UV corners and the repeated-vertex
pattern; triangles with the same three UVs but a different collapsed UV corner
receive separate textures. Failed matches leave the polygon unchanged.

## Projected texture preview

### Visual Reference

Load reference (or File > Load visual reference) loads a separate OBJ or ZTP
alongside the working model, at the same origin and with the same camera.
OBJ import options are independent; animated OBJ sequences use the existing
`name_000001.obj` naming convention. ZTP references support non-SGL and SGL.
Remove reference unloads it without changing the working model or its LOD.

The reference is not selectable, editable, exported, or written to disk.
Loading it preserves the working model's export name and import settings and
suppresses the OBJ loader's palette output. Reference textures are independent.
If the working export path matches a reference loaded during the session,
the export gains a `_working` suffix instead of overwriting that reference.
Its pose follows the working model's frame and interpolation setting: frames
past the reference's final frame hold its last pose, and frames past the working
model's range are not played. Playback, pause, speed and looping belong to the
working model; a static working model keeps an animated reference at frame zero.
Self-shadowing includes both geometries, including occlusion between them.
The reference remains visible with the working model's LOD preview; shadow tests
still use the original working geometry, not the LOD.

The normal textured preview projects each face's four corners first, then draws
a bilinear screen-space grid without perspective-correct texture interpolation.
Grid density follows the bilinear cross term rather than edge length. It targets
at most half a screen pixel of position error and 1/255 of vertex color error,
subject to the 64-subdivision-per-axis cap. An affine face with affine colors
needs only one cell, regardless of its screen size. Shared vertices are built
by interpolating row endpoints, then submitted in one indexed OpenGL draw;
vertex storage and the last subdivision pattern's indices are reused.
Texture flip flags and vertex preview colors are preserved. Wireframe mode
keeps the face outline; the solid-color automatic LOD preview and model export
are unchanged.

For a bilinear component with cross term A - B + C - D, the maximum triangle
interpolation error per cell is the cross term's magnitude divided by four
times the column count times the row count. Density uses screen-space distance
for positions and the largest channel difference for colors.

Triangles retain their repeated corner. Faces crossing the near plane or with
non-projectable corners use the original OpenGL quad path for clipping. The
preview is an approximation, not a pixel-exact VDP1 renderer: it retains the
OpenGL depth buffer, lighting, transparency, and shadow behavior.

Run the focused math tests from the workspace root in PowerShell:

```powershell
& 'C:\Program Files\CodeBlocks\MinGW\bin\g++.exe' -std=gnu++14 -Wall -static tests/render_tests.cpp SRC/ZTP/tools.cpp SRC/ZTP/light.cpp -o obj/Debug/render_tests.exe
if ($LASTEXITCODE -eq 0) { & .\obj\Debug\render_tests.exe }
```

Tests cover projection matrix conventions, interpolation after projection,
depth and color interpolation, all four texture flip directions, collapsed
triangle edges, adaptive grid limits, and rejection of invalid, behind-camera,
and near-clipped corners. Large affine faces and warped/colored faces verify
the reduced subdivision count and interpolation error targets.
Bounding-box regressions cover descending vertices
and replacement geometry, including a single vertex. Ground shadows use the
loaded model's global maximum Y for every submesh, including the LOD preview.
These tests do not require an OpenGL context.
They also cover clamped reference poses, interpolation, single-frame/static
models, and cross-model self-shadowing when reference geometry moves or is removed.