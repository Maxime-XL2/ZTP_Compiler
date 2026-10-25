# ZTP Z01 / S01: specification for the Saturn loader

This document describes the bytes emitted by the compiler. All offsets and
sizes below are in bytes. Multibyte integers are big-endian. No native C++
object, host pointer, STL container or compiler padding is serialized.

## What changes compared with legacy ZTP

1. A 32-byte container header precedes the existing model payload.
2. The first eight bytes are `ZTP_Z01\0` for useSGL=0 or `ZTP_S01\0` for useSGL=1.
   These are seven ASCII characters followed by one zero byte, not a newline.
3. An optional, four-byte-aligned LOD directory and compact LOD face streams
   follow the payload. The directory includes base level 0 and all added levels.
4. When LODs exist, vertex order is changed consistently in the base pose,
   every animation frame, base polygons and every LOD polygon list.
5. Positions, textures and animation frames are not duplicated per LOD.
6. Base level normals are shared by all levels. There is no additional normal
   stream per LOD in version 01.

The legacy payload's field widths, record layouts and endianness are unchanged.
Do not read an identified file with an old loader starting at byte zero.
The converter reads legacy non-SGL and SGL payloads without a signature and
writes only identified containers on export or Save. It preserves the source
mode on Save. The old experimental useSGL=2 writer layout is not supported.
An unknown `ZTP_` signature/version is rejected, not treated as a legacy file.

## Container header: 32 bytes

| Offset | Size | Type | Field |
|---:|---:|---|---|
| 0 | 8 | byte[8] | Signature, including terminating zero |
| 8 | 4 | u32 | modelOffset: always 32 |
| 12 | 4 | u32 | modelBytes: exact legacy payload size, excluding padding and LOD |
| 16 | 4 | u32 | lodOffset: absolute file offset, or zero |
| 20 | 4 | u32 | lodBytes: directory plus all LOD face streams, or zero |
| 24 | 2 | u16 | levelCount: includes base level 0; 1..64 |
| 26 | 2 | u16 | meshCount: must equal the payload's mesh count; 1..256 |
| 28 | 4 | u32 | flags: bit 0 = LOD directory present; bit 1 = SGL LOD indices are u16 |

Without added LODs: levelCount=1, flags=0, lodOffset=lodBytes=0 and file size
equals 32+modelBytes. With added LODs: bit 0 is set; bit 1 is additionally set
only for S01 files whose added LODs need more than 256 points,
`lodOffset = align4(32 + modelBytes)` and file size equals lodOffset+lodBytes.
The zero to three alignment bytes between payload and directory are zero.
Offsets are from the start of the file, not from the payload or directory.

## Shared textures

Both payload types use the existing texture block. There is one 12-byte
header followed immediately by image bytes for each texture, in index order:

| Relative offset | Type | Field |
|---:|---|---|
| 0 | u16 | width |
| 2 | u16 | height |
| 4 | u16 | depth: 3 for indexed 4bpp, 1 for RGB555 |
| 6 | u16 | paletteSize: 16 for indexed, 0 for RGB555 |
| 8 | u32 | pointer placeholder: zero |
| 12 | bytes | image: width*height/2 indexed bytes, or width*height*2 RGB bytes |

Indexed pixels use the high nibble first. RGB pixels are big-endian u16.
After all headers/images, every texture has a 32-byte palette (16 big-endian
u16), including RGB textures. There is no per-image padding. Indexed images
must have an even pixel count. `textureBytes` includes headers, images and all
palettes. Texture indices, including base polygon references, are zero-based.

## Z01 payload: unchanged non-SGL layout

Offsets in this table are relative to modelOffset (32 in a modern file).

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | meshCount, u16 |
| 2 | 2 | textureCount, u16 |
| 4 | 4 | textureBytes, u32 |
| 8 | 2 | totalStoredPoints V, u16 |
| 10 | 2 | basePolygonCount P, u16; does NOT include LOD faces |
| 12 | 12 | BoxMin, three signed 16.16 s32 |
| 24 | 12 | BoxMax, three signed 16.16 s32 |
| 36 | 12 | scale, three signed 16.16 s32 |
| 48 | 2 | frameCount F, u16 |
| 50 | 2 | framerate, u16 |
| 52 | 4 | pointer placeholder, zero |
| 56 | 36 | nine reserved u32, zero |
| 92 | textureBytes | shared texture block |

