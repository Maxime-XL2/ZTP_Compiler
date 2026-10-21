#include "../COMMON.H"

extern string fbxLoadError;
#include <algorithm>
#include <iomanip>
#include <limits>
#include <vector>
#include <array>
#include <iterator>
#include <new>
#include <cstring>
#include "mtl.h"
extern int USE_SGL;
extern bool loadFBX(const string &, float *, animated_model_t *, int, int, int, float, float, float);

namespace
{
class ZTPReader
{
public:
    explicit ZTPReader(const std::vector<uint8_t> &data, size_t position = 0) : data(data), offset(position) {}

    bool readU8(uint8_t &value)
    {
        if (remaining() < 1) return false;
        value = data[offset++];
        return true;
    }

    bool readS8(int8_t &value)
    {
        uint8_t raw;
        if (!readU8(raw)) return false;
        value = raw < 0x80 ? static_cast<int8_t>(raw) : static_cast<int8_t>(static_cast<int>(raw) - 0x100);
        return true;
    }

    bool readU16(uint16_t &value)
    {
        uint8_t first, second;
        if (!readU8(first) || !readU8(second)) return false;
        value = static_cast<uint16_t>((first << 8) | second);
        return true;
    }

    bool readU32(uint32_t &value)
    {
        uint8_t bytes[4];
        for (unsigned int index = 0; index < 4; ++index)
            if (!readU8(bytes[index])) return false;
        value = (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) |
                (static_cast<uint32_t>(bytes[2]) << 8) | bytes[3];
        return true;
    }

    bool readS16(int16_t &value)
    {
        uint16_t raw;
        if (!readU16(raw)) return false;
        value = raw < 0x8000 ? static_cast<int16_t>(raw) : static_cast<int16_t>(int32_t(raw) - 0x10000);
        return true;
    }

    bool readS32(int32_t &value)
    {
        uint32_t raw;
        if (!readU32(raw)) return false;
        const int64_t signedValue = raw & 0x80000000u ? static_cast<int64_t>(raw) - 0x100000000LL : raw;
        value = static_cast<int32_t>(signedValue);
        return true;
    }

    bool readBytes(uint8_t *destination, size_t count)
    {
        if (remaining() < count) return false;
        std::copy(data.begin() + offset, data.begin() + offset + count, destination);
        offset += count;
        return true;
    }

    size_t remaining() const { return data.size() - offset; }
    size_t position() const { return offset; }

private:
    const std::vector<uint8_t> &data;
    size_t offset;
};

struct ZTPMeshRange
{
    uint16_t firstPoint;
    uint16_t endPoint;
    uint16_t firstPolygon;
    uint16_t endPolygon;
};

void destroyLoadedZTP(animated_model_t *model)
{
    if (!model) return;
    delete model->lodData;
    if (model->model)
        for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
        {
            delete [] model->model[mesh].pntbl;
            delete [] model->model[mesh].pltbl;
        }
    if (model->keyFrames)
        for (unsigned int frame = 0; frame < model->nbFrames; ++frame)
            delete [] model->keyFrames[frame].cVert;
    if (model->texture)
        for (unsigned int texture = 0; texture < model->nbTextures; ++texture)
            delete [] model->texture[texture].pixel;
    delete [] model->keyFrames;
    delete [] model->model;
    delete [] model->texture;
    delete model;
}

bool readFixedVector(ZTPReader &reader, float *destination, unsigned int count)
{
    for (unsigned int index = 0; index < count; ++index)
    {
        int32_t fixed;
        if (!reader.readS32(fixed)) return false;
        destination[index] = fixed / 65536.0f;
    }
    return true;
}
}

static bool readLodDirectory(animated_model_t * model, const std::vector<uint8_t> & bytes,
                             uint32_t lodOffset, unsigned int levels, bool wideIndices)
{
    const size_t directoryBytes = size_t(levels) * model->nbModels * 16;
    if (bytes.size() < directoryBytes) return false;
    ZTPReader directory(bytes);
    if (!model->lodData) model->lodData = new model_lod_data_t;
    model->lodData->levels.resize(levels - 1);
    size_t nextFace = directoryBytes;
    std::vector<unsigned int> previousPoints(model->nbModels);
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh) previousPoints[mesh] = model->model[mesh].nbPoint;
    unsigned int previousReduction = 0;
    for (unsigned int level = 0; level < levels; ++level)
    {
        unsigned int currentReduction = 0;
        if (level) model->lodData->levels[level - 1].meshes.resize(model->nbModels);
        for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
        {
            uint16_t points, flags, reduction, reserved;
            uint32_t faces, faceOffset;
            if (!directory.readU16(points) || !directory.readU16(flags) || !directory.readU32(faces) ||
                !directory.readU32(faceOffset) || !directory.readU16(reduction) || !directory.readU16(reserved) || reserved)
                return false;
            if (!level)
            {
                if (points != previousPoints[mesh] || flags || faces != model->model[mesh].nbPolygon || faceOffset || reduction)
                    return false;
                continue;
            }
            if (points > previousPoints[mesh] || points > (wideIndices ? 65535u : 256u) || flags != 1 || faces > maxPolygons ||
                !reduction || reduction > 99 || reduction <= previousReduction ||
                (mesh && reduction != currentReduction) ||
                (faces ? faceOffset != lodOffset + nextFace : faceOffset != 0) ||
                uint64_t(nextFace) + uint64_t(faces) * (wideIndices ? 16u : 12u) > bytes.size()) return false;
            currentReduction = reduction;
            previousPoints[mesh] = points;
            lod_mesh_t & lod = model->lodData->levels[level - 1].meshes[mesh];
            lod.pointCount = points;
            lod.faces.resize(faces);
            ZTPReader faceReader(bytes, nextFace);
            for (polygon_t & face : lod.faces)
            {
                for (unsigned int corner = 0; corner < 4; ++corner)
                {
                    uint16_t point;
                    if (wideIndices)
                    {
                        if (!faceReader.readU16(point) || point >= points) return false;
                    }
                    else
                    {
                        uint8_t compactPoint;
                        if (!faceReader.readU8(compactPoint) || compactPoint >= points) return false;
                        point = compactPoint;
                    }
                    face.vertIdx[corner] = point;
                }
                uint16_t texture, command, attributes, color;
                if (!faceReader.readU16(texture) || !faceReader.readU16(command) ||
                    !faceReader.readU16(attributes) || !faceReader.readU16(color) || texture != 0xFFFF ||
                    (command & 15) != FUNC_Polygon || (attributes & 0x38) != 0x28) return false;
                face.texture = -1;
                face.SGL_ATTR.dir = command & 0x7FF;
                face.SGL_ATTR.sorting = (command >> 12) & 3;
                face.SGL_ATTR.flag = (command >> 11) & 1;
                face.SGL_ATTR.atrb = attributes;
                face.SGL_ATTR.colno = color;
                face.color.rgb = color;
                face.color.r = (color & 31) * 8;
                face.color.g = ((color >> 5) & 31) * 8;
                face.color.b = ((color >> 10) & 31) * 8;
                face.color.a = 255;
            }
            nextFace += size_t(faces) * (wideIndices ? 16u : 12u);
        }
        if (level)
        {
            model->lodData->levels[level - 1].reduction = currentReduction;
            previousReduction = currentReduction;
        }
    }
    return nextFace == bytes.size();
}

