#include "../COMMON.H"
#include <algorithm>
#include <iomanip>
#include <limits>
#include <vector>
#include <array>
#include <iterator>
#include <new>
#include "mtl.h"
extern int USE_SGL;

namespace
{
class ZTPReader
{
public:
    explicit ZTPReader(const std::vector<uint8_t> &data) : data(data), offset(0) {}

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

animated_model_t * loadZTPFile(const string &filePath)
{
    ifstream file(filePath.c_str(), ios::binary);
    if (!file.is_open()) return NULL;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (file.bad() || bytes.empty()) return NULL;

    ZTPReader reader(bytes);
    uint16_t modelCount, textureCount, pointCount, polygonCount, frameCount, frameRate;
    uint32_t textureBlockSize, reserved;
    if (!reader.readU16(modelCount) || !reader.readU16(textureCount) || !reader.readU32(textureBlockSize) ||
        !reader.readU16(pointCount) || !reader.readU16(polygonCount)) return NULL;
    if (modelCount == 0 || modelCount > 256 || textureCount > 1280 || polygonCount > maxPolygons) return NULL;

    animated_model_t *model = new (std::nothrow) animated_model_t();
    if (!model) return NULL;
    model->nbModels = modelCount;
    model->nbTextures = textureCount;
    model->model = new (std::nothrow) model_t[modelCount]();
    model->texture = new (std::nothrow) texture_t[textureCount]();
    if (!model->model || (textureCount && !model->texture))
    {
        destroyLoadedZTP(model);
        return NULL;
    }

    if (!readFixedVector(reader, model->BoxMin, XYZ) || !readFixedVector(reader, model->BoxMax, XYZ) ||
        !readFixedVector(reader, model->scale, XYZ) || !reader.readU16(frameCount) ||
        !reader.readU16(frameRate) || !reader.readU32(reserved))
    {
        destroyLoadedZTP(model);
        return NULL;
    }
    for (unsigned int index = 0; index < 9; ++index)
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
            polygon.SGL_ATTR.dir = command & 0x0FFF;
            polygon.SGL_ATTR.sorting = (command >> 12) & 3;
            polygon.SGL_ATTR.flag = (command >> 11) & 1;
            polygon.SGL_ATTR.atrb = attributes;
            polygon.SGL_ATTR.texno = textureId;
            polygon.SGL_ATTR.colno = colorNumber;
            model->texture[textureId].SGL_ATTR.colno = colorNumber;
        }
    }

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