The bounding box covers the base vertices and all animation frames. For Z01,
its bounds reflect the signed 8-bit coordinates after the same rounding used
when those vertices are written; S01 bounds cover the stored fixed-point
coordinates.

After the texture block, in this exact order:

1. M mesh-range records, 12 bytes each.
2. V base point records, 4 bytes each, concatenated by mesh.
3. F frame blocks, each containing V point records in the SAME mesh/point order.
4. P base polygon records, 12 bytes each, concatenated by mesh.
5. V base vertex normals, three signed 16.16 s32 per vertex (12 bytes each).

Mesh-range record:

| Offset | Type | Field |
|---:|---|---|
| 0 | u16 | firstStoredPoint, inclusive, global |
| 2 | u16 | endStoredPoint, exclusive, global |
| 4 | u16 | firstBasePolygon, inclusive, global |
| 6 | u16 | endBasePolygon, exclusive, global |
| 8 | u16 | light ID: emitted as zero |
| 10 | s16 | light distance: emitted as zero |

Point record: s8 X, s8 Y, s8 Z, u8 compressed normal index. Apply the shared
scale to positions as in the legacy loader. Each frame stride is **4*V**,
regardless of the active LOD. Normal indices for frames are preserved.

Base polygon record:

| Offset | Type | Field |
|---:|---|---|
| 0 | u8[4] | point indices: low byte of global stored point index |
| 4 | u16 | texture index |
| 6 | u16 | command/direction low 12 bits, sorting bits 12..13, dual-plane bit 11 |
| 8 | u16 | drawing attributes / PMOD |
| 10 | u16 | color number |

Recover a base polygon's local vertex with
`(encodedIndex - (firstStoredPoint & 255)) & 255`. Add firstStoredPoint to address
the shared global point/frame array. Each mesh is limited to 256 points.
Do not apply this global-low-byte decoding to the added LOD streams: those
indices are directly LOCAL to their mesh.

Sorting uses the SGL enumeration: BFR=0, MIN=1, MAX=2, CEN=3. CEN is the
default for newly constructed attributes and means center/average sorting.
Modern loading preserves the mode encoded by the file; mode zero is a valid
BFR, not an unset value. For unidentified non-SGL files only, if EVERY polygon
command contains only low-six-bit VDP1 command/flip data (no bits 6..15), the
converter treats the file as the historical raw-command variant and defaults
sorting to CEN. This matches the old engine's center fallback. Saving then
encodes CEN explicitly. An unidentified all-BFR, single-plane raw-command file
cannot be distinguished from that historical variant; use an identified Z01
file to express BFR unambiguously. Legacy files with any packed upper command
bits keep their encoded modes. SGL attributes carry the mode in their low two sorting
bits, with option flags in the other bits. Z01/legacy non-SGL records and
added LOD records carry the mode in command bits 12..13.

Z01 payload size is exactly:

`92 + textureBytes + 12*M + 4*V*(1+F) + 12*P + 12*V`.

## S01 payload: unchanged SGL layout

| Relative offset | Size | Field |
|---:|---:|---|
| 0 | 2 | meshCount, u16 |
| 2 | 2 | textureCount, u16 |
| 4 | 4 | textureBytes, u32 |
| 8 | 4 | legacy RAM-size estimate, u32 |
| 12 | 12 | BoxMin, three signed 16.16 s32 |
| 24 | 12 | BoxMax, three signed 16.16 s32 |
| 36 | 2 | frameCount F, u16 |
| 38 | 2 | framerate, u16 |
| 40 | 4 | pointer placeholder, zero |
| 44 | textureBytes | shared texture block |