static animated_model_t * readZTPFile(const string &filePath, bool sgl)
{
    ifstream file(filePath.c_str(), ios::binary);
    if (!file.is_open()) return NULL;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (file.bad() || bytes.empty()) return NULL;

    std::vector<uint8_t> lodBlock;
    uint32_t lodOffset = 0;
    uint16_t levels = 1, containerMeshes = 0;
    bool wideLodIndices = false;
    const bool modern = bytes.size() >= 4 && std::memcmp(bytes.data(), "ZTP_", 4) == 0;
    if (modern)
    {
        if (bytes.size() < 32) return NULL;
        if (std::memcmp(bytes.data(), "ZTP_Z01\0", 8) == 0) sgl = false;
        else if (std::memcmp(bytes.data(), "ZTP_S01\0", 8) == 0) sgl = true;
        else return NULL;
        ZTPReader container(bytes);
        uint8_t signature[8];
        uint32_t modelOffset, modelBytes, lodBytes, flags;
        if (!container.readBytes(signature, 8) || !container.readU32(modelOffset) ||
            !container.readU32(modelBytes) || !container.readU32(lodOffset) ||
            !container.readU32(lodBytes) || !container.readU16(levels) ||
            !container.readU16(containerMeshes) || !container.readU32(flags) || modelOffset != 32 ||
            modelBytes > bytes.size() - 32 || !levels || levels > 64 || !containerMeshes || containerMeshes > 256 ||
            (levels == 1 ? (flags || lodOffset || lodBytes || modelBytes != bytes.size() - 32) :
             ((flags != 1 && (!sgl || flags != 3)) || lodOffset != ((32u + modelBytes + 3u) & ~3u) ||
              uint64_t(lodOffset) + lodBytes != bytes.size())))
            return NULL;
        wideLodIndices = flags == 3;
        if (levels > 1)
        {
            if (lodOffset > bytes.size()) return NULL;
            for (size_t padding = 32 + modelBytes; padding < lodOffset; ++padding)
                if (bytes[padding]) return NULL;
            lodBlock.assign(bytes.begin() + lodOffset, bytes.end());
        }
        bytes.resize(32 + modelBytes);
        bytes.erase(bytes.begin(), bytes.begin() + 32);
    }

    ZTPReader reader(bytes);
    uint16_t modelCount, textureCount, pointCount = 0, polygonCount = 0, frameCount, frameRate;
    uint32_t textureBlockSize, reserved;
    if (!reader.readU16(modelCount) || !reader.readU16(textureCount) || !reader.readU32(textureBlockSize)) return NULL;
    if (sgl) { if (!reader.readU32(reserved)) return NULL; }
    else if (!reader.readU16(pointCount) || !reader.readU16(polygonCount)) return NULL;
    if (modelCount == 0 || modelCount > 256 || textureCount > 1280 || polygonCount > maxPolygons) return NULL;
    if (modern && containerMeshes != modelCount) return NULL;

    animated_model_t *model = new (std::nothrow) animated_model_t();
    if (!model) return NULL;
    model->nbModels = modelCount;
    model->sourceSgl = sgl ? 1 : 0;
    model->nbTextures = textureCount;
    model->model = new (std::nothrow) model_t[modelCount]();
    model->texture = new (std::nothrow) texture_t[textureCount]();
    if (!model->model || (textureCount && !model->texture))
    {
        destroyLoadedZTP(model);
        return NULL;
    }

    if (!readFixedVector(reader, model->BoxMin, XYZ) || !readFixedVector(reader, model->BoxMax, XYZ) ||
        (!sgl && !readFixedVector(reader, model->scale, XYZ)) || !reader.readU16(frameCount) ||
        !reader.readU16(frameRate) || !reader.readU32(reserved))
    {
        destroyLoadedZTP(model);
        return NULL;
    }
    for (unsigned int axis = 0; sgl && axis < XYZ; ++axis) model->scale[axis] = 1.0f;
    for (unsigned int index = 0; !sgl && index < 9; ++index)
        if (!reader.readU32(reserved))
        {
            destroyLoadedZTP(model);
            return NULL;
        }
    model->nbFrames = frameCount;
    model->framerate = frameRate;

    const size_t textureBlockStart = reader.position();
    std::vector<std::vector<uint8_t> > encodedTextures(textureCount);
    for (unsigned int textureIndex = 0; textureIndex < textureCount; ++textureIndex)
    {
        texture_t &texture = model->texture[textureIndex];
        uint16_t width, height, depth, paletteSize;
        if (!reader.readU16(width) || !reader.readU16(height) || !reader.readU16(depth) ||
            !reader.readU16(paletteSize) || !reader.readU32(reserved) || width == 0 || height == 0 ||
            static_cast<uint64_t>(width) * height > 16u * 1024u * 1024u ||
            (depth != COL_16 && depth != COL_32K) ||
            paletteSize != (depth == COL_16 ? 16 : 0))
        {
            destroyLoadedZTP(model);
            return NULL;
        }
        texture.width = width;
        texture.height = height;
        texture.colorDepth = depth;
        texture.textureId = textureIndex;
        const size_t pixelCount = static_cast<size_t>(width) * height;
        if (depth == COL_16 && (pixelCount & 1))
        {
            destroyLoadedZTP(model);
            return NULL;
        }
        const size_t imageByteCount = depth == COL_32K ? pixelCount * 2 : (pixelCount + 1) / 2;
        encodedTextures[textureIndex].resize(imageByteCount);
        if (!reader.readBytes(encodedTextures[textureIndex].data(), imageByteCount))
        {
            destroyLoadedZTP(model);
            return NULL;
        }
    }

    for (unsigned int textureIndex = 0; textureIndex < textureCount; ++textureIndex)
    {
        texture_t &texture = model->texture[textureIndex];
        const unsigned int depth = texture.colorDepth;
        const size_t pixelCount = static_cast<size_t>(texture.width) * texture.height;
        for (unsigned int color = 0; color < 16; ++color)
            if (!reader.readU16(texture.clut[color]))
            {
                destroyLoadedZTP(model);
                return NULL;
            }

        texture.pixel = new (std::nothrow) pixel_t[pixelCount]();
        if (!texture.pixel)
        {
            destroyLoadedZTP(model);
            return NULL;
        }
        for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
        {
            pixel_t &pixel = texture.pixel[pixelIndex];
            uint16_t color;
            if (depth == COL_32K)
            {
                color = static_cast<uint16_t>((encodedTextures[textureIndex][pixelIndex * 2] << 8) |
                                              encodedTextures[textureIndex][pixelIndex * 2 + 1]);
            }
            else
            {
                const uint8_t packed = encodedTextures[textureIndex][pixelIndex / 2];
                pixel.palette_idx = (pixelIndex & 1) ? packed & 0x0F : packed >> 4;
                color = texture.clut[pixel.palette_idx];
            }
            pixel.rgb = color;
            pixel.r = static_cast<uint8_t>((color & 0x1F) * 8);
            pixel.g = static_cast<uint8_t>(((color >> 5) & 0x1F) * 8);
            pixel.b = static_cast<uint8_t>(((color >> 10) & 0x1F) * 8);
            pixel.a = color == 0 ? 0 : 255;
        }
    }
    if (reader.position() - textureBlockStart != textureBlockSize)
    {
        destroyLoadedZTP(model);
        return NULL;
    }

    if (sgl)
    {
        const auto fail = [&]() -> animated_model_t * { destroyLoadedZTP(model); return NULL; };
        uint32_t totalPoints = 0, totalFaces = 0;
        for (unsigned int mesh = 0; mesh < modelCount; ++mesh)
        {
            model_t & part = model->model[mesh];
            uint32_t points, faces;
            if (!reader.readU32(reserved) || !reader.readU32(points) || !reader.readU32(reserved) ||
                !reader.readU32(faces) || !reader.readU32(reserved) || !reader.readU32(reserved) ||
                points > 65535 || faces > maxPolygons || uint64_t(totalPoints) + points > 65535 ||
                uint64_t(totalFaces) + faces > maxPolygons ||
                uint64_t(points) * 24 + uint64_t(faces) * 32 > reader.remaining()) return fail();
            part.nbPoint = points;
            part.nbPolygon = faces;
            part.pntbl = new vertex_t[points]();
            part.pltbl = new polygon_t[faces]();
            for (unsigned int axis = 0; axis < XYZ; ++axis)
            {
                part.BoxMin[axis] = std::numeric_limits<float>::max();
                part.BoxMax[axis] = -std::numeric_limits<float>::max();
            }
            for (unsigned int point = 0; point < points; ++point)
            {
                if (!readFixedVector(reader, part.pntbl[point].point, XYZ)) return fail();
                for (unsigned int axis = 0; axis < XYZ; ++axis)
                {
                    part.BoxMin[axis] = MIN(part.BoxMin[axis], part.pntbl[point].point[axis]);
                    part.BoxMax[axis] = MAX(part.BoxMax[axis], part.pntbl[point].point[axis]);
                }
            }
            for (unsigned int face = 0; face < faces; ++face)
            {
                polygon_t & polygon = part.pltbl[face];
                if (!readFixedVector(reader, polygon.normal, XYZ)) return fail();
                for (unsigned int corner = 0; corner < 4; ++corner)
                {
                    uint16_t point;
                    if (!reader.readU16(point) || point >= points) return fail();
                    polygon.vertIdx[corner] = point;
                }
            }
            for (unsigned int face = 0; face < faces; ++face)
            {
                polygon_t & polygon = part.pltbl[face];
                uint8_t flag, sorting;
                if (!reader.readU8(flag) || !reader.readU8(sorting) || !reader.readU16(polygon.SGL_ATTR.texno) ||
                    !reader.readU16(polygon.SGL_ATTR.atrb) || !reader.readU16(polygon.SGL_ATTR.colno) ||
                    !reader.readU16(polygon.SGL_ATTR.gstb) || !reader.readU16(polygon.SGL_ATTR.dir) ||
                    polygon.SGL_ATTR.texno >= textureCount) return fail();
                polygon.SGL_ATTR.flag = flag;
                polygon.SGL_ATTR.sorting = sorting;
                polygon.texture = polygon.SGL_ATTR.texno;
            }
            for (unsigned int point = 0; point < points; ++point)
                if (!readFixedVector(reader, part.pntbl[point].normal, XYZ)) return fail();
            totalPoints += points;
            totalFaces += faces;
        }
        if (!totalPoints || uint64_t(totalPoints) * frameCount > 4u * 1024u * 1024u) return fail();
        const uint64_t frameBytes = 8 + uint64_t(totalPoints) * 6 + (totalPoints % 2) * 2 + ((totalFaces + 3u) & ~3u);
        if (frameBytes * frameCount != reader.remaining()) return fail();
        model->keyFrames = new key_frame_t[frameCount]();
        model->lodData = new model_lod_data_t;
        model->lodData->sglFrameNormals.resize(frameCount);
        for (unsigned int frame = 0; frame < frameCount; ++frame)
        {
            if (!reader.readU32(reserved) || !reader.readU32(reserved)) return fail();
            model->keyFrames[frame].cVert = new vertex_t[totalPoints]();
            unsigned int offset = 0;
            for (unsigned int mesh = 0; mesh < modelCount; ++mesh)
                for (unsigned int point = 0; point < model->model[mesh].nbPoint; ++point, ++offset)
                {
                    vertex_t & vertex = model->keyFrames[frame].cVert[offset];
                    for (unsigned int axis = 0; axis < XYZ; ++axis)
                    {
                        int16_t coordinate;
                        if (!reader.readS16(coordinate)) return fail();
                        vertex.point[axis] = coordinate / 16.0f;
                        vertex.normal[axis] = model->model[mesh].pntbl[point].normal[axis];
                    }
                }
            if (totalPoints % 2) { uint16_t padding; if (!reader.readU16(padding)) return fail(); }
            auto & normals = model->lodData->sglFrameNormals[frame];
            normals.resize(totalFaces);
            if (!reader.readBytes(normals.data(), normals.size())) return fail();
            for (unsigned int padding = totalFaces; padding % 4; ++padding)
            {
                uint8_t value;
                if (!reader.readU8(value)) return fail();
            }
        }
        if (levels > 1 && !readLodDirectory(model, lodBlock, lodOffset, levels, wideLodIndices)) return fail();
        extern void calcLight(animated_model_t *);
        calcLight(model);
        return model;
    }

    std::vector<ZTPMeshRange> ranges(modelCount);
    uint32_t totalPoints = 0;
    uint32_t totalPolygons = 0;
    for (unsigned int mesh = 0; mesh < modelCount; ++mesh)
    {
        ZTPMeshRange &range = ranges[mesh];
        uint16_t lightId, lightDistance;
        if (!reader.readU16(range.firstPoint) || !reader.readU16(range.endPoint) ||
            !reader.readU16(range.firstPolygon) || !reader.readU16(range.endPolygon) ||
            !reader.readU16(lightId) || !reader.readU16(lightDistance) ||
            range.firstPoint != totalPoints || range.firstPolygon != totalPolygons ||
            range.endPoint < range.firstPoint || range.endPolygon < range.firstPolygon ||
            range.endPoint - range.firstPoint > 256)
        {
            destroyLoadedZTP(model);
            return NULL;
        }
        totalPoints = range.endPoint;
        totalPolygons = range.endPolygon;
        model->model[mesh].nbPoint = range.endPoint - range.firstPoint;
        model->model[mesh].nbPolygon = range.endPolygon - range.firstPolygon;
    }
    if (totalPoints != pointCount || totalPolygons != polygonCount || totalPoints == 0 ||
        static_cast<uint64_t>(totalPoints) * frameCount > 4u * 1024u * 1024u)
    {
        destroyLoadedZTP(model);
        return NULL;
    }

    for (unsigned int mesh = 0; mesh < modelCount; ++mesh)
    {
        model_t &submodel = model->model[mesh];
        submodel.pntbl = new (std::nothrow) vertex_t[submodel.nbPoint]();
        submodel.pltbl = new (std::nothrow) polygon_t[submodel.nbPolygon]();
        if ((submodel.nbPoint && !submodel.pntbl) || (submodel.nbPolygon && !submodel.pltbl))
        {
            destroyLoadedZTP(model);
            return NULL;
        }
        for (unsigned int axis = 0; axis < XYZ; ++axis)
        {
            submodel.BoxMin[axis] = std::numeric_limits<float>::max();
            submodel.BoxMax[axis] = -std::numeric_limits<float>::max();
        }
    }

    const auto readPoints = [&](vertex_t *destination) -> bool
    {
        for (unsigned int mesh = 0; mesh < modelCount; ++mesh)
        {
            model_t &submodel = model->model[mesh];
            for (unsigned int point = 0; point < submodel.nbPoint; ++point)
            {
                vertex_t &vertex = destination ? destination[ranges[mesh].firstPoint + point] : submodel.pntbl[point];
                for (unsigned int axis = 0; axis < XYZ; ++axis)
                {
                    int8_t coordinate;
                    if (!reader.readS8(coordinate)) return false;
                    vertex.point[axis] = coordinate;
                    if (!destination)
                    {
                        submodel.BoxMin[axis] = MIN(submodel.BoxMin[axis], vertex.point[axis]);
                        submodel.BoxMax[axis] = MAX(submodel.BoxMax[axis], vertex.point[axis]);
                    }
                }
                uint8_t normalIndex;
                if (!reader.readU8(normalIndex)) return false;
                vertex.normalArray = normalIndex;
            }
        }
        return true;
    };
    if (!readPoints(NULL))
    {
        destroyLoadedZTP(model);
        return NULL;
    }

    model->keyFrames = new (std::nothrow) key_frame_t[frameCount]();
    if (frameCount && !model->keyFrames)
    {
        destroyLoadedZTP(model);
        return NULL;
    }
    for (unsigned int frame = 0; frame < frameCount; ++frame)
    {
        model->keyFrames[frame].cVert = new (std::nothrow) vertex_t[totalPoints]();
        if (!model->keyFrames[frame].cVert || !readPoints(model->keyFrames[frame].cVert))
        {
            destroyLoadedZTP(model);
            return NULL;
        }
    }

    bool legacyRawCommands = !modern;
    for (unsigned int mesh = 0; mesh < modelCount; ++mesh)
    {
        model_t &submodel = model->model[mesh];
        for (unsigned int polygonIndex = 0; polygonIndex < submodel.nbPolygon; ++polygonIndex)
        {
            polygon_t &polygon = submodel.pltbl[polygonIndex];
            for (unsigned int corner = 0; corner < 4; ++corner)
            {
                uint8_t globalIndex;
                if (!reader.readU8(globalIndex))
                {
                    destroyLoadedZTP(model);
                    return NULL;
                }
                const unsigned int localIndex = (globalIndex - (ranges[mesh].firstPoint & 0xFF)) & 0xFF;
                if (localIndex >= submodel.nbPoint)
                {
                    destroyLoadedZTP(model);
                    return NULL;
                }
                polygon.vertIdx[corner] = localIndex;
            }
            uint16_t textureId, command, attributes, colorNumber;
            if (!reader.readU16(textureId) || !reader.readU16(command) || !reader.readU16(attributes) ||
                !reader.readU16(colorNumber) || textureId >= textureCount)
            {
                destroyLoadedZTP(model);
                return NULL;
            }
            polygon.texture = textureId;
            if (command & 0xFFC0) legacyRawCommands = false;
            polygon.SGL_ATTR.dir = command & 0x0FFF;
            polygon.SGL_ATTR.sorting = (command >> 12) & 3;
            polygon.SGL_ATTR.flag = (command >> 11) & 1;
            polygon.SGL_ATTR.atrb = attributes;
            polygon.SGL_ATTR.texno = textureId;
            polygon.SGL_ATTR.colno = colorNumber;
            model->texture[textureId].SGL_ATTR.colno = colorNumber;
        }
    }
    if (legacyRawCommands)
        for (unsigned int mesh = 0; mesh < modelCount; ++mesh)
            for (unsigned int face = 0; face < model->model[mesh].nbPolygon; ++face)
                model->model[mesh].pltbl[face].SGL_ATTR.sorting = SORT_CEN;

    for (unsigned int mesh = 0; mesh < modelCount; ++mesh)
    {
        model_t &submodel = model->model[mesh];
        for (unsigned int point = 0; point < submodel.nbPoint; ++point)
            if (!readFixedVector(reader, submodel.pntbl[point].normal, XYZ))
            {
                destroyLoadedZTP(model);
                return NULL;
            }
    }
    for (unsigned int frame = 0; frame < frameCount; ++frame)
        for (unsigned int mesh = 0; mesh < modelCount; ++mesh)
            for (unsigned int point = 0; point < model->model[mesh].nbPoint; ++point)
            {
                vertex_t &animated = model->keyFrames[frame].cVert[ranges[mesh].firstPoint + point];
                const vertex_t &base = model->model[mesh].pntbl[point];
                std::copy(base.normal, base.normal + XYZ, animated.normal);
                std::copy(base.color, base.color + XYZ, animated.color);
            }

    if (reader.remaining() || (levels > 1 && !readLodDirectory(model, lodBlock, lodOffset, levels, false)))
    {
        destroyLoadedZTP(model);
        return NULL;
    }
    extern void calcLight(animated_model_t *);
    calcLight(model);
    return model;
}

