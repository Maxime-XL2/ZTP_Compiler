# MTL Support And Integration Plan

## Implemented

OBJ `mtllib` declarations are scanned before loading the base mesh. Multiple
libraries are supported; quote library paths containing spaces. Paths are
relative to the OBJ. When no `mtllib` is declared, a same-stem `.mtl` file is
tried. A missing library is nonfatal. Later definitions of a material win.

`newmtl` names match `usemtl`, including names containing spaces. `map_Kd` paths
are relative to their MTL file, not the working directory. Absolute Windows,
UNC and root-relative paths are accepted. Quoted image paths, unquoted image
paths containing spaces, comments and common map options are parsed.

An explicit diffuse image can be PNG (decoded by the existing lodepng library)
or an uncompressed 24/32-bit TGA. Both use the existing palette conversion,
UV-dependent row orientation and binary alpha convention (alpha below 8 is
transparent). Material names stay unchanged, preserving `ANIM`, `2D`,
`DUAL_`, `C_WHITE` and material-based texture identity. Both normal loading and
per-face UV texture generation resolve the same source images.

Resolution order:

1. The material's `map_Kd`, when present and supported.
2. The existing `<input folder><material name>.TGA` lookup.
3. The existing placeholder texture if neither image can be loaded.

Unknown materials, missing MTL files, missing `map_Kd`, unsupported formats and
failed decoding use this fallback. Map options are consumed to locate the image
but do not yet transform UVs or change sampling. Unknown map options discard
the map rather than treating an option value as a filename. RLE TGA, JPEG,
line continuations and escaped quotes are not implemented. Material `None`
retains its existing no-texture meaning. MTL is not reread for animation frames.
The special `PALETTE.TGA` lookup and exported binary layouts are unchanged.

## Integration Order

Keep imported material metadata in a compiler-side table, separate from
`texture_t`, `polygon_t`, `ATTR` and serialized Saturn data. Retain a source
material association per face before UV texture generation rewrites its texture
index. This also lets multiple materials share an image but use different
surface settings. Track absent values separately from explicitly supplied zero.

1. **Diffuse color (`Kd`).** Generate a constant-color source for a material
   without `map_Kd`, including black; use the existing palette/single-color
   attribute path. Make tinting an existing image opt-in, since Blender's
   exported `Kd` defaults could darken existing assets. Bake before palette
   conversion and `createTexture`; let the LOD consume the resulting pixels.
2. **UV transforms (`-s`, `-o`, `-clamp`).** Store these with the map and apply
   scale then offset to each face's UVs before `rotatePolygon`/`createTexture`.
   Implement wrap/clamp in sampling, not by globally changing shared UVs.
   For non-UV loading, warn on transforms that cannot be represented by the
   existing flip/rotation surface attributes.
3. **Opacity (`d`, `Tr`, `map_d`).** Normalize `Tr` to `1 - Tr`, document
   precedence when both occur, and combine scalar opacity, diffuse alpha and
   opacity-map samples before palette generation. Offer explicit binary cutout
   and Saturn mesh approximations using existing surface attributes. Do not
   promise continuous alpha blending or silently change sorting/double-sidedness.
4. **Lighting (`illum`, `Ka`, `Ke`).** Use existing per-surface light/Gouraud
   controls for an explicitly chosen unlit policy (`illum 0`). Treat ambient
   and emissive values as preview/baked-color inputs until an equivalent target
   policy exists. Apply imported attributes after `specialConditions` in normal
   mode and after generated textures in UV mode so defaults cannot erase them.
   Existing manual surface edits remain the final override.
5. **Specular and advanced maps (`Ks`, `Ns`, `Ni`, bump/normal/reflection).**
   Parse and preserve metadata first, warn on unsupported export, then consider
   preview or offline baking. Standard MTL does not define double-sided Saturn
   flags, sprite animation or sorting; retain the current naming conventions and
   surface editor. Do not reinterpret `Ni` as a supported refraction effect.

For each stage, test material separation, normal and UV modes, palette results,
animation naming, manual overrides and legacy assets without an MTL. Verify
exported field widths/alignment remain unchanged; any new runtime material data
needs a separately agreed, versioned file-format change.