S01 has no scale field: positions are already scaled. Do not interpret its
RAM-size estimate as payload length or as a budget for the added LOD buffers.
Use modelBytes/lodBytes and decoded counts to compute actual allocations.

After textures, each mesh has these consecutive blocks:

1. Six u32 (24 bytes): point pointer=0, storedPointCount, polygon pointer=0,
   basePolygonCount, attribute pointer=0, normal pointer=0.
2. Base positions: storedPointCount * three signed 16.16 s32 (12 bytes/point).
3. Base polygons: three signed 16.16 s32 normal components then four local u16
   point indices (20 bytes/polygon).
4. Attributes: flag u8, sorting u8, texno u16, atrb u16, colno u16, gstb u16,
   dir u16 (12 bytes/polygon).
5. Base vertex normals: three signed 16.16 s32 (12 bytes/point).

After ALL mesh blocks, each animation frame contains:

1. Two u32 frame-index placeholders (8 bytes; emitted as the frame number).
2. All V positions, concatenated by mesh: three signed 12.4 s16 per point.
3. One zero s16 if V is odd, so the position block is four-byte aligned.
4. P compressed polygon-normal indices, u8, concatenated by mesh.
5. Zero bytes rounding the polygon-normal block up to four bytes.

Frame stride is `8 + 6*V + 2*(V % 2) + align4(P)`; it is NEVER shortened when
a coarse LOD is active. Base polygon normal indices still describe the base
topology, not the additional LOD faces.

S01 payload size is `44 + textureBytes + sum(24 + 24*meshPoints +
32*meshPolygons) + F*frameStride`.

SGL base meshes support up to 65,535 points. Existing S01 LODs use u8 indices
and remain byte-for-byte compatible. When an S01 LOD needs more than 256 active
points, container flag bit 1 selects u16 LOD indices; all added LOD face records
in that file then use the wide encoding. Z01 continues to use u8 LOD indices
and is limited to 256 active points per mesh.

## LOD directory: 16 bytes per (level, mesh)

The directory starts at lodOffset. Entries are level-major, then mesh-major:
`entryOffset = lodOffset + 16*(level*meshCount + mesh)`.
Level 0 is the original/base topology. Levels 1, 2, ... are additional levels
sorted by strictly increasing target reduction percentage.

| Entry offset | Size | Type | Field |
|---:|---:|---|---|
| 0 | 2 | u16 | activePointCount, the prefix length for this level/mesh |
| 2 | 2 | u16 | flags: 0 for base, 1 for flat RGB added-LOD faces |
| 4 | 4 | u32 | polygonCount for this level/mesh |
| 8 | 4 | u32 | polygonOffset: absolute file offset, or zero |
| 12 | 2 | u16 | targetReduction: base=0, added levels=1..99 |
| 14 | 2 | u16 | reserved: zero |

For level 0: activePointCount=storedPointCount, polygonCount=base face count,
polygonOffset=0, flags=0. Its geometry is in the original model payload and
must be decoded using the Z01/S01 base layout, not the LOD face format.

For added levels: flags=1; polygonOffset points to a separate list of 12-byte
records described below. Empty lists have polygonOffset=0. Lists are written
in the same level/mesh order, immediately after the complete directory, with
no gaps. The current reader enforces this canonical layout. Every face index
must be strictly less than activePointCount. Reduction is identical for all
meshes within one level; it is an editor target, NOT a distance threshold or
the guaranteed achieved reduction.

## Added LOD face: compact and wide encodings

| Offset | Type | Field |
|---:|---|---|
| 0 | u8[4] or u16[4] | LOCAL vertex indices into the mesh's shared point prefix |
| 4 or 8 | u16 | texture sentinel: 0xFFFF (no texture lookup) |
| 6 or 10 | u16 | FUNC_Polygon=4, sorting bits 12..13, dual-plane bit 11 |
| 8 or 12 | u16 | PMOD: source attributes with color-mode bits 3..5 replaced by 5 (0x28) |
| 10 or 14 | u16 | solid RGB555 color, normally bit 15 set |