animated_model_t * loadZTPFile(const string & filePath)
{
    animated_model_t * model = readZTPFile(filePath, false);
    if (model) return model;
    return readZTPFile(filePath, true);
}
int generate_texture_list(string inString, animated_model_t * model)
{
    if (inString=="None") return -1;
    for (unsigned int i=0; i<model->nbTextures; i++)
    {
        if (inString == model->texture[i].name) return i;
    }
    model->texture[model->nbTextures].name = inString;
    model->texture[model->nbTextures].textureId = model->nbTextures;
    model->nbTextures++;
    return model->nbTextures-1;
}

/**Old implementation, weird code, it works, so who cares?**/
static std::vector<std::vector<unsigned int> > objMeshVertexSources;

static bool resolveOBJIndex(const string &text, size_t count, size_t &index)
{
    if (text.empty()) return false;
    try
    {
        size_t parsed = 0;
        const long long value = std::stoll(text, &parsed);
        if (parsed != text.size() || value == 0) return false;
        const long long resolved = value > 0 ? value - 1 : static_cast<long long>(count) + value;
        if (resolved < 0 || static_cast<unsigned long long>(resolved) >= count) return false;
        index = static_cast<size_t>(resolved);
        return true;
    }
    catch (const std::exception &)
    {
        return false;
    }
}

struct OBJFaceVertex
{
    size_t position;
    uv_t uv;
};

static bool triangulateOBJFace(const vector<OBJFaceVertex> &face,
                               const vector<vertex_t> &vertices,
                               vector<std::array<size_t, 3> > &triangles,
                               bool &convex)
{
    if (face.size() < 3) return false;

    double normal[3] = {0, 0, 0};
    for (size_t corner = 0; corner < face.size(); ++corner)
    {
        const float *current = vertices[face[corner].position].point;
        const float *next = vertices[face[(corner + 1) % face.size()].position].point;
        normal[0] += (current[Y] - next[Y]) * (current[Z] + next[Z]);
        normal[1] += (current[Z] - next[Z]) * (current[X] + next[X]);
        normal[2] += (current[X] - next[X]) * (current[Y] + next[Y]);
    }

    unsigned int droppedAxis = 0;
    if (fabs(normal[1]) > fabs(normal[droppedAxis])) droppedAxis = 1;
    if (fabs(normal[2]) > fabs(normal[droppedAxis])) droppedAxis = 2;
    if (fabs(normal[droppedAxis]) <= 1e-12) return false;

    vector<std::array<double, 2> > projected(face.size());
    double coordinateScale = 1.0;
    for (size_t corner = 0; corner < face.size(); ++corner)
    {
        const float *point = vertices[face[corner].position].point;
        const unsigned int firstAxis = (droppedAxis + 1) % 3;
        const unsigned int secondAxis = (droppedAxis + 2) % 3;
        projected[corner][0] = point[firstAxis];
        projected[corner][1] = point[secondAxis];
        coordinateScale = MAX(coordinateScale, fabs(projected[corner][0]));
        coordinateScale = MAX(coordinateScale, fabs(projected[corner][1]));
    }

    const double epsilon = coordinateScale * coordinateScale * 1e-12;
    const auto cross = [&](size_t a, size_t b, size_t c) -> double
    {
        return (projected[b][0] - projected[a][0]) * (projected[c][1] - projected[a][1]) -
               (projected[b][1] - projected[a][1]) * (projected[c][0] - projected[a][0]);
    };
    double signedArea = 0;
    for (size_t corner = 0; corner < face.size(); ++corner)
    {
        const size_t next = (corner + 1) % face.size();
        signedArea += projected[corner][0] * projected[next][1] -
                      projected[corner][1] * projected[next][0];
    }
    if (fabs(signedArea) <= epsilon) return false;
    const double orientation = signedArea > 0 ? 1.0 : -1.0;
    convex = true;
    for (size_t corner = 0; corner < face.size(); ++corner)
    {
        const size_t previous = (corner + face.size() - 1) % face.size();
        const size_t next = (corner + 1) % face.size();
        if (cross(previous, corner, next) * orientation < -epsilon) convex = false;
    }

    vector<size_t> remaining(face.size());
    for (size_t corner = 0; corner < face.size(); ++corner) remaining[corner] = corner;
    while (remaining.size() > 3)
    {
        bool clipped = false;
        for (size_t index = 0; index < remaining.size(); ++index)
        {
            const size_t previous = remaining[(index + remaining.size() - 1) % remaining.size()];
            const size_t current = remaining[index];
            const size_t next = remaining[(index + 1) % remaining.size()];
            if (cross(previous, current, next) * orientation <= epsilon) continue;

            bool containsCorner = false;
            for (size_t candidate : remaining)
            {
                if (candidate == previous || candidate == current || candidate == next) continue;
                const double first = cross(previous, current, candidate) * orientation;
                const double second = cross(current, next, candidate) * orientation;
                const double third = cross(next, previous, candidate) * orientation;
                if (first >= -epsilon && second >= -epsilon && third >= -epsilon)
                {
                    containsCorner = true;
                    break;
                }
            }
            if (containsCorner) continue;

            triangles.push_back({{previous, current, next}});
            remaining.erase(remaining.begin() + index);
            clipped = true;
            break;
        }
        if (!clipped) return false;
    }
    triangles.push_back({{remaining[0], remaining[1], remaining[2]}});
    return true;
}

