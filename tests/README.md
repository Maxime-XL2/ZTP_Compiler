# Tests

## FBX vertex animation

The FBX importer samples the longest-duration animation stack at the source
frame rate, deriving the frame count from the clip duration. Mesh topology must
remain fixed across those samples.
If an FBX has blend shapes but no animation duration, the importer generates a
looping sequence with one frame per channel's target shape, applying one shape
at full weight per frame. This automatic sequence is currently supported for
unskinned meshes; authored FBX animations continue to be sampled as before.
Triangles and quads are preserved; larger polygons are triangulated. UVs are
retained on face corners, and diffuse material colors and external PNG/TGA
textures are passed through the existing texture pipeline. Embedded textures
and advanced FBX shader features are not imported.

Skinned FBX imports include an opt-in `1-bone preview` control. Each vertex is
previewed using its strongest skin-cluster influence, so joint transitions are
rigid and may differ from the fully weighted sampled animation. This only changes
the in-memory viewport pose; exported ZTP keyframes remain the sampled animation.
Start/End values outside the loaded frame range are clamped to its nearest
endpoint when applied.

Animation sidecars support per-range frame deltas in 60 Hz ticks (`delta 4` =
15 fps, `delta 8` = 7.5 fps). The experimental resampling checkbox starts
unchecked for the source cadence; selecting a delta different from the stored
source delta checks it automatically. It can be unchecked to keep the original
keyframe cadence. When enabled, preview simulates the chosen sampling cadence
and ZTP export resamples only the selected ranges, concatenates them without
exporting frames outside those ranges, then writes their remapped indices and
deltas to the companion `.animations.txt` file. Higher-rate choices
generate interpolated positions; lower-rate choices always reduce the stored
keyframes to the chosen cadence, regardless of interpolation error. The range
endpoints are retained, so counts scale approximately with the cadence ratio
(with a small endpoint-rounding difference). Experimental export rejects
overlapping ranges. For an already loaded ZTP, reduced export requires Save as
to a different path so the source remains untouched. The ZTP binary still has
only a global framerate field: a Saturn reader must load the companion sidecar
to use per-animation ranges and deltas.

File > Export skeleton (C) exports a selected FBX skeleton to a separate,
standalone C source file; it does not change ZTP. The generated array contains
unique skin bones plus any ancestors needed to preserve the hierarchy. Parent
indices use `uint16_t` (`0xFFFF` means root). Rest and animation transforms are
stored in tightly packed channels. Each animation pose is a 6-byte record with
three signed `int8_t` Euler angles and three signed `int8_t` integer positions.
Angles use 256 steps per turn in XYZ order. Positions are rounded to whole
model units; if needed, one global Q16.16 translation-unit scale is selected
so they fit the signed 8-bit range. The generated C defines this decoder scale,
and the completion dialog reports it. Scale is stored once per node in signed
Q2.6 `int8_t` values; export rejects scale animation that differs from the
rest scale, along with shear and invalid transforms. Hierarchy metadata is an
8-byte record with 4-byte alignment; byte channels require no per-sample
padding. The export currently does not include per-point bone assignments.
The completion dialog reports the bone count, hierarchy node count, animation
frame count, selected translation scale and static data footprint (hierarchy
records, rest channels, animation poses and UTF-8 name bytes), not the
generated source-text size.

Run the focused importer test from the workspace root in PowerShell:

```powershell
& 'C:\Program Files\CodeBlocks\MinGW\bin\gcc.exe' -c third_party/ufbx/ufbx.c -o obj/Debug/ufbx_tests.o
if ($LASTEXITCODE -eq 0) { & 'C:\Program Files\CodeBlocks\MinGW\bin\g++.exe' -std=gnu++14 -Wall -static -ffunction-sections -fdata-sections tests/fbx_tests.cpp SRC/ZTP/fbx.cpp SRC/ZTP/lod.cpp third_party/meshoptimizer/simplifier.cpp obj/Debug/ufbx_tests.o '-Wl,--gc-sections' -o obj/Debug/fbx_tests.exe }
if ($LASTEXITCODE -eq 0) { & .\obj\Debug\fbx_tests.exe .\tests\fixtures\animated_triangle.fbx --expect-motion }
```