The compact encoding is 12 bytes and remains common to Z01 and S01. The wide
encoding is 16 bytes, is valid only for S01, and is selected for the whole file
by container flag bit 1. Wide point indices and the active point count are u16.

Triangles repeat their third index in the fourth slot. Existing quads can
remain when the simplifier falls back to source topology. Do not dereference
texture 0xFFFF or treat the final u16 as a palette/Gouraud table address.
The color is the alpha-weighted mean of the source texture, quantized to
RGB555. Source surface attributes and sorting are otherwise inherited.
Per-level normals and UVs are not stored. Gouraud/lighting support must use
the shared base vertex normals or normals calculated by the engine; the
compiler's flat-lit preview is not a promise of pixel-identical Saturn lighting.

## Prefix order and runtime playback

For EACH mesh, the writer first places points referenced by the coarsest LOD,
then points needed by successively finer levels, then all remaining original
points. All original points are retained, including base-only/unused ones.
A single permutation remaps base vertices, vertex normals, all frame records,
base faces and all LOD faces. The working model is not modified by saving.

Example of stored counts:

| Level | Active points | Addressable range within each mesh |
|---|---:|---|
| 0 (base) | 240 | [0, 240) |
| 1 | 180 | [0, 180) |
| 2 (coarse) | 120 | [0, 120) |

Different independently simplified levels can reference non-nested vertex
sets. Prefixes use cumulative unions from coarse to fine, so a fine prefix
can contain points unused by its own polygon list. Counts are non-increasing
from base to coarse, but need not strictly decrease.

Runtime algorithm for mesh m and active level L:

1. Keep storedPointCount[m] for allocation and frame/submesh addressing.
2. Read activePointCount[L,m] from the directory (base count if no directory).
3. Find both animation frames using the FULL stored frame stride.
4. Within each frame, find mesh m using the SUM OF FULL stored counts of all
   preceding meshes, NOT their active counts.
5. Interpolate, transform and project only points [0, activePointCount[L,m]).
6. Draw only that level's face list. At level 0, use the original base topology.
7. When switching to a finer level, compute newly active points at the current
   animation phase before drawing them. Do not advance/restart the animation.

Keep each mesh's output point buffer sized to its FULL stored count if base
mesh offsets are reused for transformed vertices. Skipping points must not
shift the output base of the next mesh. You may instead use per-mesh pointers,
but polygon addressing must follow that choice consistently.

The converter's preview currently still interpolates the full geometry so
full-model shadow tests remain valid. The prefix counts provide the engine
with the information to skip unnecessary interpolation/transformation.
Distance selection and hysteresis belong to the engine and are not encoded
in version 01. Bounds and scale remain those of the base model.

## Alignment and compatibility checklist

- Read numeric fields big-endian; never use a Windows sizeof(struct) as a file size.
- Container and directory records are multiples of four bytes. LOD faces are
  12 bytes and start on four-byte boundaries. Existing payload blocks retain
  their old layout without additional internal padding. Texture sizes may
  therefore affect internal alignment; decode/copy unaligned legacy data into
  aligned Saturn buffers rather than casting arbitrary file offsets.
- No change to the serialized widths of vertices, ranges or base polygons.
  The compiler's animated_model_t has additional HOST-only metadata fields;
  they are not Saturn structures and are not written verbatim.
- Check offsets, lengths, point ranges and index bounds before constructing
  runtime pointers. Reject unknown signatures, flags and versions.
- Up to 64 total levels, 256 meshes, 1280 textures, 65535 stored points,
  65535 base polygons, and 65535 frames are representable; the converter also
   limits base polygons and each added face list to 32768, and total stored
   points * frames to 4 million for resource safety.
- LOD generation is animation-aware, but samples intermediate poses. It does
  not prove absence of intersections/inversions at every possible playback time.
- Legacy readers need an update before loading either identified format.
  No version identifier, distance switching or pointer extension is hidden
  inside the legacy header's reserved words.