static bool parseOBJFaceVertex(const string &token, size_t vertexCount, size_t texCoordCount,
                               size_t normalCount, OBJFaceVertex &vertex)
{
    const size_t firstSlash = token.find('/');
    const string positionText = token.substr(0, firstSlash);
    if (!resolveOBJIndex(positionText, vertexCount, vertex.position)) return false;
    vertex.uv.U = 0.0f;
    vertex.uv.V = 0.0f;

    if (firstSlash != string::npos)
    {
        const size_t secondSlash = token.find('/', firstSlash + 1);
        const size_t uvEnd = secondSlash == string::npos ? token.size() : secondSlash;
        const string uvText = token.substr(firstSlash + 1, uvEnd - firstSlash - 1);
        if (secondSlash == string::npos && uvText.empty()) return false;
        if (!uvText.empty())
        {
            size_t uvIndex = 0;
            if (!resolveOBJIndex(uvText, texCoordCount, uvIndex)) return false;
            vertex.uv = texture_coord[uvIndex];
        }
        if (secondSlash != string::npos)
        {
            if (token.find('/', secondSlash + 1) != string::npos) return false;
            const string normalText = token.substr(secondSlash + 1);
            size_t normalIndex = 0;
            if (!resolveOBJIndex(normalText, normalCount, normalIndex)) return false;
        }
    }
    return true;
}


void rotatePolygon(model_t * m) {
if (useUV) return;
    polygon_t * pol = &m->pltbl[m->nbPolygon];

    int indexZero = 0;
    float minUV[2]={99999, 99999};
    for (int i=0; i<4; i++){
        if (pol->vertUV[i].U==0 && pol->vertUV[i].V==0) {
            indexZero=i;
            break;
        }
        if (pol->vertUV[i].U > minUV[0]) continue;
        if (pol->vertUV[i].V > minUV[1]) continue;
        minUV[0]=pol->vertUV[i].U;
        minUV[1]=pol->vertUV[i].V;
        indexZero=i;
    }

    unsigned int idxArray[4];
    uv_t         vertUV[4];

    for (int i=0; i<4; i++){
        idxArray[i]=pol->vertIdx[i];
        vertUV[i].U = pol->vertUV[i].U;
        vertUV[i].V = pol->vertUV[i].V;
    }
    for (int i=0; i<4; i++){
        int idx = (indexZero+i+1)&3;
        pol->vertIdx[i]=idxArray[idx];
        pol->vertUV[i].U=vertUV[idx].U;
        pol->vertUV[i].V=vertUV[idx].V;
    }
}

/****
*Main function to read the OBJ file
*Param : Obj file, upscale value (integer), your model data (_MOD_DATA) and boolean if it's a base model
*****/
bool load_OBJ_to_mesh(ifstream *file, float *scalefactor, animated_model_t *aModel, int keyFrameID)
{
    if (!file->is_open()) return false;
    file->clear();
    file->seekg(0, ios::beg);

    string line;
    vector<vertex_t> sourceVertices;
    vector<vector<int> > localVertexIndices;
    size_t normalCount = 0;
    int textpointer = -1;
    int meshIndex = -1;
    tot_tex_coord = 0;
    if (keyFrameID == -1) objMeshVertexSources.clear();

    const auto addSourceVertexToMesh = [&](int targetMesh, size_t sourceIndex) -> bool
    {
        vector<int> &localIndices = localVertexIndices[targetMesh];
        if (sourceIndex >= localIndices.size()) localIndices.resize(sourceIndex + 1, -1);
        if (localIndices[sourceIndex] >= 0) return true;

        model_t &mesh = aModel->model[targetMesh];
        if (mesh.nbPoint >= maxPolygons) return false;
        const unsigned int localIndex = mesh.nbPoint++;
        mesh.pntbl[localIndex] = sourceVertices[sourceIndex];
        localIndices[sourceIndex] = static_cast<int>(localIndex);
        objMeshVertexSources[targetMesh].push_back(static_cast<unsigned int>(sourceIndex));
        return true;
    };

    const auto beginMesh = [&](const string &name) -> bool
    {
        if (meshIndex >= 255) return false;
        ++meshIndex;
        model_t &mesh = aModel->model[meshIndex];
        mesh.nbPoint = 0;
        mesh.nbPolygon = 0;
        mesh.pntbl = new vertex_t[maxPolygons]();
        mesh.pltbl = new polygon_t[maxPolygons]();
        localVertexIndices.push_back(vector<int>(sourceVertices.size(), -1));
        objMeshVertexSources.push_back(vector<unsigned int>());
        string meshName = name.empty() ? "Default" : name;
        const size_t copiedNameLength = std::min(meshName.size(), sizeof(mesh.name) - 1);
        memcpy(mesh.name, meshName.c_str(), copiedNameLength);
        mesh.name[copiedNameLength] = '\0';
        cout << "\nMESH NAME : " << meshName << "... Now loading polygons!\n";

        if (meshIndex == 0)
            for (size_t sourceIndex = 0; sourceIndex < sourceVertices.size(); ++sourceIndex)
                if (!addSourceVertexToMesh(meshIndex, sourceIndex)) return false;
        return true;
    };

    while (getline(*file, line))
    {
        line = ztp_mtl::trim(line);
        if (line.empty() || line[0] == '#') continue;
        stringstream record(line);
        string directive;
        record >> directive;

        if (directive == "v")
        {
            vertex_t vertex = {};
            if (!(record >> vertex.point[X] >> vertex.point[Y] >> vertex.point[Z])) return false;
            for (unsigned int axis = 0; axis < 3; ++axis)
                vertex.point[axis] *= scalefactor[axis];
            sourceVertices.push_back(vertex);
            if (keyFrameID == -1 && meshIndex >= 0 &&
                !addSourceVertexToMesh(meshIndex, sourceVertices.size() - 1)) return false;
        }
        else if (directive == "vt")
        {
            uv_t uv = {};
            if (!(record >> uv.U)) return false;
            record >> uv.V;
            if (tot_tex_coord >= maxPolygons * 4) return false;
            texture_coord[tot_tex_coord++] = uv;
        }
        else if (directive == "vn")
        {
            float normal[3];
            if (!(record >> normal[0] >> normal[1] >> normal[2])) return false;
            ++normalCount;
        }
        else if ((directive == "o" || directive == "g") && keyFrameID == -1)
        {
            string name;
            record >> name;
            if (!beginMesh(name)) return false;
        }
        else if (directive == "usemtl" && keyFrameID == -1)
        {
            string mtlName;
            getline(record, mtlName);
            textpointer = generate_texture_list(ztp_mtl::trim(mtlName), aModel);
        }
        else if (directive == "f" && keyFrameID == -1)
        {
            if (meshIndex < 0 && !beginMesh("Default")) return false;
            vector<OBJFaceVertex> face;
            string token;
            while (record >> token)
            {
                if (token[0] == '#') break;
                OBJFaceVertex vertex = {};
                if (!parseOBJFaceVertex(token, sourceVertices.size(), tot_tex_coord, normalCount, vertex)) return false;
                face.push_back(vertex);
            }
            if (face.size() < 3) return false;

            vector<unsigned int> localFace;
            localFace.reserve(face.size());
            for (const OBJFaceVertex &vertex : face)
            {
                if (!addSourceVertexToMesh(meshIndex, vertex.position)) return false;
                localFace.push_back(static_cast<unsigned int>(localVertexIndices[meshIndex][vertex.position]));
            }

            model_t &mesh = aModel->model[meshIndex];
            const auto appendPolygon = [&](const OBJFaceVertex &a, const OBJFaceVertex &b,
                                           const OBJFaceVertex &c, const OBJFaceVertex &d,
                                           unsigned int indexA, unsigned int indexB,
                                           unsigned int indexC, unsigned int indexD) -> bool
            {
                if (mesh.nbPolygon >= maxPolygons) return false;
                polygon_t &polygon = mesh.pltbl[mesh.nbPolygon];
                polygon = polygon_t();
                polygon.vertIdx[0] = indexA;
                polygon.vertIdx[1] = indexB;
                polygon.vertIdx[2] = indexC;
                polygon.vertIdx[3] = indexD;
                polygon.vertUV[0] = a.uv;
                polygon.vertUV[1] = b.uv;
                polygon.vertUV[2] = c.uv;
                polygon.vertUV[3] = d.uv;
                polygon.texture = textpointer;
                rotatePolygon(&mesh);
                ++mesh.nbPolygon;
                return true;
            };

            if (face.size() == 3)
            {
                if (!appendPolygon(face[2], face[2], face[1], face[0],
                                   localFace[2], localFace[2], localFace[1], localFace[0])) return false;
            }
            else
            {
                vector<std::array<size_t, 3> > triangles;
                bool convex = false;
                if (!triangulateOBJFace(face, sourceVertices, triangles, convex)) return false;
                if (face.size() == 4 && convex)
                {
                    if (!appendPolygon(face[3], face[2], face[1], face[0],
                                       localFace[3], localFace[2], localFace[1], localFace[0])) return false;
                    continue;
                }
                for (const std::array<size_t, 3> &triangle : triangles)
                {
                    const size_t a = triangle[0], b = triangle[1], c = triangle[2];
                    if (!appendPolygon(face[c], face[b], face[a], face[a],
                                       localFace[c], localFace[b], localFace[a], localFace[a])) return false;
                }
            }
        }
    }

    if (keyFrameID == -1)
    {
        if (meshIndex < 0 && !sourceVertices.empty() && !beginMesh("Default")) return false;
        if (meshIndex < 0) return false;
        aModel->nbModels = static_cast<unsigned int>(meshIndex + 1);
        extern void recalculateNormals(model_t *pol);
        extern void pointNormals(model_t *pol);
        for (unsigned int index = 0; index < aModel->nbModels; ++index)
        {
            recalculateNormals(&aModel->model[index]);
            pointNormals(&aModel->model[index]);
        }
    }
    else
    {
        unsigned int destination = 0;
        for (size_t mesh = 0; mesh < objMeshVertexSources.size(); ++mesh)
            for (unsigned int sourceIndex : objMeshVertexSources[mesh])
            {
                if (sourceIndex >= sourceVertices.size()) return false;
                aModel->keyFrames[keyFrameID].cVert[destination++] = sourceVertices[sourceIndex];
            }
    }

    file->clear();
    cout << "...done!\n";
    return true;
}