The fixture checks triangulated indices and verifies that sampled animation
changes vertex positions. To check the dominant-bone preview against a skinned
FBX, pass its path and `--preview-bones`; the test checks motion, interpolation,
reduced-base remapping, restoration and unchanged export keyframes.
To verify skeleton export against a skinned FBX, pass
`--export-skeleton "obj\\Debug\\test_skeleton.c"`; the test checks the generated
C source shape, animation frame count and reported static-data footprint.

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
OBJ coordinate fitting also checks animated points across all meshes and
upscales sub-126.5-unit geometry while keeping the model's scale factor inverse.

## ZTP loading

The ZTP reader supports identified `ZTP_Z01` / `ZTP_S01` containers and legacy
unidentified non-SGL/SGL payloads, including textures, animations and multi-LOD
directories. Every export/save writes the modern identified format.
Exact offsets and engine migration rules are in [ZTP_FORMAT.md](ZTP_FORMAT.md).
Loaded models expose File > Save and Save as. For ZTP inputs, Save writes to
the loaded file; Save as chooses a destination and makes it the next Save target.
For FBX/OBJ inputs, Save exports a ZTP to `OUT/` by default, while Save as picks
the output path; subsequent saves and the existing exit export reuse that path.
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
indices are retained for the base mesh and every animation frame. Unused source
textures are omitted from the saved file, while the current base and all LODs
are retained.
OBJ imports retain their existing `OUT/` export directory.

The Skins menu accepts multiple complete TGA/PNG atlases for OBJ/FBX models
loaded with UVs. Each atlas is cut for every polygon using the same UV-driven
texture-generation pipeline as the base model. Preview switches between the
base model and each complete skin; export writes a separate ZTP model without
animation frames, retaining matching mesh/point indexes for external animation
linking. Skins are not available when loading ZTP directly because its faces do
not retain source UV coordinates. Atlas variants are regenerated after
geometry/topology edits.

Run the focused reader/writer tests from the workspace root in PowerShell:

```powershell
& 'C:\Program Files\CodeBlocks\MinGW\bin\g++.exe' -std=gnu++14 -Wall -static -ffunction-sections -fdata-sections tests/ztp_tests.cpp SRC/ZTP/lodepng.cpp SRC/ZTP/load.cpp SRC/ZTP/writeZTP.cpp SRC/ZTP/tools.cpp SRC/ZTE/MATH/matrix.cpp SRC/ZTP/lod.cpp third_party/meshoptimizer/simplifier.cpp '-Wl,--gc-sections' -o obj/Debug/ztp_tests.exe
if ($LASTEXITCODE -eq 0) { & .\obj\Debug\ztp_tests.exe }
```

The tests cover both texture depths, polygon attributes and indices, keyframe
positions, and truncated input. Round-trip tests verify the 32-byte migration overhead,
textures, geometry, bounds, scale, frames and per-polygon attributes for static,
animated and multi-mesh models, even if the global SGL mode initially equals 1.
Save tests also check replacement, reloading and failure on a read-only target
without damaging the existing contents.
The texture-rebake test creates `obj/Debug/lod_texture_diagnostic.png`: the
source atlas is shown beside the reduced mesh rasterized from its transferred
UVs. The panels are compared pixel by pixel to catch UV drift.
Same-material UV islands are also tested for seam isolation and point reduction.
An overlapping folded surface checks that retained vertices keep their exact
source UVs instead of taking coordinates from a geometrically nearby face.
Passing a ZTP path to the test executable also checks that real model's
round-trip in an isolated temporary directory without writing its source.
Passing `--obj "C:\path\model.obj"` instead checks LOD generation and base
replacement at 18%, 37% and 75% reduction: point reduction, the non-SGL point
limit, and no new open boundary edges. The source OBJ is not modified.

Real texture export coverage (including quad rebaking) uses a separate test:

```powershell
& 'C:\Program Files\CodeBlocks\MinGW\bin\gcc.exe' -c third_party/ufbx/ufbx.c -o obj/Debug/ufbx_tests.o
if ($LASTEXITCODE -eq 0) { & 'C:\Program Files\CodeBlocks\MinGW\bin\g++.exe' -std=gnu++14 -static -ffunction-sections -fdata-sections tests/texture_export_tests.cpp SRC/ZTP/fbx.cpp SRC/ZTP/textures.cpp SRC/ZTP/lodepng.cpp SRC/ZTP/load.cpp SRC/ZTP/writeZTP.cpp SRC/ZTP/tools.cpp SRC/ZTP/vector.cpp SRC/ZTE/MATH/matrix.cpp SRC/ZTP/lod.cpp third_party/meshoptimizer/simplifier.cpp obj/Debug/ufbx_tests.o '-Wl,--gc-sections' -o obj/Debug/texture_export_tests.exe }
if ($LASTEXITCODE -eq 0) { & .\obj\Debug\texture_export_tests.exe 'C:\path\model.obj' }
```

Append `--compressed` to enable 4-bit texture compression. When enabled, every
exported texture must use a 16-color palette, including textures that initially
contain more than 16 colors. Both export and preserve-save paths must reload
with no unused texture entries and the expected texture byte count. Quad
rebaking can still increase used texture bytes because generated texture
dimensions are not proportional to face count.
The integration test also converts Titan to quads in non-SGL mode, replaces
the base at 1%, checks the 256-point limit and validates reloaded face indices.

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
The Generate LOD menu can also replace the base mesh using the same reduction
slider. The reduced base retains its texture/material references and animation
frames and is the geometry written on Save. The original geometry, animation
frames and LOD levels remain in memory until Restore original base is selected;
restoring also reinstates the original LOD levels.

Convert tris to quads globally matches compatible adjacent triangles on the
current base mesh. It preserves point positions and animation frames, and does
not run mesh simplification or reduce geometric quality. Existing material,
surface-attribute, geometry and UV seam checks still apply. The matching
maximizes the number of quads and orders shared edges by length as a tie-break
heuristic when selecting among equally large matchings.
Triangle normals may differ by up to 45 degrees; pairs need not be coplanar.
Convexity is checked with cross products at two opposite corners.

The generator uses meshoptimizer 1.3 to simplify triangulated source geometry.
The target is relative to the original polygon count, not the temporary triangle
count, so replacing a quad with two triangles does not count as a reduction.
After simplification, compatible adjacent triangles are merged into quads for
every generated LOD level; pairs that fail the existing geometry, material,
attribute or UV continuity checks remain triangles.
The target is at least four triangles per submesh, unless the source is smaller.
A zero-percent target preserves the original geometry. If simplification does
not reduce the triangulated index count, the original faces are kept for that submesh.
Textured simplification runs separately per connected UV island and material.
UV transfer stays within that island, and quad merging rejects discontinuous UV edges.
Island boundary vertices remain fixed so independently simplified islands cannot
separate at their seams. Faces incident to non-manifold edges are protected
before partitioning into islands; these protections can limit the achieved reduction.

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
& 'C:\Program Files\CodeBlocks\MinGW\bin\g++.exe' -std=gnu++14 -Wall -static tests/mtl_tests.cpp SRC/ZTP/textures.cpp SRC/ZTP/lodepng.cpp SRC/ZTP/lod.cpp third_party/meshoptimizer/simplifier.cpp -o obj/Debug/mtl_tests.exe
if ($LASTEXITCODE -eq 0) { & .\obj\Debug\mtl_tests.exe }
```

Tests cover material names, relative/absolute paths, comments, map options,
missing libraries, stream rewind, duplicate definitions, actual TGA/PNG
loading in both UV modes, PNG alpha, legacy naming attributes, missing and
unsupported diffuse maps, and the final placeholder. Image fixtures are
created in the system temporary directory and removed after a successful run.
The reduced-base integration test imports a two-color striped atlas with a
same-material UV seam, reduces the mesh, and checks every rebaked texture pixel
for island leakage and detail placement, allowing one atlas texel for sampling roundoff.

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