    extern void calcLight(animated_model_t *);
    calcLight(model);
    return model;
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


void findScale(animated_model_t * aModel){
    for (int j=0; j<3; j++){
       aModel->scale[j]=(float)1.0f;
    }

    if (USE_SGL) return; //Not supported for now...

    double scale[3]={1.0f, 1.0f, 1.0f};
    double maxValue[3]={0,0,0};

    for (uint32_t i=0; i<aModel->nbModels; i++) {
        for (uint32_t ii=0; ii<aModel->model[i].nbPoint; ii++){
           for (uint32_t j=0; j<3; j++){
                maxValue[j]=MAX(maxValue[j], fabs(aModel->model[i].pntbl[ii].point[j]));
           }
        }
    }
    for (uint32_t i=0; i<aModel->nbFrames; i++){
        vertex_t * v = &aModel->keyFrames[i].cVert[0];
        for (uint32_t j=0; j<aModel->model[0].nbPoint; j++, v++){
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
        vertex_t * v = &aModel->keyFrames[i].cVert[0];

        for (uint32_t j=0; j<aModel->model[0].nbPoint; j++, v++){

            for (uint32_t jj=0; jj<3; jj++){
              v->point[jj]*=invScale[jj];
            }
        }
    }
}

string fileOutName;
void LoadOBJ(string inFolder, string fileIn, float * scalefactor, animated_model_t * aModel, bool readOnly = false)
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
        aModel->nbTextures=0;

        polygon_t * pol;
        for (uint32_t i=0; i<aModel->nbModels; i++)  {//Ici faut une boucle pour créer les nouvelles textures
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
    //if (aModel->nbFrames==0) reOrgModel(model);
    uint32_t totVertices = 0;
    for (uint32_t i=0; i<aModel->nbModels; i++) {
        totVertices+=aModel->model[i].nbPoint;
    }


    for (uint32_t i=0; i<aModel->nbFrames; i++)
    {
        aModel->keyFrames[i].cVert = new vertex_t [totVertices];
        stringstream frameNumber;
        frameNumber << std::setw(6) << std::setfill('0') << i + 1;
        string postVal = frameNumber.str();
        string animName = inFolder + fileIn + "_" + postVal + ".obj";
        cout << animName << "\n\n****************\n";
        ifstream animFile(animName, ios::in | ios::ate);
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


    void calculateBoundingBox(float * boxMin, float * boxMax, vertex_t * vertexArray, unsigned int totalVertices);
    aModel->BoxMin[X] = aModel->BoxMin[Y] = aModel->BoxMin[Z] =  65536.0f;
    aModel->BoxMax[X] = aModel->BoxMax[Y] = aModel->BoxMax[Z] = -65536.0f;
    for (uint32_t i = 0; i < aModel->nbModels; i++)
    {
        calculateBoundingBox(&aModel->BoxMin[X], &aModel->BoxMax[X], &aModel->model[i].pntbl[0], aModel->model[i].nbPoint); //Updates the whole thing
        aModel->model[i].BoxMax[X]=aModel->model[i].BoxMax[Y]=aModel->model[i].BoxMax[Z]=-65536.0f;
        aModel->model[i].BoxMin[X]=aModel->model[i].BoxMin[Y]=aModel->model[i].BoxMin[Z]= 65536.0f;
        calculateBoundingBox(&aModel->model[i].BoxMin[X], &aModel->model[i].BoxMax[X], &aModel->model[i].pntbl[0], aModel->model[i].nbPoint); //Calculates the sub-model's bounding box in case you want to add per limb collision, headshots, etc.
    }
    if (!useUV)    specialConditions(0, aModel->nbTextures, aModel);
    for (unsigned int modelIndex = 0; modelIndex < aModel->nbModels; ++modelIndex)
    {
        for (unsigned int polygonIndex = 0; polygonIndex < aModel->model[modelIndex].nbPolygon; ++polygonIndex)
            aModel->model[modelIndex].pltbl[polygonIndex].SGL_ATTR.atrb |= CL_Gouraud;
    }

    cout << "Calculating lighting...\n";
    extern void calcLight(animated_model_t*);
    calcLight(aModel);





    findScale(aModel);


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
                                     string path, string fileName, bool readOnly = false)
{
    USE_SGL = useSgl;
    useUV = useUv >= 1 ? 1 : 0;
    specialOption = specialOptions;

    float scaleFactor[XYZ] = {scale, scale, scale};
    animated_model_t * newModel = new animated_model_t;
    ZeroMemory(newModel, sizeof(animated_model_t));
    newModel->nbFrames = frameCount;
    newModel->framerate = 0;
    newModel->nbModels = 0;
    newModel->model = new model_t[256];
    ZeroMemory(newModel->model, sizeof(model_t) * 256);
    newModel->keyFrames = new key_frame_t[newModel->nbFrames];
    ZeroMemory(newModel->keyFrames, sizeof(key_frame_t) * newModel->nbFrames);
    LoadOBJ(path, fileName, scaleFactor, newModel, readOnly);
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
    if (useUV>=1) useUV=1;

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


    return loadingConfigured(frameCount, USE_SGL, scale, useUV, specialOption, path, fileName);
    //delete [] newModel.animation;


   /** uint32_t cnt=0;
    for (uint32_t i = 0; i<newModel->model[0].nbPolygon; i++){
        if (newModel->model[0].pltbl[i].isMask==0) cnt++;
    }
cout << "DEBUG....\n"; cout << cnt << " out of " << newModel->model[0].nbPolygon << " are masks.\n";*/
}