void specialConditions(unsigned short startPtr, unsigned short endPtr, animated_model_t * aModel)
{
    if (aModel->nbTextures <= 0) return;

    texture_t * t;
    for (unsigned short i=startPtr; i<endPtr; i++)
    {
        t = &aModel->texture[i];
        uint16_t colorDepth = CL16Look;
        if (t->colorDepth==COL_32K) colorDepth=CL32KRGB;

        std::size_t findDual = t->name.find("DUAL_");  //Legacy stuff...
        std::size_t findWhite = t->name.find("C_WHITE"); //Legacy stuff...

        if ((findDual==0) && (t->clut[1]==0 && t->clut[0]!=0) && colorDepth==CL16Look)
        {
            cout << "Found a dual plane attribute texture (no backface culling)\n";
            t->SGL_ATTR=ATTRIBUTE(Dual_Plane, SORT_MIN, No_Texture, t->clut[0], CL32KRGB|No_Gouraud, CL32KRGB| MESHoff | Window_In, sprPolygon, 0|UseNearClip);
        }
        else if ((findWhite == 0) && (t->clut[1]==0 && t->clut[0]!=0)&& colorDepth==CL16Look)
        {
            cout << "Found a quad with SORT_MIN flag.\n";
            t->SGL_ATTR=ATTRIBUTE(Single_Plane, SORT_MIN, No_Texture, t->clut[0], CL32KRGB|No_Gouraud, CL32KRGB| MESHoff | Window_In, sprPolygon, 0|UseNearClip);
        }
        else if (t->clut[0]==0)
        {
           t->SGL_ATTR = ATTRIBUTE(Dual_Plane,SORT_CEN, i, LUTidx(i), No_Gouraud,Window_In|MESHoff|HSSon|ECdis|SPenb|colorDepth,sprNoflip,0|UseNearClip);
        }
        else
        {
            if (t->clut[1]==0 && t->clut[0]!=0)
                t->SGL_ATTR=ATTRIBUTE(Single_Plane, SORT_CEN, No_Texture, t->clut[0], CL32KRGB|No_Gouraud, CL32KRGB| MESHoff | Window_In, sprPolygon, 0|UseNearClip);
            else
                t->SGL_ATTR=ATTRIBUTE(Single_Plane,SORT_CEN, i, LUTidx(i), No_Gouraud,Window_In|MESHoff|HSSon|ECdis|SPdis|colorDepth,sprNoflip,0|UseNearClip);
        }

        if ((t->clut[0] != 0) && (t->clut[1]!=0)) {
            uint32_t color[3] = {0,0,0};
            uint32_t cnt = t->width*t->height;
            for (uint32_t i=0; i<cnt; i++) {
                color[0]+=t->pixel[i].r;
                color[1]+=t->pixel[i].g;
                color[2]+=t->pixel[i].b;
            }
            if (cnt != 0 ){
                for (uint32_t i=0; i<3; i++) {
                    color[i] /= cnt;
                    color[i] = color[i]/8;
                }
            }
            t->SGL_ATTR.colno=C_RGB((uint16_t)color[0], (uint16_t)color[1], (uint16_t)color[2]);
            t->SGL_ATTR.atrb |= CL_Gouraud;
        }
    }

    ///???????? What's that crap??*/
    if (!useUV){
        for (unsigned int i=0; i<aModel->nbModels; i++){
                model_t * m = &aModel->model[i];

            for (unsigned int ii=0; ii<m->nbPolygon; ii++){
                polygon_t * p = &m->pltbl[ii];
                t = &aModel->texture[p->texture];
                //p->sorting = t->SGL_ATTR.sorting;
                p->SGL_ATTR = t->SGL_ATTR;
            }
        }
    }
}

#if 0
bool used_textures(animated_model_t * aModel, int textId)
{
    for (unsigned int mesh=0; mesh<aModel->nbModels; mesh++)
    {
        model_t * m = &aModel->model[mesh];
        for (unsigned int i=0; i< m->nbPolygon; i++)
        {
            if (LOD_mesh[mesh].attbl[i].texno == textID)
                return true;
        }
    }
    return false;
}

void delete_unused_texture(unsigned int textID)
{

    for (unsigned int mesh=0; mesh<MAP_DATA.TOTAL_MESH; mesh++)
    {
       // cout << "\n" << mesh << " ";
        for (unsigned int i=0; i< LOD_mesh[mesh].nbPolygon; i++)
        {
            //cout << i << ", ";
            if (LOD_mesh[mesh].attbl[i].texno > textID)
                LOD_mesh[mesh].attbl[i].texno--;
        }
        for (unsigned int i=0; i< LOD_mesh2[mesh].nbPolygon; i++)
        {
            //cout << i << ", ";
            if (LOD_mesh2[mesh].attbl[i].texno > textID)
                LOD_mesh2[mesh].attbl[i].texno--;
            LOD_mesh2[mesh].attbl[i].extraInfo=0;
        }
    }

//cout << "Deleting2 " << textID << ", " << tmpTotalLODtxt-1 << "\n";
    for (int i=textID; i<tmpTotalLODtxt-1; i++)
    {

        img[i].height=img[i+1].height;
        img[i].width=img[i+1].width;
        img[i].colorDepth=img[i+1].colorDepth;
        //cout << "W:" << img[i].width << ", H:" << img[i].height << ", " << TextureArray[i] << "\n";
        if (img[i].pixel) delete [] img[i].pixel;
        img[i].pixel = new pixel_t[MAX(img[i].width*img[i].height, 8)];
        for (unsigned int ii=0; ii< MAX(img[i].width*img[i].height, 8); ii++)
        {
            img[i].pixel[ii].color=img[i+1].pixel[ii].color;
        }
        TextureArray[i]=TextureArray[i+1];
        TextureSpecialAttribute[i]=TextureSpecialAttribute[i+1];
    }
 //   cout << "Deleting3\n";
    delete [] img[tmpTotalLODtxt].pixel;

    tmpTotalLODtxt--;
}
#endif // 0


void findScale(animated_model_t * aModel, bool allowUpscale){
    if (!aModel) return;
    for (int j=0; j<3; j++){
       aModel->scale[j]=(float)1.0f;
    }

    if (USE_SGL) return; //Not supported for now...

    double scale[3]={1.0f, 1.0f, 1.0f};
    double maxValue[3]={0,0,0};
    uint64_t totalPoints = 0;

    for (uint32_t i=0; i<aModel->nbModels; i++) {
        totalPoints += aModel->model[i].nbPoint;
        for (uint32_t ii=0; ii<aModel->model[i].nbPoint; ii++){
           for (uint32_t j=0; j<3; j++){
                maxValue[j]=MAX(maxValue[j], fabs(aModel->model[i].pntbl[ii].point[j]));
           }
        }
    }
    for (uint32_t i=0; i<aModel->nbFrames; i++){
        if (!aModel->keyFrames || !aModel->keyFrames[i].cVert) continue;
        vertex_t * v = aModel->keyFrames[i].cVert;
        for (uint64_t j=0; j<totalPoints; j++, v++){
            for (uint32_t jj=0; jj<3; jj++){
              maxValue[jj]=MAX(maxValue[jj], fabs(v->point[jj]));
            }
        }
    }

    for (int i=0; i<3; i++){
        scale[i]=1.0f;
        //maxValue[i] = MAX(fabs(aModel->BoxMax[i]),fabs(aModel->BoxMin[i]));
        if (maxValue[i]>=126.5f){
            scale[i]=maxValue[i]/126.5f;
        }
        else if (allowUpscale && maxValue[i] > 0.0f){
            scale[i]=MAX(maxValue[i]/126.5, 1.0/65536.0);
        }
    }

    double invScale[3]={1.0f/scale[0], 1.0f/scale[1], 1.0f/scale[2]};

    for (int j=0; j<3; j++){
       aModel->scale[j]=(float)scale[j];
    }

    for (uint32_t i=0; i<aModel->nbModels; i++) {
        for (uint32_t ii=0; ii<aModel->model[i].nbPoint; ii++){
           for (uint32_t j=0; j<3; j++){
                aModel->model[i].pntbl[ii].point[j]*=invScale[j];
           }
        }
    }
    for (uint32_t i=0; i<aModel->nbFrames; i++){
        if (!aModel->keyFrames || !aModel->keyFrames[i].cVert) continue;
        vertex_t * v = aModel->keyFrames[i].cVert;
        for (uint64_t j=0; j<totalPoints; j++, v++){
            for (uint32_t jj=0; jj<3; jj++){
              v->point[jj]*=invScale[jj];
            }
        }
    }
}

string fileOutName;
static void rotateQuarterTurns(float * vector, int axis, int quarterTurns)
{
    quarterTurns = ((quarterTurns % 4) + 4) % 4;
    for (int turn = 0; turn < quarterTurns; ++turn)
    {
        const float first = vector[axis == X ? Y : X];
        const float second = vector[axis == Z ? Y : Z];
        if (axis == X)
        {
            vector[Y] = -vector[Z];
            vector[Z] = first;
        }
        else if (axis == Y)
        {
            vector[X] = second;
            vector[Z] = -first;
        }
        else
        {
            vector[X] = -second;
            vector[Y] = first;
        }
    }
}

void rotateModelData(animated_model_t * model, int rotationX, int rotationY, int rotationZ)
{
    const int rotations[3] = {rotationX, rotationY, rotationZ};
    unsigned int frameOffset = 0;
    for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
    {
        model_t & mesh = model->model[meshIndex];
        for (unsigned int point = 0; point < mesh.nbPoint; ++point)
        {
            for (int axis = X; axis <= Z; ++axis)
            {
                rotateQuarterTurns(mesh.pntbl[point].point, axis, rotations[axis]);
                rotateQuarterTurns(mesh.pntbl[point].normal, axis, rotations[axis]);
            }
        }
        for (unsigned int face = 0; face < mesh.nbPolygon; ++face)
            for (int axis = X; axis <= Z; ++axis)
                rotateQuarterTurns(mesh.pltbl[face].normal, axis, rotations[axis]);

        for (unsigned int frame = 0; frame < model->nbFrames; ++frame)
            for (unsigned int point = 0; point < mesh.nbPoint; ++point)
                for (int axis = X; axis <= Z; ++axis)
                {
                    vertex_t & vertex = model->keyFrames[frame].cVert[frameOffset + point];
                    rotateQuarterTurns(vertex.point, axis, rotations[axis]);
                    rotateQuarterTurns(vertex.normal, axis, rotations[axis]);
                }
        frameOffset += mesh.nbPoint;
    }
}

