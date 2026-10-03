# Automatic LOD prototype

The native side panel provides a reduction target (0-99%), Generate LOD, and
Show LOD. Generation switches to the LOD preview; unchecking Show LOD restores
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

This remains a preview with projected ground shadows. The original export path
and binary layouts are unchanged; the LOD is not exported and is not connected
to collision or self-shadowing tests. Original animation interpolation still
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