void translateModelData(animated_model_t * model, float translationX, float translationY, float translationZ)
{
    const float translation[3] = {translationX, translationY, translationZ};
    const float largest = std::numeric_limits<float>::max();
    for (int axis = X; axis <= Z; ++axis)
    {
        model->BoxMin[axis] = largest;
        model->BoxMax[axis] = -largest;
    }

    unsigned int frameOffset = 0;
    for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
    {
        model_t & mesh = model->model[meshIndex];
        for (int axis = X; axis <= Z; ++axis)
        {
            mesh.BoxMin[axis] = largest;
            mesh.BoxMax[axis] = -largest;
        }
        for (unsigned int point = 0; point < mesh.nbPoint; ++point)
        {
            vertex_t & vertex = mesh.pntbl[point];
            for (int axis = X; axis <= Z; ++axis)
            {
                vertex.point[axis] += translation[axis];
                mesh.BoxMin[axis] = MIN(mesh.BoxMin[axis], vertex.point[axis]);
                mesh.BoxMax[axis] = MAX(mesh.BoxMax[axis], vertex.point[axis]);
                model->BoxMin[axis] = MIN(model->BoxMin[axis], vertex.point[axis]);
                model->BoxMax[axis] = MAX(model->BoxMax[axis], vertex.point[axis]);
            }
        }
        for (unsigned int frame = 0; frame < model->nbFrames; ++frame)
            for (unsigned int point = 0; point < mesh.nbPoint; ++point)
            {
                vertex_t & vertex = model->keyFrames[frame].cVert[frameOffset + point];
                for (int axis = X; axis <= Z; ++axis)
                    vertex.point[axis] += translation[axis];
            }
        frameOffset += mesh.nbPoint;
    }
}

void LoadOBJ(string inFolder, string fileIn, float * scalefactor, animated_model_t * aModel,
             int rotationX, int rotationY, int rotationZ,
             float translationX, float translationY, float translationZ, bool readOnly = false)
{
    cout << "\nNow loading : " << fileIn << "\n";
    string fileInName = inFolder + fileIn + ".obj";


    ifstream file(fileInName, ios::in | ios::ate);
    if (!file.is_open()) {
            cout << "ERROR READING OBJ FILE : " << fileInName << "\n";
            if (readOnly) return;
            exit(-1);
    }

    aModel->nbTextures=0;
    aModel->texture = new texture_t[2048]();
    const ztp_mtl::MaterialTextures materials = ztp_mtl::load(file, fileInName, cout);

   // aModel->model[0].pntbl = new vertex_t[256];
   // aModel->model[0].pltbl = new polygon_t[256];
    cout << "Preparing to open OBJ file...\n";
    if (load_OBJ_to_mesh(&file, scalefactor, aModel, -1)) {cout << "\nSuccess : " << fileIn << " loaded...\n";}
    else {cout << "\nERROR : Couldn't load Obj\n"; return;}
    file.close();

    extern int ReadTGAFile (string folder, texture_t * texture, string sourcePath = "");
    const auto loadMaterialTexture = [&](texture_t * texture)
    {
        const auto material = materials.find(texture->name);
        return ReadTGAFile(inFolder, texture, material == materials.end() ? "" : material->second);
    };
    if (useUV) {
        std::vector<texture_t> sourceTextures(aModel->texture, aModel->texture + aModel->nbTextures);
        for (unsigned int sourceIndex = 0; sourceIndex < sourceTextures.size(); ++sourceIndex)
            loadMaterialTexture(&sourceTextures[sourceIndex]);

        extern void createFakeTexture(texture_t *);
        bool needsFallback = false;
        for (unsigned int modelIndex = 0; modelIndex < aModel->nbModels; ++modelIndex)
            for (unsigned int faceIndex = 0; faceIndex < aModel->model[modelIndex].nbPolygon; ++faceIndex)
            {
                int materialIndex = aModel->model[modelIndex].pltbl[faceIndex].texture;
                if (materialIndex < 0 || materialIndex >= static_cast<int>(sourceTextures.size())) needsFallback = true;
            }
        const unsigned int fallbackIndex = static_cast<unsigned int>(sourceTextures.size());
        if (needsFallback)
        {
            sourceTextures.push_back(texture_t());
            sourceTextures.back().name = "None";
            createFakeTexture(&sourceTextures.back());
        }
        cacheAutomaticLodSourceTextures(aModel, sourceTextures.data(), sourceTextures.size());
        aModel->nbTextures=0;

        polygon_t * pol;
        for (uint32_t i=0; i<aModel->nbModels; i++)  {//Ici faut une boucle pour cr�er les nouvelles textures
            for (uint32_t ii=0; ii<aModel->model[i].nbPolygon; ii++){
                pol=&aModel->model[i].pltbl[ii];
                int materialIndex = pol->texture;
                const texture_t & sourceTexture = sourceTextures[
                    materialIndex >= 0 && materialIndex < static_cast<int>(fallbackIndex) ?
                    static_cast<unsigned int>(materialIndex) : fallbackIndex];
                createTexture(pol, aModel, sourceTexture);
            }
        }
        for (unsigned int sourceIndex = 0; sourceIndex < sourceTextures.size(); ++sourceIndex)
            delete [] sourceTextures[sourceIndex].pixel;
    }
    else {
        for (unsigned int i=0; i<aModel->nbTextures; i++)    {loadMaterialTexture(&aModel->texture[i]);}
    }



    if (USE_SGL==2 && !readOnly) {
            cout << USE_SGL << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n0";
        aModel->texture[aModel->nbTextures].name = "PALETTE";
        ReadTGAFile(inFolder, &aModel->texture[aModel->nbTextures]);
        extern void writePalette(animated_model_t * a);
        writePalette(aModel);
    }


#if 0
float dist[4]={0,0,0,0};
float v[3];
uint32_t cnt=0;
vertex_t * v1, * v2;
model_t * m = &aModel->model[0];
for (uint32_t i=0; i<m->nbPolygon; i++){
    polygon_t * p = &m->pltbl[i];
    for (uint32_t ii=0; ii<4; ii++) {
        cnt=0;
        if (p->vertIdx[ii&3]==p->vertIdx[(ii+1)&3]) {
            dist[ii]=0;
            continue; //If the vertices are the same, skip
        }
        cnt++;
        v1=&m->pntbl[p->vertIdx[ii&3]];
        v2=&m->pntbl[p->vertIdx[(ii+1)&3]];
        for (uint32_t j=0; j<3; j++) {
            v[j]=v1->point[j]-v2->point[j]; }
        dist[ii]=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    }
    float newDist = 0;
    for (uint32_t j=0; j<4; j++)
        { newDist+=dist[j];}
    if (cnt!=0){
        newDist/= (float)cnt;
        if (newDist > 10.0* (float)scalefactor[X])
            p->isMask=1;
    }
}
#endif // 0
#if 0

static bool loadFBX(const string &filePath, float *scaleFactor, animated_model_t *model,
                    int rotationX, int rotationY, int rotationZ,
                    float translationX, float translationY, float translationZ)
{
    ufbx_load_opts loadOptions = {};
    loadOptions.load_external_files = true;
    loadOptions.ignore_missing_external_files = true;
    ufbx_error error = {};
    ufbx_scene *scene = ufbx_load_file(filePath.c_str(), &loadOptions, &error);
    if (!scene)
    {
        char errorText[512] = {};
        ufbx_format_error(errorText, sizeof(errorText), &error);
        cerr << "ERROR READING FBX FILE: " << filePath << "\n" << errorText << "\n";
        return false;
    }

    const ufbx_anim_stack *stack = scene->anim_stacks.count ? scene->anim_stacks.data[0] : NULL;
    const ufbx_anim *animation = stack ? stack->anim : scene->anim;
    const double timeBegin = stack ? stack->time_begin : 0.0;
    const double timeEnd = stack ? stack->time_end : 0.0;
    const unsigned int frameCount = MAX(1u, model->nbFrames);
    model->nbFrames = frameCount;

    ufbx_evaluate_opts evaluateOptions = {};
    evaluateOptions.evaluate_skinning = true;
    evaluateOptions.evaluate_caches = true;

    const double firstTime = timeBegin;
    ufbx_scene *firstPose = ufbx_evaluate_scene(scene, animation, firstTime, &evaluateOptions, &error);
    if (!firstPose)
    {
        ufbx_free_scene(scene);
        return false;
    }

    std::vector<size_t> meshNodes;
    size_t totalPoints = 0;
    for (size_t nodeIndex = 0; nodeIndex < firstPose->nodes.count; ++nodeIndex)
    {
        const ufbx_node *node = firstPose->nodes.data[nodeIndex];
        if (!node->mesh || !node->visible) continue;
        if (model->nbModels >= 256 || node->mesh->num_vertices > UINT_MAX - totalPoints)
        {
            ufbx_free_scene(firstPose);
            ufbx_free_scene(scene);
            return false;
        }
        meshNodes.push_back(nodeIndex);
        totalPoints += node->mesh->num_vertices;
    }
    if (meshNodes.empty() || !totalPoints || uint64_t(totalPoints) * frameCount > 4u * 1024u * 1024u)
    {
        ufbx_free_scene(firstPose);
        ufbx_free_scene(scene);
        return false;
    }

    model->nbModels = static_cast<unsigned int>(meshNodes.size());
    model->nbTextures = 0;
    model->texture = new texture_t[1]();
    size_t pointOffset = 0;
    std::vector<std::vector<uint32_t> > faceTopology(meshNodes.size());
    for (size_t meshIndex = 0; meshIndex < meshNodes.size(); ++meshIndex)
    {
        const ufbx_node *node = firstPose->nodes.data[meshNodes[meshIndex]];
        const ufbx_mesh *mesh = node->mesh;
        model_t &outMesh = model->model[meshIndex];
        outMesh.nbPoint = static_cast<unsigned int>(mesh->num_vertices);
        outMesh.pntbl = new vertex_t[outMesh.nbPoint]();
        outMesh.pltbl = new polygon_t[mesh->num_triangles]();
        outMesh.nbPolygon = static_cast<unsigned int>(mesh->num_triangles);
        strncpy(outMesh.name, node->name.data, sizeof(outMesh.name) - 1);

        size_t polygonIndex = 0;
        std::vector<uint32_t> &topology = faceTopology[meshIndex];
        for (size_t faceIndex = 0; faceIndex < mesh->num_faces; ++faceIndex)
        {
            const ufbx_face face = mesh->faces.data[faceIndex];
            std::vector<uint32_t> triangles(static_cast<size_t>(face.num_indices > 2 ? face.num_indices - 2 : 0) * 3);
            const size_t triangleCount = ufbx_triangulate_face(triangles.data(), triangles.size(), mesh, face);
            if (triangleCount != face.num_indices - 2)
            {
                ufbx_free_scene(firstPose);
                ufbx_free_scene(scene);
                return false;
            }
            for (size_t triangle = 0; triangle < triangleCount; ++triangle)
            {
                polygon_t &polygon = outMesh.pltbl[polygonIndex++];
                polygon_t defaults = {};
                polygon = defaults;
                polygon.texture = -1;
                polygon.SGL_ATTR.atrb = CL_Gouraud;
                for (unsigned int corner = 0; corner < 3; ++corner)
                {
                    const uint32_t meshCorner = triangles[triangle * 3 + corner];
                    if (meshCorner >= mesh->vertex_indices.count) return false;
                    const uint32_t pointIndex = mesh->vertex_indices.data[meshCorner];
                    if (pointIndex >= mesh->num_vertices) return false;
                    polygon.vertIdx[corner] = pointIndex;
                    topology.push_back(pointIndex);
                    if (mesh->vertex_uv.exists)
                    {
                        const ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, meshCorner);
                        polygon.vertUV[corner].U = static_cast<float>(uv.x);
                        polygon.vertUV[corner].V = static_cast<float>(uv.y);
                    }
                }
                polygon.vertIdx[3] = polygon.vertIdx[2];
                polygon.vertUV[3] = polygon.vertUV[2];
            }
        }
        if (polygonIndex != outMesh.nbPolygon)
        {
            ufbx_free_scene(firstPose);
            ufbx_free_scene(scene);
            return false;
        }

        for (size_t pointIndex = 0; pointIndex < mesh->num_vertices; ++pointIndex)
        {
            vertex_t &vertex = outMesh.pntbl[pointIndex];
            const uint32_t firstCorner = mesh->vertex_first_index.data[pointIndex];
            ufbx_vec3 position = mesh->vertices.data[pointIndex];
            if (firstCorner != UFBX_NO_INDEX && mesh->skinned_position.exists)
                position = ufbx_get_vertex_vec3(&mesh->skinned_position, firstCorner);
            if (mesh->skinned_is_local) position = ufbx_transform_position(&node->geometry_to_world, position);
            vertex.point[0] = static_cast<float>(position.x) * scaleFactor[0];
            vertex.point[1] = static_cast<float>(position.y) * scaleFactor[1];
            vertex.point[2] = static_cast<float>(position.z) * scaleFactor[2];
        }
        pointOffset += mesh->num_vertices;
    }

    for (unsigned int frame = 0; frame < frameCount; ++frame)
    {
        const double time = frameCount > 1 ? timeBegin + (timeEnd - timeBegin) * frame / (frameCount - 1) : timeBegin;
        ufbx_scene *pose = frame == 0 ? firstPose :
            ufbx_evaluate_scene(scene, animation, time, &evaluateOptions, &error);
        if (!pose)
        {
            ufbx_free_scene(firstPose);
            ufbx_free_scene(scene);
            return false;
        }
        model->keyFrames[frame].cVert = new vertex_t[totalPoints]();
        size_t frameOffset = 0;
        for (size_t meshIndex = 0; meshIndex < meshNodes.size(); ++meshIndex)
        {
            const ufbx_node *node = pose->nodes.data[meshNodes[meshIndex]];
            const ufbx_mesh *mesh = node->mesh;
            const ufbx_mesh *baseMesh = firstPose->nodes.data[meshNodes[meshIndex]]->mesh;
            if (!mesh || mesh->num_vertices != baseMesh->num_vertices || mesh->num_faces != baseMesh->num_faces ||
                mesh->vertex_indices.count != baseMesh->vertex_indices.count)
            {
                if (pose != firstPose) ufbx_free_scene(pose);
                ufbx_free_scene(firstPose);
                ufbx_free_scene(scene);
                return false;
            }
            for (size_t index = 0; index < mesh->vertex_indices.count; ++index)
                if (mesh->vertex_indices.data[index] != baseMesh->vertex_indices.data[index]) return false;
            for (size_t faceIndex = 0; faceIndex < mesh->num_faces; ++faceIndex)
            {
                if (mesh->faces.data[faceIndex].index_begin != baseMesh->faces.data[faceIndex].index_begin ||
                    mesh->faces.data[faceIndex].num_indices != baseMesh->faces.data[faceIndex].num_indices)
                    return false;
            }
            const model_t &baseMeshData = model->model[meshIndex];
            for (size_t pointIndex = 0; pointIndex < mesh->num_vertices; ++pointIndex)
            {
                vertex_t &vertex = model->keyFrames[frame].cVert[frameOffset + pointIndex];
                vertex = baseMeshData.pntbl[pointIndex];
                const uint32_t firstCorner = mesh->vertex_first_index.data[pointIndex];
                ufbx_vec3 position = mesh->vertices.data[pointIndex];
                if (firstCorner != UFBX_NO_INDEX && mesh->skinned_position.exists)
                    position = ufbx_get_vertex_vec3(&mesh->skinned_position, firstCorner);
                if (mesh->skinned_is_local) position = ufbx_transform_position(&node->geometry_to_world, position);
                vertex.point[0] = static_cast<float>(position.x) * scaleFactor[0];
                vertex.point[1] = static_cast<float>(position.y) * scaleFactor[1];
                vertex.point[2] = static_cast<float>(position.z) * scaleFactor[2];
            }
            frameOffset += mesh->num_vertices;
        }
        if (pose != firstPose) ufbx_free_scene(pose);
    }

    extern void recalculateNormals(model_t *);
    extern void pointNormals(model_t *);
    for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
    {
        recalculateNormals(&model->model[meshIndex]);
        pointNormals(&model->model[meshIndex]);
    }
    rotateModelData(model, rotationX, rotationY, rotationZ);
    translateModelData(model, translationX, translationY, translationZ);
    calcLight(model);
    findScale(model, false);

    ufbx_free_scene(firstPose);
    ufbx_free_scene(scene);
    return true;
}
#endif // 0
    //if (aModel->nbFrames==0) reOrgModel(model);
    uint32_t totVertices = 0;
    for (uint32_t i=0; i<aModel->nbModels; i++) {
        totVertices+=aModel->model[i].nbPoint;
    }

    string animationPrefix;
    string animationSeparator;
    unsigned int animationDigits = 0;
    bool baseIsFirstAnimationFrame = false;
    if (fileIn.size() >= 7 && fileIn.compare(fileIn.size() - 7, 7, "_000001") == 0)
    {
        animationPrefix = fileIn.substr(0, fileIn.size() - 7);
        animationSeparator = "_";
        animationDigits = 6;
        baseIsFirstAnimationFrame = true;
    }
    else if (fileIn.size() >= 6 && fileIn.compare(fileIn.size() - 6, 6, "000001") == 0)
    {
        animationPrefix = fileIn.substr(0, fileIn.size() - 6);
        animationDigits = 6;
        baseIsFirstAnimationFrame = true;
    }
    else if (fileIn.size() >= 4 && fileIn.compare(fileIn.size() - 4, 4, "0000") == 0)
    {
        animationPrefix = fileIn.substr(0, fileIn.size() - 4);
        animationDigits = 4;
        baseIsFirstAnimationFrame = true;
    }

    for (uint32_t i=0; i<aModel->nbFrames; i++)
    {
        aModel->keyFrames[i].cVert = new vertex_t [totVertices];
        stringstream frameNumber;
        frameNumber << std::setw(6) << std::setfill('0') << i + 1;
        string postVal = frameNumber.str();
        string animName;
        if (baseIsFirstAnimationFrame)
        {
            if (i == 0) animName = fileInName;
            else
            {
                stringstream frameNumber;
                frameNumber << std::setw(animationDigits) << std::setfill('0')
                            << (animationDigits == 4 ? i : i + 1);
                animName = inFolder + animationPrefix + animationSeparator + frameNumber.str() + ".obj";
            }
        }
        else
        {
            animName = inFolder + fileIn + "_" + postVal + ".obj";
        }
        cout << animName << "\n\n****************\n";
        ifstream animFile(animName, ios::in | ios::ate);
        if (!animFile.is_open() && !baseIsFirstAnimationFrame)
        {
            animName = inFolder + fileIn + postVal + ".obj";
            animFile.clear();
            animFile.open(animName, ios::in | ios::ate);
        }
        if (!animFile.is_open() && !baseIsFirstAnimationFrame)
        {
            stringstream zeroBasedFrameNumber;
            zeroBasedFrameNumber << std::setw(4) << std::setfill('0') << i;
            animName = inFolder + fileIn + zeroBasedFrameNumber.str() + ".obj";
            animFile.clear();
            animFile.open(animName, ios::in | ios::ate);
        }
        if (!animFile.is_open()) {
            cout << "ERROR READING OBJ FILE\n";
            delete [] aModel->keyFrames[i].cVert;
            aModel->keyFrames[i].cVert = NULL;
            aModel->nbFrames = i;
            break;
        }
        if (load_OBJ_to_mesh(&animFile, scalefactor, aModel, i)) {cout << "\nKeyFrame No. " << i << " loaded...\n";}
        else {
            cout << "\nERROR : Couldn't load keyframe\n";
            delete [] aModel->keyFrames[i].cVert;
            aModel->keyFrames[i].cVert = NULL;
            aModel->nbFrames = i;
            break;
        }
        animFile.close();
    }


    rotateModelData(aModel, rotationX, rotationY, rotationZ);
    translateModelData(aModel, translationX, translationY, translationZ);

    if (!useUV)    specialConditions(0, aModel->nbTextures, aModel);
    for (unsigned int modelIndex = 0; modelIndex < aModel->nbModels; ++modelIndex)
    {
        for (unsigned int polygonIndex = 0; polygonIndex < aModel->model[modelIndex].nbPolygon; ++polygonIndex)
            aModel->model[modelIndex].pltbl[polygonIndex].SGL_ATTR.atrb |= CL_Gouraud;
    }

    cout << "Calculating lighting...\n";
    extern void calcLight(animated_model_t*);
    calcLight(aModel);





    findScale(aModel, true);
    if (rotationX || rotationY || rotationZ)
    {
        extern void recalculateNormals(model_t *);
        extern void pointNormals(model_t *);
        vertex_t * basePoints[256] = {};
        uint32_t frameOffset = 0;
        for (uint32_t meshIndex = 0; meshIndex < aModel->nbModels; ++meshIndex)
        {
            model_t & mesh = aModel->model[meshIndex];
            basePoints[meshIndex] = mesh.pntbl;
            for (uint32_t frame = 0; frame < aModel->nbFrames; ++frame)
            {
                mesh.pntbl = &aModel->keyFrames[frame].cVert[frameOffset];
                recalculateNormals(&mesh);
                pointNormals(&mesh);
            }
            mesh.pntbl = basePoints[meshIndex];
            recalculateNormals(&mesh);
            pointNormals(&mesh);
            frameOffset += mesh.nbPoint;
        }
        calcLight(aModel);
    }


#if 0
string fileOutName = "OUT/" + fileIn;
    cout << "NOW CREATING BINARY FILE " << fileOutName << ".ZTP\n";
    string binaryFileName = fileOutName + ".ZTP";  //Z-Treme character

    ofstream binFile(binaryFileName.c_str(), ios::out | ios::binary);
    if (!binFile.is_open()) {cout << "\nERROR WHILE WRITING BINARY FILE!\n"; return;}
    write_model_binary(&binFile, aModel);

    cout << "Writing debug text file...\n";
    extern bool write_MOD_HEADER(string filename, animated_model_t * aModel);
    write_MOD_HEADER("OUT.TXT", aModel);
    binFile.close();
#else
if (!readOnly) fileOutName = /*"OUT/" +*/ fileIn;
#endif // 0    void write_model_binary(ofstream * file, animated_model_t * aModel);



}




animated_model_t * loadingConfigured(unsigned int frameCount, int useSgl, float scale,
                                     uint32_t useUv, uint32_t specialOptions,
                                     string path, string fileName, bool readOnly,
                                     int rotationX, int rotationY, int rotationZ);
animated_model_t * loadingConfigured(unsigned int frameCount, int useSgl, float scale,
                                     uint32_t useUv, uint32_t specialOptions,
                                     string path, string fileName, bool readOnly,
                                     int rotationX, int rotationY, int rotationZ,
                                     float translationX, float translationY, float translationZ);
animated_model_t * loadingConfigured(unsigned int frameCount, int useSgl, float scale,
                                     uint32_t useUv, uint32_t specialOptions,
                                     string path, string fileName, bool readOnly,
                                     int rotationX, int rotationY, int rotationZ,
                                     float translationX, float translationY, float translationZ,
                                     uint32_t maxTextureWidth, uint32_t maxTextureHeight);

animated_model_t * loadingConfigured(unsigned int frameCount, int useSgl, float scale,
                                     uint32_t useUv, uint32_t specialOptions,
                                     string path, string fileName, bool readOnly = false)
{
    return loadingConfigured(frameCount, useSgl, scale, useUv, specialOptions,
                              path, fileName, readOnly, 0, 0, 0);
}

animated_model_t * loadingConfigured(unsigned int frameCount, int useSgl, float scale,
                                     uint32_t useUv, uint32_t specialOptions,
                                     string path, string fileName, bool readOnly,
                                     int rotationX, int rotationY, int rotationZ)
{
    return loadingConfigured(frameCount, useSgl, scale, useUv, specialOptions, path, fileName,
                              readOnly, rotationX, rotationY, rotationZ, 0.0f, 0.0f, 0.0f);
}

animated_model_t * loadingConfigured(unsigned int frameCount, int useSgl, float scale,
                                     uint32_t useUv, uint32_t specialOptions,
                                     string path, string fileName, bool readOnly,
                                     int rotationX, int rotationY, int rotationZ,
                                     float translationX, float translationY, float translationZ)
{
    return loadingConfigured(frameCount, useSgl, scale, useUv, specialOptions, path, fileName,
        readOnly, rotationX, rotationY, rotationZ, translationX, translationY, translationZ, 1024, 256);
}

animated_model_t * loadingConfigured(unsigned int frameCount, int useSgl, float scale,
                                     uint32_t useUv, uint32_t specialOptions,
                                     string path, string fileName, bool readOnly,
                                     int rotationX, int rotationY, int rotationZ,
                                     float translationX, float translationY, float translationZ,
                                     uint32_t maxTextureWidth, uint32_t maxTextureHeight)
{
    string extension = fileName.size() >= 4 ? fileName.substr(fileName.size() - 4) : "";
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    const bool isFbx = extension == ".fbx";
    if (!isFbx && frameCount > ZTP_MAX_ANIMATION_FRAMES)
    {
        cout << "Animation frame count exceeds " << ZTP_MAX_ANIMATION_FRAMES
             << "; loading only the first " << ZTP_MAX_ANIMATION_FRAMES << " frames.\n";
        frameCount = ZTP_MAX_ANIMATION_FRAMES;
    }
    if (!isFbx && !frameCount) frameCount = 1;
    USE_SGL = useSgl;
    useUV = useUv >= 1 ? 1 : 0;
    specialOption = specialOptions;
    maxGeneratedTextureWidth = MAX(8u, MIN(maxTextureWidth, 65535u));
    maxGeneratedTextureHeight = MAX(1u, MIN(maxTextureHeight, 256u));

    float scaleFactor[XYZ] = {scale, scale, scale};
    animated_model_t * newModel = new animated_model_t;
    ZeroMemory(newModel, sizeof(animated_model_t));
    newModel->nbFrames = isFbx ? 0 : frameCount;
    newModel->sourceSgl = useSgl == 1 ? 1 : 0;
    newModel->framerate = 0;
    newModel->nbModels = 0;
    newModel->model = new model_t[256];
    ZeroMemory(newModel->model, sizeof(model_t) * 256);
    newModel->keyFrames = isFbx ? NULL : new key_frame_t[newModel->nbFrames]();
    fbxLoadError.clear();
    if (extension == ".fbx")
    {
        if (!loadFBX(path + fileName, scaleFactor, newModel, rotationX, rotationY, rotationZ,
                     translationX, translationY, translationZ))
        {
            cout << fbxLoadError << "\n";
            destroyLoadedZTP(newModel);
            return NULL;
        }
        fileOutName = fileName.substr(0, fileName.size() - 4);
    }
    else
        LoadOBJ(path, fileName, scaleFactor, newModel, rotationX, rotationY, rotationZ,
            translationX, translationY, translationZ, readOnly);
    return newModel;
}

animated_model_t * loading(string line)
{

    stringstream classType(line);            string bufferStr;
    getline(classType, bufferStr, ' ');
    string c[32]; //Stupidly high for no real reasons, could be smarter
    c[0]=bufferStr;
    int tot=1;
    while (classType>>c[tot]){
        tot++;
    }

    string path;
    string fileName;
    float scale;
    unsigned int frameCount;

    cout << "SEGA SATURN Z-TREME ENGINE MODEL CONVERTER 0.45\n";
    cout << "BY XL2 (2022-02-02)\n\n";
    cout << "New since v.0.4 : Supports texture coordinates!\nSupports a maximum of 256 vertices/faces per model for the non-SGL version\n";

    cout << c[0] << "\n";
    if (c[0] != "") {
        frameCount = static_cast<unsigned int>(std::atoi(c[0].c_str()));
    } else {
        cout << "\n***STEP 1***";
        cout << "\n   Enter how many keyframes your model has\n";
        cout <<"   Example : for 6 keyframes to interpolate your animations, enter 6.\n   0 for no animation.\n\n";
        cout << "ENTER TOTAL KEYFRAMES : ";
        cin >> frameCount;
    }


    if (c[1]!="") {
        USE_SGL = std::atoi(c[1].c_str());
    } else {
        cout << "\n***STEP 2***\n   Will you be using SGL? '0' for NO, '1' for YES. Default : '1'\nUSE SGL : ";
        cin >> USE_SGL;
    }
    if (USE_SGL != 0 && USE_SGL != 2) USE_SGL=1;

    if (c[2]!="") {
        scale = std::atof(c[2].c_str());
    } else {
        cout << "\n***STEP 3***\n   You will now have to enter the scale value and press enter. \n   Example : 64, 128, 100 or any integer value.\n\nENTER SCALE : ";
        cin >> scale;
    }
    if (scale==0.0f) scale=1.0f;
    if (c[3]!=""){
        useUV=std::atoi(c[3].c_str());
    } else {
        cout << "\n***STEP 4***\n   Do you want to use UV coordinates to generate Saturn textures for each material?\n   Example : 0 for no, 1 for yes.\n\nUSE UV coordinates : ";
        cin >> useUV;
    }
    if (useUV >= 1) useUV = 1;

    if (c[4]!=""){
        specialOption=std::atoi(c[4].c_str());
    } else {
        cout << "\n\n***STEP 5***\n   Put your option flags (bits). Bit 0 = Force 4 BPP. Bit 1 = Force power of 8 height textures for UV. (Default = 0)\n\nSPECIAL OPTION : ";
        cin >> specialOption;
    }

    if (c[5]!="") {
        path = c[5];
    } else {
        cout << "\n***STEP 6***\n   You will now have to enter the folder's relative path\n   IMPORTANT : Don't forget to end it with a '/'.\n   Example : IN/TITLE/  and then press enter.\n\nENTER THE PATH : ";
        cin >> path;
    }


    if (c[6]!=""){
        fileName = c[6];
    } else {
        cout << "\n\n***STEP 7***\n\n   You will now have to enter the OBJ filename WITHOUT the extension.\n   Example : TITLE and then press enter.\n\nENTER THE NAME : ";
        cin >> fileName;
    }


    if (c[7]!=""){
        extern BOOL bQuit;
        bQuit = std::atoi(c[7].c_str())&1;
    }

    const uint32_t maxTextureWidth = c[8] != "" ? MAX(8, std::atoi(c[8].c_str())) : 1024u;
    const uint32_t maxTextureHeight = c[9] != "" ? MAX(1, std::atoi(c[9].c_str())) : 256u;

    return loadingConfigured(frameCount, USE_SGL, scale, useUV, specialOption, path, fileName,
        false, 0, 0, 0, 0.0f, 0.0f, 0.0f, maxTextureWidth, maxTextureHeight);
    //delete [] newModel.animation;


   /** uint32_t cnt=0;
    for (uint32_t i = 0; i<newModel->model[0].nbPolygon; i++){
        if (newModel->model[0].pltbl[i].isMask==0) cnt++;
    }
cout << "DEBUG....\n"; cout << cnt << " out of " << newModel->model[0].nbPolygon << " are masks.\n";*/
}
