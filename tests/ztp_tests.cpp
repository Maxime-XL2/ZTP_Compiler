#include "../SRC/COMMON.H"
#include "../SRC/ZTP/lodepng.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <iostream>
#include <iterator>
#include <cstring>
#include <cstddef>
#include <algorithm>
#include <map>

static_assert(sizeof(ATTR) == 12, "ATTR serialized layout must remain 12 bytes");
static_assert(offsetof(ATTR, sorting) == 1, "Sorting must remain the second attribute byte");

animated_model_t *loadZTPFile(const string &filePath);
void write_model_binary(ofstream *, animated_model_t *, bool);
uv_t texture_coord[maxPolygons * 4];
uint32_t tot_tex_coord = 0;
uint32_t useUV = 0;
uint32_t specialOption = 0;
uint32_t maxGeneratedTextureWidth = 512;
uint32_t maxGeneratedTextureHeight = 256;
bool setFbxBonePreviewPointRemap(animated_model_t *, const std::vector<unsigned int> &) { return true; }
bool applyFbxBonePreviewPose(animated_model_t *, unsigned int, unsigned int, float, bool) { return false; }
bool loadFBX(const string &, float *, animated_model_t *, int, int, int, float, float, float) { return false; }
string fbxLoadError;
extern int USE_SGL;
bool bQuit = false;
void recalculateNormals(model_t *) {}
void pointNormals(model_t *) {}
int ReadTGAFile(string, texture_t *, string) { return 0; }
void createFakeTexture(texture_t *) {}
static bool textureBakeTestEnabled = false;
static unsigned int textureBakeTestCalls = 0;
static string textureBakeTestName;
static uint16_t textureBakeTestPixel = 0;
static vector<string> textureBakeTestMaterials;
static vector<float> textureBakeTestCenters;
static string diagnosticImagePath;
void createTexture(polygon_t * polygon, animated_model_t * model, const texture_t & sourceTexture)
{
    if (!textureBakeTestEnabled) return;
    ++textureBakeTestCalls;
    textureBakeTestName = sourceTexture.name;
    textureBakeTestPixel = sourceTexture.pixel ? sourceTexture.pixel[0].rgb : 0;
    textureBakeTestMaterials.push_back(sourceTexture.name);
    float centerX = 0.0f;
    for (unsigned int corner = 0; corner < 4; ++corner)
        centerX += model->model[0].pntbl[polygon->vertIdx[corner]].point[X] * 0.25f;
    textureBakeTestCenters.push_back(centerX);
    polygon->texture = model->nbTextures++;
}
void calcLight(animated_model_t *) {}

static void setDiagnosticPixel(vector<unsigned char> &image, unsigned int width,
                               unsigned int x, unsigned int y, const unsigned char color[4])
{
    const size_t index = (size_t(y) * width + x) * 4;
    for (unsigned int channel = 0; channel < 4; ++channel) image[index + channel] = color[channel];
}

static void sampleDiagnosticAtlas(float u, float v, unsigned char color[4])
{
    u = MAX(0.0f, MIN(1.0f, u));
    v = MAX(0.0f, MIN(1.0f, v));
    const float scaledU = u * 16.0f, scaledV = v * 14.0f;
    const unsigned int tileU = MIN(15u, static_cast<unsigned int>(scaledU));
    const unsigned int tileV = MIN(13u, static_cast<unsigned int>(scaledV));
    const bool border = scaledU - tileU < 0.06f || scaledV - tileV < 0.06f;
    color[0] = border ? 245 : static_cast<unsigned char>(25 + (tileU * 53 + tileV * 29) % 210);
    color[1] = border ? 245 : static_cast<unsigned char>(20 + (tileU * 17 + tileV * 67) % 215);
    color[2] = border ? 245 : static_cast<unsigned char>(30 + (tileU * 71 + tileV * 11) % 205);
    color[3] = 255;
}

static void writeLodTextureDiagnostic(const animated_model_t *model,
                                      unsigned int columns, unsigned int rows)
{
    const unsigned int panelWidth = 256, height = 224, width = panelWidth * 2;
    vector<unsigned char> image(size_t(width) * height * 4, 255);
    vector<unsigned char> covered(size_t(panelWidth) * height, 0);
    unsigned char color[4];
    for (unsigned int y = 0; y < height; ++y)
        for (unsigned int x = 0; x < panelWidth; ++x)
        {
            const float sourceX = float(x) / (panelWidth - 1) * (columns - 1);
            const unsigned int cell = MIN(columns - 2, static_cast<unsigned int>(sourceX));
            const float fraction = sourceX - cell;
            const float leftU = float(cell) / (columns - 1);
            const float rightU = float(cell + 1) / (columns - 1);
            const float u = leftU * leftU + (rightU * rightU - leftU * leftU) * fraction;
            sampleDiagnosticAtlas(u, float(y) / (height - 1), color);
            setDiagnosticPixel(image, width, x, y, color);
        }

    const model_t &mesh = model->model[0];
    for (unsigned int faceIndex = 0; faceIndex < mesh.nbPolygon; ++faceIndex)
    {
        const polygon_t &face = mesh.pltbl[faceIndex];
        const unsigned int triangles = face.vertIdx[2] == face.vertIdx[3] ? 1 : 2;
        const unsigned int triangleCorners[2][3] = {{0, 1, 2}, {0, 2, 3}};
        for (unsigned int triangle = 0; triangle < triangles; ++triangle)
        {
            float screen[3][2];
            const unsigned int *corners = triangleCorners[triangle];
            for (unsigned int vertex = 0; vertex < 3; ++vertex)
            {
                const vertex_t &point = mesh.pntbl[face.vertIdx[corners[vertex]]];
                screen[vertex][0] = point.point[X] / (columns - 1) * (panelWidth - 1);
                screen[vertex][1] = point.point[Y] / (rows - 1) * (height - 1);
            }
            const float denominator = (screen[1][1] - screen[2][1]) * (screen[0][0] - screen[2][0]) +
                (screen[2][0] - screen[1][0]) * (screen[0][1] - screen[2][1]);
            if (std::fabs(denominator) < 1e-8f) continue;
            const int minX = MAX(0, static_cast<int>(std::floor(MIN(screen[0][0], MIN(screen[1][0], screen[2][0])))));
            const int maxX = MIN(static_cast<int>(panelWidth - 1),
                static_cast<int>(std::ceil(MAX(screen[0][0], MAX(screen[1][0], screen[2][0])))));
            const int minY = MAX(0, static_cast<int>(std::floor(MIN(screen[0][1], MIN(screen[1][1], screen[2][1])))));
            const int maxY = MIN(static_cast<int>(height - 1),
                static_cast<int>(std::ceil(MAX(screen[0][1], MAX(screen[1][1], screen[2][1])))));
            for (int y = minY; y <= maxY; ++y)
                for (int x = minX; x <= maxX; ++x)
                {
                    const float w0 = ((screen[1][1] - screen[2][1]) * (x - screen[2][0]) +
                        (screen[2][0] - screen[1][0]) * (y - screen[2][1])) / denominator;
                    const float w1 = ((screen[2][1] - screen[0][1]) * (x - screen[2][0]) +
                        (screen[0][0] - screen[2][0]) * (y - screen[2][1])) / denominator;
                    const float w2 = 1.0f - w0 - w1;
                    if (w0 < -1e-5f || w1 < -1e-5f || w2 < -1e-5f) continue;
                    float u = 0.0f, v = 0.0f;
                    for (unsigned int vertex = 0; vertex < 3; ++vertex)
                    {
                        const float weight = vertex == 0 ? w0 : vertex == 1 ? w1 : w2;
                        u += face.vertUV[corners[vertex]].U * weight;
                        v += face.vertUV[corners[vertex]].V * weight;
                    }
                    sampleDiagnosticAtlas(u, v, color);
                    setDiagnosticPixel(image, width, panelWidth + x, y, color);
                    covered[size_t(y) * panelWidth + x] = 1;
                }
        }
    }

    unsigned int compared = 0, mismatched = 0;
    for (unsigned int y = 0; y < height; ++y)
        for (unsigned int x = 0; x < panelWidth; ++x)
        {
            if (!covered[size_t(y) * panelWidth + x]) continue;
            ++compared;
            const size_t expected = (size_t(y) * width + x) * 4;
            const size_t actual = (size_t(y) * width + panelWidth + x) * 4;
            if (std::memcmp(&image[expected], &image[actual], 3) != 0) ++mismatched;
        }
    assert(compared > panelWidth * height * 9 / 10);
    std::fprintf(stderr, "LOD texture diagnostic: %u/%u pixels differ (%u%%)\n",
        mismatched, compared, mismatched * 100 / compared);
    assert(mismatched * 100 < compared * 10);
        vector<unsigned char> png;
        const unsigned int encodeError = lodepng::encode(png, image, width, height);
        if (encodeError) std::fprintf(stderr, "Diagnostic PNG error: %s\n", lodepng_error_text(encodeError));
        assert(encodeError == 0);
        ofstream output(diagnosticImagePath.c_str(), ios::binary);
        assert(output.is_open());
        output.write(reinterpret_cast<const char *>(png.data()), png.size());
        assert(output.good());
}

static void writeU8(vector<uint8_t> &bytes, uint8_t value)
{
    bytes.push_back(value);
}

static void writeU16(vector<uint8_t> &bytes, uint16_t value)
{
    writeU8(bytes, static_cast<uint8_t>(value >> 8));
    writeU8(bytes, static_cast<uint8_t>(value));
}

static void writeU32(vector<uint8_t> &bytes, uint32_t value)
{
    writeU8(bytes, static_cast<uint8_t>(value >> 24));
    writeU8(bytes, static_cast<uint8_t>(value >> 16));
    writeU8(bytes, static_cast<uint8_t>(value >> 8));
    writeU8(bytes, static_cast<uint8_t>(value));
}

static void writeFixed(vector<uint8_t> &bytes, int32_t value)
{
    writeU32(bytes, static_cast<uint32_t>(value * 65536));
}

static vector<uint8_t> makeFixture(uint16_t depth, unsigned int meshCount = 1, unsigned int frameCount = 1)
{
    vector<uint8_t> bytes;
    const uint32_t textureBlockSize = depth == COL_32K ? 52 : 46;
    writeU16(bytes, meshCount);
    writeU16(bytes, 1);
    writeU32(bytes, textureBlockSize);
    writeU16(bytes, 3 * meshCount);
    writeU16(bytes, meshCount);
    for (int axis = 0; axis < 3; ++axis) writeFixed(bytes, 0);
    writeFixed(bytes, 10);
    writeFixed(bytes, 10);
    writeFixed(bytes, 0);
    for (int axis = 0; axis < 3; ++axis) writeFixed(bytes, 1);
    writeU16(bytes, frameCount);
    writeU16(bytes, 2);
    writeU32(bytes, 0);
    for (int index = 0; index < 9; ++index) writeU32(bytes, 0);

    writeU16(bytes, 2);
    writeU16(bytes, 2);
    writeU16(bytes, depth);
    writeU16(bytes, depth == COL_32K ? 0 : 16);
    writeU32(bytes, 0);
    if (depth == COL_32K)
    {
        for (uint16_t color : {uint16_t(0x801F), uint16_t(0x83E0), uint16_t(0xFC00), uint16_t(0)})
            writeU16(bytes, color);
    }
    else
    {
        writeU8(bytes, 0x12);
        writeU8(bytes, 0x30);
    }
    for (unsigned int color = 0; color < 16; ++color)
        writeU16(bytes, color == 1 ? 0x801F : color == 2 ? 0x83E0 : color == 3 ? 0xFC00 : 0);

    for (unsigned int mesh = 0; mesh < meshCount; ++mesh)
    {
        writeU16(bytes, mesh * 3);
        writeU16(bytes, (mesh + 1) * 3);
        writeU16(bytes, mesh);
        writeU16(bytes, mesh + 1);
        writeU16(bytes, 0);
        writeU16(bytes, 0);
    }
    const int8_t basePoints[3][3] = {{0, 0, 0}, {10, 0, 0}, {0, 10, 0}};
    const int8_t framePoints[3][3] = {{0, 0, 0}, {20, 0, 0}, {0, 20, 0}};
    for (unsigned int mesh = 0; mesh < meshCount; ++mesh)
    for (const auto &point : basePoints)
    {
        for (int axis = 0; axis < 3; ++axis) writeU8(bytes, static_cast<uint8_t>(point[axis]));
        writeU8(bytes, 161);
    }
    for (unsigned int frame = 0; frame < frameCount; ++frame)
    for (unsigned int mesh = 0; mesh < meshCount; ++mesh)
    for (const auto &point : framePoints)
    {
        for (int axis = 0; axis < 3; ++axis) writeU8(bytes, static_cast<uint8_t>(point[axis]));
        writeU8(bytes, frame + 7);
    }
    for (unsigned int mesh = 0; mesh < meshCount; ++mesh)
    {
        writeU8(bytes, mesh * 3);
        writeU8(bytes, mesh * 3 + 1);
        writeU8(bytes, mesh * 3 + 2);
        writeU8(bytes, mesh * 3 + 2);
        writeU16(bytes, 0);
        writeU16(bytes, 0x2805);
        writeU16(bytes, 0x0200);
        writeU16(bytes, mesh + 1);
    }
    for (unsigned int point = 0; point < 3 * meshCount; ++point)
    {
        writeFixed(bytes, 0);
        writeFixed(bytes, 0);
        writeFixed(bytes, 1);
    }
    return bytes;
}

static void destroyModel(animated_model_t *model)
{
    if (!model) return;
    delete model->lodData;
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
    {
        delete [] model->model[mesh].pntbl;
        delete [] model->model[mesh].pltbl;
    }
    for (unsigned int frame = 0; frame < model->nbFrames; ++frame)
        delete [] model->keyFrames[frame].cVert;
    for (unsigned int texture = 0; texture < model->nbTextures; ++texture)
        delete [] model->texture[texture].pixel;
    delete [] model->model;
    delete [] model->keyFrames;
    delete [] model->texture;
    delete model;
}

static animated_model_t *loadFixture(uint16_t depth, const string &path, unsigned int meshCount = 1,
                                    unsigned int frameCount = 1)
{
    vector<uint8_t> bytes = makeFixture(depth, meshCount, frameCount);
    ofstream file(path.c_str(), ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    file.close();
    assert(file.good());
    return loadZTPFile(path);
}

static void checkRoundTrip(animated_model_t * original, uint16_t depth, const string & path,
                           size_t expectedSize = 0)
{
    USE_SGL = 1;
    ofstream output(path.c_str(), ios::binary);
    write_model_binary(&output, original, true);
    assert(USE_SGL == 1);
    output.close();
    assert(output.good());
    ifstream bytes(path.c_str(), ios::binary | ios::ate);
    const size_t outputSize = static_cast<size_t>(bytes.tellg());
    assert(outputSize == 32 + (expectedSize ? expectedSize : makeFixture(depth, original->nbModels, original->nbFrames).size()));
    if (expectedSize) std::cout << "Real model round-trip: " << expectedSize << " -> " << outputSize << " bytes\n";
    bytes.close();
    animated_model_t * reloaded = loadZTPFile(path);
    assert(reloaded);
    assert(reloaded->nbModels == original->nbModels);
    assert(reloaded->nbTextures == original->nbTextures);
    assert(reloaded->nbFrames == original->nbFrames);
    assert(reloaded->framerate == original->framerate);
    for (unsigned int axis = 0; axis < XYZ; ++axis)
    {
        assert(reloaded->scale[axis] == original->scale[axis]);
        assert(reloaded->BoxMin[axis] == original->BoxMin[axis]);
        assert(reloaded->BoxMax[axis] == original->BoxMax[axis]);
    }
    unsigned int offset = 0;
    for (unsigned int mesh = 0; mesh < original->nbModels; ++mesh)
    {
        const model_t & expected = original->model[mesh];
        const model_t & actual = reloaded->model[mesh];
        assert(actual.nbPoint == expected.nbPoint && actual.nbPolygon == expected.nbPolygon);
        for (unsigned int point = 0; point < expected.nbPoint; ++point)
        {
            assert(actual.pntbl[point].normalArray == expected.pntbl[point].normalArray);
            for (unsigned int frame = 0; frame < original->nbFrames; ++frame)
                assert(reloaded->keyFrames[frame].cVert[offset + point].normalArray ==
                       original->keyFrames[frame].cVert[offset + point].normalArray);
            for (unsigned int axis = 0; axis < XYZ; ++axis)
            {
                assert(actual.pntbl[point].point[axis] == expected.pntbl[point].point[axis]);
                assert(actual.pntbl[point].normal[axis] == expected.pntbl[point].normal[axis]);
                for (unsigned int frame = 0; frame < original->nbFrames; ++frame)
                    assert(reloaded->keyFrames[frame].cVert[offset + point].point[axis] ==
                           original->keyFrames[frame].cVert[offset + point].point[axis]);
            }
                }
        for (unsigned int face = 0; face < expected.nbPolygon; ++face)
        {
            const polygon_t & first = expected.pltbl[face];
            const polygon_t & second = actual.pltbl[face];
            assert(first.texture == second.texture);
            assert(first.SGL_ATTR.dir == second.SGL_ATTR.dir);
            assert(first.SGL_ATTR.flag == second.SGL_ATTR.flag);
            assert(first.SGL_ATTR.sorting == second.SGL_ATTR.sorting);
            assert(first.SGL_ATTR.atrb == second.SGL_ATTR.atrb);
            assert(first.SGL_ATTR.colno == second.SGL_ATTR.colno);
            for (unsigned int corner = 0; corner < 4; ++corner)
                assert(first.vertIdx[corner] == second.vertIdx[corner]);
        }
        offset += expected.nbPoint;
    }
    for (unsigned int texture = 0; texture < original->nbTextures; ++texture)
    {
        const texture_t & first = original->texture[texture];
        const texture_t & second = reloaded->texture[texture];
        assert(first.width == second.width && first.height == second.height && first.colorDepth == second.colorDepth);
        for (unsigned int color = 0; color < 16; ++color) assert(first.clut[color] == second.clut[color]);
        for (unsigned int pixel = 0; pixel < first.width * first.height; ++pixel)
        {
            assert(first.pixel[pixel].rgb == second.pixel[pixel].rgb);
            assert(first.pixel[pixel].palette_idx == second.pixel[pixel].palette_idx);
        }
    }
    destroyModel(reloaded);
    assert(saveZTPFile(path, original));
    reloaded = loadZTPFile(path);
    assert(reloaded);
    destroyModel(reloaded);
    ifstream savedFile(path.c_str(), ios::binary);
    const vector<uint8_t> savedBytes((std::istreambuf_iterator<char>(savedFile)), std::istreambuf_iterator<char>());
    savedFile.close();
    assert(SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_READONLY));
    assert(!saveZTPFile(path, original));
    ifstream protectedFile(path.c_str(), ios::binary);
    const vector<uint8_t> protectedBytes((std::istreambuf_iterator<char>(protectedFile)), std::istreambuf_iterator<char>());
    protectedFile.close();
    assert(savedBytes == protectedBytes);
    assert(SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL));
    std::remove(path.c_str());
}

static vector<uint8_t> readFileBytes(const string & path)
{
    ifstream file(path.c_str(), ios::binary);
    return vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

static void checkLegacyRawSorting()
{
    vector<uint8_t> bytes = makeFixture(COL_16);
    const size_t polygonOffset = 92 + 46 + 12 + 4 * 3 * 2;
    bytes[polygonOffset + 6] = 0;
    bytes[polygonOffset + 7] = FUNC_Texture;
    ofstream old("raw_commands.ztp", ios::binary);
    old.write(reinterpret_cast<const char *>(bytes.data()), bytes.size()); old.close();
    animated_model_t * loaded = loadZTPFile("raw_commands.ztp");
    assert(loaded && loaded->model[0].pltbl[0].SGL_ATTR.sorting == SORT_CEN);
    assert(saveZTPFile("raw_commands_modern.ztp", loaded));
    animated_model_t * modern = loadZTPFile("raw_commands_modern.ztp");
    assert(modern && modern->model[0].pltbl[0].SGL_ATTR.sorting == SORT_CEN);
    destroyModel(modern);
    loaded->model[0].pltbl[0].SGL_ATTR.sorting = SORT_BFR;
    assert(saveZTPFile("raw_commands_modern.ztp", loaded));
    modern = loadZTPFile("raw_commands_modern.ztp");
    assert(modern && modern->model[0].pltbl[0].SGL_ATTR.sorting == SORT_BFR);
    destroyModel(modern);
    destroyModel(loaded);
    std::remove("raw_commands.ztp"); std::remove("raw_commands_modern.ztp");
}

static void checkSortingModes()
{
    ATTR attribute = {};
    polygon_t polygon = {};
    texture_t texture = {};
    assert(attribute.sorting == SORT_CEN);
    assert(polygon.SGL_ATTR.sorting == SORT_CEN);
    assert(texture.SGL_ATTR.sorting == SORT_CEN);
    const unsigned int modes[] = {SORT_BFR, SORT_MIN, SORT_MAX, SORT_CEN};
    const char * names[] = {"SORT_BFR", "SORT_MIN", "SORT_MAX", "SORT_CEN"};
    for (unsigned int mode = 0; mode < 4; ++mode)
        assert(std::strcmp(sortingModeName(modes[mode] | UseNearClip | UseLight), names[mode]) == 0);
    for (unsigned int sgl = 0; sgl < 2; ++sgl)
    {
        animated_model_t * model = loadFixture(COL_16, "sorting_legacy.ztp");
        assert(model);
        model->sourceSgl = sgl;
        for (unsigned int mode : modes)
        {
            model->model[0].pltbl[0].SGL_ATTR.sorting = mode | (sgl ? UseNearClip | UseLight : 0);
            assert(saveZTPFile("sorting_modern.ztp", model));
            animated_model_t * loaded = loadZTPFile("sorting_modern.ztp");
            assert(loaded && (loaded->model[0].pltbl[0].SGL_ATTR.sorting & 3) == mode);
            if (sgl) assert(loaded->model[0].pltbl[0].SGL_ATTR.sorting == (mode | UseNearClip | UseLight));
            destroyModel(loaded);
            vector<uint8_t> bytes = readFileBytes("sorting_modern.ztp");
            ofstream old("sorting_legacy.ztp", ios::binary | ios::trunc);
            old.write(reinterpret_cast<const char *>(bytes.data() + 32), bytes.size() - 32); old.close();
            loaded = loadZTPFile("sorting_legacy.ztp");
            assert(loaded && (loaded->model[0].pltbl[0].SGL_ATTR.sorting & 3) == mode);
            destroyModel(loaded);
        }
        destroyModel(model);
    }
    std::remove("sorting_legacy.ztp"); std::remove("sorting_modern.ztp");
}

static void checkLodFile(bool sgl)
{
    const string sourcePath = "lod_source.ztp", path = "lod_modern.ztp", secondPath = "lod_resaved.ztp";
    animated_model_t * model = loadFixture(COL_16, sourcePath, 2, 2);
    assert(model);
    for (unsigned int mesh = 0; mesh < 2; ++mesh)
    {
        vertex_t * expanded = new vertex_t[4]();
        std::copy(model->model[mesh].pntbl, model->model[mesh].pntbl + 3, expanded);
        expanded[3].point[Z] = 15;
        expanded[3].normal[Z] = 1;
        delete [] model->model[mesh].pntbl;
        model->model[mesh].pntbl = expanded;
        model->model[mesh].nbPoint = 4;
        model->model[mesh].pltbl[0].vertIdx[3] = 3;
    }
    for (unsigned int frame = 0; frame < 2; ++frame)
    {
        vertex_t * expanded = new vertex_t[8]();
        for (unsigned int mesh = 0; mesh < 2; ++mesh)
        {
            std::copy(model->keyFrames[frame].cVert + mesh * 3, model->keyFrames[frame].cVert + mesh * 3 + 3,
                expanded + mesh * 4);
            expanded[mesh * 4 + 3].point[Z] = 30 + frame;
            expanded[mesh * 4 + 3].normal[Z] = 1;
        }
        delete [] model->keyFrames[frame].cVert;
        model->keyFrames[frame].cVert = expanded;
    }
    model->sourceSgl = sgl ? 1 : 0;
    model->lodData = new model_lod_data_t;
    model->lodData->levels.resize(2);
    for (unsigned int level = 0; level < 2; ++level)
    {
        lod_level_t & lod = model->lodData->levels[level];
        lod.reduction = level ? 90 : 50;
        lod.meshes.resize(2);
        for (unsigned int mesh = 0; mesh < 2; ++mesh)
        {
            polygon_t face = model->model[mesh].pltbl[0];
            if (level)
            {
                face.vertIdx[0] = 1; face.vertIdx[1] = 2;
                face.vertIdx[2] = 3; face.vertIdx[3] = 3;
            }
            face.color.rgb = 0x801F;
            lod.meshes[mesh].faces.push_back(face);
        }
    }
    assert(saveZTPFile(path, model));
    vector<uint8_t> bytes = readFileBytes(path);
    assert(std::memcmp(bytes.data(), sgl ? "ZTP_S01\0" : "ZTP_Z01\0", 8) == 0);
    animated_model_t * loaded = loadZTPFile(path);
    assert(loaded && loaded->sourceSgl == model->sourceSgl);
    assert(loaded->lodData && loaded->lodData->levels.size() == 2);
    const unsigned int order[4] = {1, 2, 3, 0};
    for (unsigned int mesh = 0; mesh < 2; ++mesh)
    {
        assert(loaded->lodData->levels[0].meshes[mesh].pointCount == 4);
        assert(loaded->lodData->levels[1].meshes[mesh].pointCount == 3);
        for (unsigned int point = 0; point < 4; ++point)
            for (unsigned int axis = 0; axis < XYZ; ++axis)
            {
                assert(loaded->model[mesh].pntbl[point].point[axis] == model->model[mesh].pntbl[order[point]].point[axis]);
                for (unsigned int frame = 0; frame < 2; ++frame)
                    assert(loaded->keyFrames[frame].cVert[mesh * 4 + point].point[axis] ==
                        model->keyFrames[frame].cVert[mesh * 4 + order[point]].point[axis]);
            }
        assert(loaded->model[mesh].pltbl[0].vertIdx[0] == 3);
        for (unsigned int level = 0; level < 2; ++level)
            for (const polygon_t & face : loaded->lodData->levels[level].meshes[mesh].faces)
                for (unsigned int corner = 0; corner < 4; ++corner)
                    assert(face.vertIdx[corner] < loaded->lodData->levels[level].meshes[mesh].pointCount);
        assert(model->model[mesh].pltbl[0].vertIdx[0] == 0);
    }
    assert(saveZTPFile(secondPath, loaded));
    assert(bytes == readFileBytes(secondPath));
    vector<uint8_t> invalid = bytes;
    invalid[8] = 1;
    ofstream bad("invalid_modern.ztp", ios::binary);
    bad.write(reinterpret_cast<const char *>(invalid.data()), invalid.size()); bad.close();
    assert(!loadZTPFile("invalid_modern.ztp"));
    for (unsigned int variant = 0; variant < 4; ++variant)
    {
        invalid = bytes;
        if (variant == 0) invalid[6] = '2';
        else if (variant == 1) { invalid[24] = 0; invalid[25] = 0; }
        else if (variant == 2) invalid.pop_back();
        else
        {
            const uint32_t directory = (uint32_t(bytes[16]) << 24) | (uint32_t(bytes[17]) << 16) |
                (uint32_t(bytes[18]) << 8) | bytes[19];
            invalid[directory + 64] = 0;
            invalid[directory + 65] = 2;
        }
        ofstream rejected("invalid_modern.ztp", ios::binary);
        rejected.write(reinterpret_cast<const char *>(invalid.data()), invalid.size()); rejected.close();
        assert(!loadZTPFile("invalid_modern.ztp"));
    }
    assert(deleteAutomaticLod(loaded, 1));
    assert(saveZTPFile(secondPath, loaded));
    animated_model_t * remaining = loadZTPFile(secondPath);
    assert(remaining && automaticLodLevelCount(remaining) == 1);
    assert(remaining->lodData->levels[0].reduction == 90);
    assert(!deleteAutomaticLod(remaining, 0));
    assert(deleteAutomaticLod(remaining, 1));
    assert(saveZTPFile(secondPath, remaining));
    animated_model_t * baseOnly = loadZTPFile(secondPath);
    assert(baseOnly && automaticLodLevelCount(baseOnly) == 0 && baseOnly->nbModels == 2);
    destroyModel(baseOnly);
    destroyModel(remaining);
    if (sgl)
    {
        loaded->lodData->levels.clear();
        assert(saveZTPFile(secondPath, loaded));
        vector<uint8_t> single = readFileBytes(secondPath);
        ofstream old("legacy_sgl.ztp", ios::binary);
        old.write(reinterpret_cast<const char *>(single.data() + 32), single.size() - 32); old.close();
        animated_model_t * legacy = loadZTPFile("legacy_sgl.ztp");
        assert(legacy && legacy->sourceSgl == 1 && legacy->nbFrames == 2);
        assert(saveZTPFile(secondPath, legacy));
        single = readFileBytes(secondPath);
        assert(std::memcmp(single.data(), "ZTP_S01\0", 8) == 0);
        destroyModel(legacy);
        std::remove("legacy_sgl.ztp");
    }
    destroyModel(loaded);
    destroyModel(model);
    std::remove(sourcePath.c_str()); std::remove(path.c_str()); std::remove(secondPath.c_str());
    std::remove("invalid_modern.ztp");
}

static void checkNonSglReducedBaseFromLargeSource()
{
    const unsigned int columns = 19, rows = 15;
    const unsigned int pointCount = columns * rows;
    animated_model_t * model = new animated_model_t();
    model->nbModels = 1;
    model->model = new model_t[1]();
    model->model[0].nbPoint = pointCount;
    model->model[0].pntbl = new vertex_t[pointCount]();
    for (unsigned int row = 0; row < rows; ++row)
        for (unsigned int column = 0; column < columns; ++column)
        {
            vertex_t & point = model->model[0].pntbl[row * columns + column];
            point.point[X] = static_cast<float>(column);
            point.point[Y] = static_cast<float>(row);
        }
    model->model[0].nbPolygon = (columns - 1) * (rows - 1);
    model->model[0].pltbl = new polygon_t[model->model[0].nbPolygon]();
    unsigned int faceIndex = 0;
    for (unsigned int row = 0; row + 1 < rows; ++row)
        for (unsigned int column = 0; column + 1 < columns; ++column)
        {
            polygon_t & face = model->model[0].pltbl[faceIndex++];
            face.vertIdx[0] = row * columns + column;
            face.vertIdx[1] = face.vertIdx[0] + 1;
            face.vertIdx[3] = (row + 1) * columns + column;
            face.vertIdx[2] = face.vertIdx[3] + 1;
            face.texture = 0;
        }
    model->nbTextures = 1;
    model->texture = new texture_t[1]();
    model->texture[0].width = model->texture[0].height = 1;
    model->texture[0].colorDepth = COL_32K;
    model->texture[0].pixel = new pixel_t[1]();
    model->texture[0].pixel[0].a = 255;

    assert(replaceAutomaticLodBase(model, 75));
    assert(model->model[0].nbPoint <= 256 && model->model[0].nbPoint < pointCount);
    assert(model->model[0].nbPolygon < (columns - 1) * (rows - 1));
    unsigned int quadCount = 0;
    for (unsigned int face = 0; face < model->model[0].nbPolygon; ++face)
    {
        for (unsigned int corner = 0; corner < 4; ++corner)
            assert(model->model[0].pltbl[face].vertIdx[corner] < model->model[0].nbPoint);
        if (model->model[0].pltbl[face].vertIdx[2] != model->model[0].pltbl[face].vertIdx[3]) ++quadCount;
    }
    assert(quadCount > 0);

    const string path = "large_non_sgl_reduced_base.ztp";
    assert(saveZTPFile(path, model));
    const vector<uint8_t> bytes = readFileBytes(path);
    assert(std::memcmp(bytes.data(), "ZTP_Z01\0", 8) == 0);
    animated_model_t * loaded = loadZTPFile(path);
    assert(loaded && !loaded->sourceSgl && loaded->model[0].nbPoint == model->model[0].nbPoint);
    destroyModel(loaded);
    std::remove(path.c_str());

    assert(restoreAutomaticLodBase(model));
    assert(model->model[0].nbPoint == pointCount);
    assert(model->model[0].pltbl[model->model[0].nbPolygon - 1].vertIdx[2] == pointCount - 1);
    destroyModel(model);
}

static void checkAutomaticLodTextureRebake()
{
    const unsigned int columns = 19, rows = 15;
    const unsigned int pointCount = columns * rows;
    animated_model_t * model = new animated_model_t();
    model->nbModels = 1;
    model->model = new model_t[1]();
    model->model[0].nbPoint = pointCount;
    model->model[0].pntbl = new vertex_t[pointCount]();
    for (unsigned int row = 0; row < rows; ++row)
        for (unsigned int column = 0; column < columns; ++column)
        {
            const unsigned int pointIndex = row * columns + column;
            vertex_t & point = model->model[0].pntbl[pointIndex];
            point.point[X] = static_cast<float>(column);
            point.point[Y] = static_cast<float>(row);
        }
    model->model[0].nbPolygon = (columns - 1) * (rows - 1);
    model->model[0].pltbl = new polygon_t[model->model[0].nbPolygon]();
    unsigned int faceIndex = 0;
    for (unsigned int row = 0; row + 1 < rows; ++row)
        for (unsigned int column = 0; column + 1 < columns; ++column)
        {
            polygon_t & face = model->model[0].pltbl[faceIndex++];
            face.vertIdx[0] = row * columns + column;
            face.vertIdx[1] = face.vertIdx[0] + 1;
            face.vertIdx[3] = (row + 1) * columns + column;
            face.vertIdx[2] = face.vertIdx[3] + 1;
            face.texture = column < (columns - 1) / 2 ? 0 : 1;
            const float leftU = float(column) / (columns - 1);
            const float rightU = float(column + 1) / (columns - 1);
            const float topV = float(row) / (rows - 1);
            const float bottomV = float(row + 1) / (rows - 1);
            face.vertUV[0] = {leftU * leftU, topV};
            face.vertUV[1] = {rightU * rightU, topV};
            face.vertUV[2] = {rightU * rightU, bottomV};
            face.vertUV[3] = {leftU * leftU, bottomV};
        }
    model->texture = new texture_t[2048]();
    model->nbTextures = 2;
    for (unsigned int texture = 0; texture < model->nbTextures; ++texture)
    {
        model->texture[texture].width = model->texture[texture].height = 1;
        model->texture[texture].pixel = new pixel_t[1]();
    }
    pixel_t atlasPixels[2] = {};
    atlasPixels[0].rgb = 0x8123;
    atlasPixels[1].rgb = 0x8456;
    texture_t atlases[2] = {};
    atlases[0].name = "gina";
    atlases[1].name = "gina-detail";
    for (unsigned int texture = 0; texture < 2; ++texture)
    {
        atlases[texture].width = atlases[texture].height = 1;
        atlases[texture].pixel = &atlasPixels[texture];
    }
    cacheAutomaticLodSourceTextures(model, atlases, 2);
    atlasPixels[0].rgb = atlasPixels[1].rgb = 0;
    textureBakeTestCalls = 0;
    textureBakeTestMaterials.clear();
    textureBakeTestCenters.clear();
    textureBakeTestEnabled = true;
    useUV = 1;
    assert(replaceAutomaticLodBase(model, 37));
    textureBakeTestEnabled = false;
    assert(model->model[0].nbPoint < pointCount && model->model[0].nbPoint <= 256);
    assert(model->model[0].nbPolygon < (columns - 1) * (rows - 1));
    assert(textureBakeTestCalls == model->model[0].nbPolygon && textureBakeTestCalls > 0);
    assert(textureBakeTestName == "gina" || textureBakeTestName == "gina-detail");
    assert(textureBakeTestPixel == 0x8123 || textureBakeTestPixel == 0x8456);
    bool bakedFirstMaterial = false, bakedSecondMaterial = false;
    for (size_t index = 0; index < textureBakeTestMaterials.size(); ++index)
    {
        const string & material = textureBakeTestMaterials[index];
        bakedFirstMaterial = bakedFirstMaterial || material == "gina";
        bakedSecondMaterial = bakedSecondMaterial || material == "gina-detail";
        if (textureBakeTestCenters[index] < 6.0f) assert(material == "gina");
        if (textureBakeTestCenters[index] > 12.0f) assert(material == "gina-detail");
    }
    assert(bakedFirstMaterial && bakedSecondMaterial);
    assert(textureBakeTestMaterials.size() == model->model[0].nbPolygon);
    unsigned int quadCount = 0;
    for (unsigned int face = 0; face < model->model[0].nbPolygon; ++face)
    {
        const polygon_t &polygon = model->model[0].pltbl[face];
        assert(polygon.texture >= 1);
        if (polygon.vertIdx[2] != polygon.vertIdx[3]) ++quadCount;
        float minimumX = model->model[0].pntbl[polygon.vertIdx[0]].point[X];
        float maximumX = minimumX;
        for (unsigned int corner = 1; corner < 4; ++corner)
        {
            const float x = model->model[0].pntbl[polygon.vertIdx[corner]].point[X];
            minimumX = MIN(minimumX, x);
            maximumX = MAX(maximumX, x);
        }
        if (textureBakeTestMaterials[face] == "gina") assert(maximumX <= 9.0001f);
        else assert(minimumX >= 8.9999f);
    }
    assert(quadCount > 0);
    assert(restoreAutomaticLodBase(model));
    assert(model->nbTextures == 2);

    textureBakeTestCalls = 0;
    textureBakeTestEnabled = true;
    assert(replaceAutomaticLodBase(model, 37));
    textureBakeTestEnabled = false;
    assert(textureBakeTestCalls == model->model[0].nbPolygon && textureBakeTestCalls > 0);
    assert(restoreAutomaticLodBase(model));
    assert(model->nbTextures == 2);

    for (unsigned int face = 0; face < model->model[0].nbPolygon; ++face)
        model->model[0].pltbl[face].texture = 0;
    cacheAutomaticLodSourceTextures(model, atlases, 2);
    textureBakeTestEnabled = true;
    assert(replaceAutomaticLodBase(model, 37));
    textureBakeTestEnabled = false;
    for (unsigned int face = 0; face < model->model[0].nbPolygon; ++face)
    {
        const polygon_t &polygon = model->model[0].pltbl[face];
        for (unsigned int corner = 0; corner < 4; ++corner)
        {
            const vertex_t &point = model->model[0].pntbl[polygon.vertIdx[corner]];
            const float u = point.point[X] / (columns - 1);
            assert(std::fabs(polygon.vertUV[corner].U - u * u) < 1e-4f);
            assert(std::fabs(polygon.vertUV[corner].V - point.point[Y] / (rows - 1)) < 1e-4f);
        }
    }
    writeLodTextureDiagnostic(model, columns, rows);
    assert(restoreAutomaticLodBase(model));
    assert(model->nbTextures == 2);
    for (unsigned int face = 0; face < model->model[0].nbPolygon; ++face)
    {
        polygon_t & polygon = model->model[0].pltbl[face];
        const bool rightChart = model->model[0].pntbl[polygon.vertIdx[0]].point[X] >= 9.0f;
        for (unsigned int corner = 0; corner < 4; ++corner)
            polygon.vertUV[corner].U += rightChart ? 0.5f : 0.0f;
    }
    cacheAutomaticLodSourceTextures(model, atlases, 2);
    textureBakeTestEnabled = true;
    assert(replaceAutomaticLodBase(model, 37));
    textureBakeTestEnabled = false;
    assert(model->model[0].nbPoint < pointCount);
    for (unsigned int face = 0; face < model->model[0].nbPolygon; ++face)
    {
        const polygon_t & polygon = model->model[0].pltbl[face];
        float minimumX = float(columns), maximumX = 0.0f;
        for (unsigned int corner = 0; corner < 4; ++corner)
        {
            const float x = model->model[0].pntbl[polygon.vertIdx[corner]].point[X];
            minimumX = MIN(minimumX, x);
            maximumX = MAX(maximumX, x);
        }
        assert(maximumX <= 9.0001f || minimumX >= 8.9999f);
        const float chartOffset = minimumX >= 8.9999f ? 0.5f : 0.0f;
        for (unsigned int corner = 0; corner < 4; ++corner)
        {
            const float u = model->model[0].pntbl[polygon.vertIdx[corner]].point[X] / (columns - 1);
            assert(std::fabs(polygon.vertUV[corner].U - u * u - chartOffset) < 1e-4f);
        }
    }
    assert(restoreAutomaticLodBase(model));
    discardAutomaticLodSourceTextures(model);
    useUV = 0;
    destroyModel(model);
}

static void checkFoldedLodUvTransfer()
{
    const unsigned int columns = 19, rows = 15;
    animated_model_t model = {};
    model_t mesh = {};
    model.nbModels = 1;
    model.model = &mesh;
    mesh.nbPoint = columns * rows;
    mesh.pntbl = new vertex_t[mesh.nbPoint]();
    mesh.nbPolygon = (columns - 1) * (rows - 1);
    mesh.pltbl = new polygon_t[mesh.nbPolygon]();
    model.texture = new texture_t[2048]();
    pixel_t pixel = {};
    texture_t atlas = {};
    atlas.name = "folded";
    atlas.width = atlas.height = 1;
    atlas.pixel = &pixel;
    for (unsigned int row = 0; row < rows; ++row)
        for (unsigned int column = 0; column < columns; ++column)
        {
            vertex_t & point = mesh.pntbl[row * columns + column];
            point.point[X] = std::fabs(float(column) - 9.0f);
            point.point[Y] = float(row);
            point.normal[X] = float(column);
            if (column + 1 == columns || row + 1 == rows) continue;
            polygon_t & face = mesh.pltbl[row * (columns - 1) + column];
            face.vertIdx[0] = row * columns + column;
            face.vertIdx[1] = face.vertIdx[0] + 1;
            face.vertIdx[2] = face.vertIdx[1] + columns;
            face.vertIdx[3] = face.vertIdx[0] + columns;
            for (unsigned int corner = 0; corner < 4; ++corner)
                face.vertUV[corner] = {float(column + (corner == 1 || corner == 2)) / 18.0f,
                    float(row + (corner >= 2)) / 14.0f};
        }
    useUV = 1;
    cacheAutomaticLodSourceTextures(&model, &atlas, 1);
    assert(generateAutomaticLod(&model, 37));
    unsigned int count = 0;
    const polygon_t * faces = automaticLodFaces(&model, 0, &count);
    assert(faces && automaticLodPointCount() < mesh.nbPoint);
    for (unsigned int face = 0; face < count; ++face)
        for (unsigned int corner = 0; corner < 4; ++corner)
        {
            const vertex_t & point = mesh.pntbl[faces[face].vertIdx[corner]];
            assert(std::fabs(faces[face].vertUV[corner].U - point.normal[X] / 18.0f) < 1e-4f);
            assert(std::fabs(faces[face].vertUV[corner].V - point.point[Y] / 14.0f) < 1e-4f);
        }
    resetAutomaticLod();
    discardAutomaticLodSourceTextures(&model);
    delete [] mesh.pntbl;
    delete [] mesh.pltbl;
    delete [] model.texture;
    delete model.lodData;
    useUV = 0;
}

static void checkWideSglLod()
{
    vector<vertex_t> points(300);
    for (unsigned int point = 0; point < points.size(); ++point)
        points[point].point[X] = static_cast<float>(point);
    polygon_t baseFace = {};
    baseFace.vertIdx[0] = 297;
    baseFace.vertIdx[1] = 298;
    baseFace.vertIdx[2] = 299;
    baseFace.vertIdx[3] = 299;
    baseFace.texture = 0;
    model_t mesh = {};
    mesh.nbPoint = points.size();
    mesh.nbPolygon = 1;
    mesh.pntbl = points.data();
    mesh.pltbl = &baseFace;
    pixel_t pixel = {};
    pixel.a = 255;
    texture_t texture = {};
    texture.width = texture.height = 1;
    texture.colorDepth = COL_32K;
    texture.pixel = &pixel;
    model_lod_data_t lodData = {};
    animated_model_t model = {};
    model.sourceSgl = 1;
    model.nbModels = 1;
    model.model = &mesh;
    model.nbTextures = 1;
    model.texture = &texture;
    model.lodData = &lodData;
    assert(generateAutomaticLod(&model, 0));
    assert(automaticLodFaceCount() == 1);

    const string basePath = "wide_sgl_base.ztp";
    assert(saveZTPFile(basePath, &model));
    animated_model_t * baseLoaded = loadZTPFile(basePath);
    assert(baseLoaded && baseLoaded->model[0].nbPoint == 300);
    baseLoaded->lodData->levels.resize(1);
    baseLoaded->lodData->levels[0].reduction = 50;
    baseLoaded->lodData->levels[0].meshes.resize(1);
    baseLoaded->lodData->levels[0].meshes[0].pointCount = 300;
    baseLoaded->lodData->levels[0].meshes[0].faces.push_back(baseLoaded->model[0].pltbl[0]);
    baseLoaded->nbFrames = 1;
    baseLoaded->framerate = 2;
    baseLoaded->keyFrames = new key_frame_t[1]();
    baseLoaded->keyFrames[0].cVert = new vertex_t[300];
    std::copy(baseLoaded->model[0].pntbl, baseLoaded->model[0].pntbl + 300,
        baseLoaded->keyFrames[0].cVert);
    baseLoaded->keyFrames[0].cVert[299].point[Y] = 2.0f;
    pixel_t * originalTexturePixels = baseLoaded->texture[0].pixel;
    assert(replaceAutomaticLodBase(baseLoaded, 75));
    assert(hasAutomaticLodBaseSnapshot(baseLoaded));
    assert(baseLoaded->model[0].nbPoint == 3 && baseLoaded->model[0].nbPolygon == 1);
    assert(automaticLodLevelCount(baseLoaded) == 0);
    assert(baseLoaded->model[0].pltbl[0].texture == 0);
    assert(baseLoaded->texture[0].pixel == originalTexturePixels);
    assert(baseLoaded->keyFrames[0].cVert[2].point[Y] == 2.0f);
    const string reducedBasePath = "wide_sgl_reduced_base.ztp";
    assert(saveZTPFile(reducedBasePath, baseLoaded));
    animated_model_t * reducedLoaded = loadZTPFile(reducedBasePath);
    assert(reducedLoaded && reducedLoaded->model[0].nbPoint == 3 && reducedLoaded->nbFrames == 1);
    destroyModel(reducedLoaded);
    std::remove(reducedBasePath.c_str());
    assert(restoreAutomaticLodBase(baseLoaded));
    assert(!hasAutomaticLodBaseSnapshot(baseLoaded));
    assert(baseLoaded->model[0].nbPoint == 300 && baseLoaded->model[0].nbPolygon == 1);
    assert(automaticLodLevelCount(baseLoaded) == 1 && baseLoaded->lodData->levels[0].reduction == 50);
    assert(baseLoaded->texture[0].pixel == originalTexturePixels);
    assert(baseLoaded->keyFrames[0].cVert[299].point[Y] == 2.0f);
    destroyModel(baseLoaded);
    std::remove(basePath.c_str());

    lodData.levels.resize(1);
    lod_level_t & level = lodData.levels[0];
    level.reduction = 75;
    level.meshes.resize(1);
    for (unsigned int point = 0; point < 258; point += 3)
    {
        polygon_t face = {};
        face.vertIdx[0] = point;
        face.vertIdx[1] = point + 1;
        face.vertIdx[2] = point + 2;
        face.vertIdx[3] = point + 2;
        face.color.rgb = 0x801F;
        face.color.r = 248;
        face.color.a = 255;
        level.meshes[0].faces.push_back(face);
    }

    const string path = "wide_sgl_lod.ztp";
    const string secondPath = "wide_sgl_lod_resaved.ztp";
    assert(saveZTPFile(path, &model));
    vector<uint8_t> bytes = readFileBytes(path);
    assert(std::memcmp(bytes.data(), "ZTP_S01\0", 8) == 0);
    assert(bytes[28] == 0 && bytes[29] == 0 && bytes[30] == 0 && bytes[31] == 3);
    animated_model_t * loaded = loadZTPFile(path);
    assert(loaded && loaded->sourceSgl == 1 && loaded->lodData);
    const lod_mesh_t & loadedLod = loaded->lodData->levels[0].meshes[0];
    assert(loadedLod.pointCount == 258 && loadedLod.faces.size() == 86);
    for (const polygon_t & face : loadedLod.faces)
        for (unsigned int corner = 0; corner < 4; ++corner)
            assert(face.vertIdx[corner] < loadedLod.pointCount);
    assert(saveZTPFile(secondPath, loaded));
    assert(bytes == readFileBytes(secondPath));
    destroyModel(loaded);
    resetAutomaticLod();
    std::remove(path.c_str());
    std::remove(secondPath.c_str());
}

static unsigned int openEdgeCount(const polygon_t * faces, unsigned int count,
    std::vector<std::pair<unsigned int, unsigned int> > * boundary = NULL)
{
    std::map<std::pair<unsigned int, unsigned int>, unsigned int> edges;
    for (unsigned int face = 0; face < count; ++face)
    {
        std::vector<unsigned int> corners;
        for (unsigned int corner = 0; corner < 4; ++corner)
            if (std::find(corners.begin(), corners.end(), faces[face].vertIdx[corner]) == corners.end())
                corners.push_back(faces[face].vertIdx[corner]);
        for (unsigned int corner = 0; corner < corners.size(); ++corner)
        {
            const unsigned int first = corners[corner], second = corners[(corner + 1) % corners.size()];
            ++edges[std::make_pair(MIN(first, second), MAX(first, second))];
        }
    }
    unsigned int open = 0;
    for (const auto & edge : edges) if (edge.second == 1)
    {
        ++open;
        if (boundary) boundary->push_back(edge.first);
    }
    return open;
}

static void checkObjLodBoundaries(const char * path)
{
    extern bool load_OBJ_to_mesh(ifstream *, float *, animated_model_t *, int);
    animated_model_t * model = new animated_model_t();
    model->model = new model_t[256]();
    model->texture = new texture_t[2048]();
    float scale[3] = {1, 1, 1};
    ifstream input(path);
    useUV = 1;
    assert(load_OBJ_to_mesh(&input, scale, model, -1));
    pixel_t pixel = {};
    std::vector<texture_t> atlases(model->nbTextures);
    for (texture_t & atlas : atlases)
    {
        atlas.width = atlas.height = 1;
        atlas.pixel = &pixel;
    }
    cacheAutomaticLodSourceTextures(model, atlases.data(), atlases.size());
    unsigned int sourcePointCount = 0;
    std::vector<unsigned int> sourceBoundaryCounts;
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
    {
        sourcePointCount += model->model[mesh].nbPoint;
        sourceBoundaryCounts.push_back(openEdgeCount(model->model[mesh].pltbl, model->model[mesh].nbPolygon));
    }
    for (unsigned int reduction : {18u, 37u, 75u})
    {
        assert(generateAutomaticLod(model, reduction));
        for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
        {
            unsigned int count = 0;
            const polygon_t * faces = automaticLodFaces(model, mesh, &count);
            std::vector<std::pair<unsigned int, unsigned int> > originalBoundary, reducedBoundary;
            const unsigned int before = openEdgeCount(model->model[mesh].pltbl, model->model[mesh].nbPolygon, &originalBoundary);
            const unsigned int after = openEdgeCount(faces, count, &reducedBoundary);
            std::fprintf(stderr, "OBJ LOD %u%%: %u -> %u points, %u -> %u faces, %u -> %u open edges\n",
                reduction, model->model[mesh].nbPoint, automaticLodPointCount(), model->model[mesh].nbPolygon, count, before, after);
            for (const auto & edge : reducedBoundary)
                assert(std::find(originalBoundary.begin(), originalBoundary.end(), edge) != originalBoundary.end());
            assert(after <= before);
        }
        assert(automaticLodPointCount() < sourcePointCount);
        textureBakeTestEnabled = true;
        assert(replaceAutomaticLodBase(model, reduction));
        textureBakeTestEnabled = false;
        for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
        {
            assert(model->model[mesh].nbPoint <= 256);
            assert(openEdgeCount(model->model[mesh].pltbl, model->model[mesh].nbPolygon) <= sourceBoundaryCounts[mesh]);
            std::fprintf(stderr, "OBJ reduced base %u%%: %u points, %u faces\n", reduction,
                model->model[mesh].nbPoint, model->model[mesh].nbPolygon);
        }
        assert(restoreAutomaticLodBase(model));
    }
    unsigned int originalFaceCount = 0;
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
    {
        originalFaceCount += model->model[mesh].nbPolygon;
        for (unsigned int face = 0; face < model->model[mesh].nbPolygon; ++face)
        {
            model->model[mesh].pltbl[face].texture = face + 1;
            model->model[mesh].pltbl[face].SGL_ATTR.texno = face + 1;
            model->model[mesh].pltbl[face].SGL_ATTR.flag = face & 1;
            model->model[mesh].pltbl[face].SGL_ATTR.sorting = face % 4;
            model->model[mesh].pltbl[face].SGL_ATTR.atrb = face;
        }
    }
    const unsigned int textureBakeCallsBeforeMerge = textureBakeTestCalls;
    textureBakeTestEnabled = true;
    assert(mergeBaseTrianglesIntoQuads(model));
    textureBakeTestEnabled = false;
    assert(textureBakeTestCalls > textureBakeCallsBeforeMerge);
    unsigned int convertedFaceCount = 0;
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
        convertedFaceCount += model->model[mesh].nbPolygon;
    std::fprintf(stderr, "OBJ quads: %u -> %u faces\n", originalFaceCount, convertedFaceCount);
    assert(convertedFaceCount < originalFaceCount);
    discardAutomaticLodSourceTextures(model);
    resetAutomaticLod();
    useUV = 0;
    destroyModel(model);
}

static void checkUnusedTextureExport()
{
    animated_model_t * model = new animated_model_t();
    model->nbModels = 1;
    model->model = new model_t[1]();
    model->model[0].nbPoint = 3;
    model->model[0].pntbl = new vertex_t[3]();
    model->model[0].pntbl[0].point[X] = 1.25f;
    model->model[0].pntbl[0].point[Y] = -2.25f;
    model->model[0].pntbl[1].point[X] = -3.5f;
    model->model[0].pntbl[1].point[Y] = 5.1f;
    model->model[0].pntbl[1].point[Z] = 2.25f;
    model->model[0].pntbl[2].point[X] = 2.0f;
    model->model[0].pntbl[2].point[Y] = 3.0f;
    model->model[0].pntbl[2].point[Z] = -4.0f;
    model->nbFrames = 1;
    model->keyFrames = new key_frame_t[1]();
    model->keyFrames[0].cVert = new vertex_t[3]();
    model->keyFrames[0].cVert[0].point[X] = 20.2f;
    model->keyFrames[0].cVert[0].point[Y] = -8.7f;
    model->keyFrames[0].cVert[0].point[Z] = 3.6f;
    model->keyFrames[0].cVert[1].point[X] = -4.25f;
    model->keyFrames[0].cVert[1].point[Y] = 17.6f;
    model->keyFrames[0].cVert[1].point[Z] = -15.6f;
    model->keyFrames[0].cVert[2].point[X] = 7.0f;
    model->keyFrames[0].cVert[2].point[Y] = 6.0f;
    model->keyFrames[0].cVert[2].point[Z] = 9.0f;
    model->model[0].nbPolygon = 1;
    model->model[0].pltbl = new polygon_t[1]();
    polygon_t & face = model->model[0].pltbl[0];
    face.vertIdx[0] = 0;
    face.vertIdx[1] = 1;
    face.vertIdx[2] = face.vertIdx[3] = 2;
    face.texture = 1;
    face.SGL_ATTR.texno = 1;
    model->nbTextures = 2;
    model->texture = new texture_t[2]();
    for (unsigned int texture = 0; texture < model->nbTextures; ++texture)
    {
        model->texture[texture].width = model->texture[texture].height = 1;
        model->texture[texture].colorDepth = COL_32K;
        model->texture[texture].pixel = new pixel_t[1]();
    }
    model->texture[0].pixel[0].rgb = 0x801F;
    model->texture[1].pixel[0].rgb = 0x83E0;
    model->lodData = new model_lod_data_t;
    model->lodData->levels.resize(1);
    model->lodData->levels[0].reduction = 50;
    model->lodData->levels[0].meshes.resize(1);
    model->lodData->levels[0].meshes[0].pointCount = 3;
    model->lodData->levels[0].meshes[0].faces.push_back(face);
    assert(calculateTextureExportSize(model) == 46);
    assert(calculateTextureExportSize(model, true) == 92);

    const string path = "unused_texture_export.ztp";
    for (int sgl = 0; sgl <= 1; ++sgl)
    {
        USE_SGL = sgl;
        ofstream output(path.c_str(), ios::binary);
        write_model_binary(&output, model, false);
        output.close();
        assert(output.good());
        assert(model->nbTextures == 2 && model->model[0].pltbl[0].texture == 1);

        animated_model_t * reloaded = loadZTPFile(path);
        assert(reloaded && reloaded->nbTextures == 1);
        const float expectedMin[XYZ] = {sgl ? -4.25f : -4.0f, sgl ? -8.6875f : -9.0f,
                                        sgl ? -15.5625f : -16.0f};
        const float expectedMax[XYZ] = {sgl ? 20.1875f : 20.0f, sgl ? 17.5625f : 18.0f, 9.0f};
        for (unsigned int axis = 0; axis < XYZ; ++axis)
        {
            assert(std::fabs(reloaded->BoxMin[axis] - expectedMin[axis]) < 0.0001f);
            assert(std::fabs(reloaded->BoxMax[axis] - expectedMax[axis]) < 0.0001f);
        }
        assert(reloaded->model[0].pltbl[0].texture == 0);
        assert(reloaded->model[0].pltbl[0].SGL_ATTR.texno == 0);
        assert(reloaded->texture[0].pixel[0].rgb == 0x83E0);
        destroyModel(reloaded);

        model->sourceSgl = sgl;
        face.SGL_ATTR.texno = 0;
        assert(saveZTPFile(path, model));
        reloaded = loadZTPFile(path);
        assert(reloaded && reloaded->nbTextures == 1);
        assert(reloaded->model[0].pltbl[0].texture == 0);
        assert(reloaded->model[0].pltbl[0].SGL_ATTR.texno == 0);
        assert(reloaded->lodData && reloaded->lodData->levels.size() == 1);
        assert(reloaded->lodData->levels[0].reduction == 50);
        assert(reloaded->lodData->levels[0].meshes[0].faces.size() == 1);
        assert(reloaded->texture[0].pixel[0].rgb == 0x83E0);
        destroyModel(reloaded);
        face.SGL_ATTR.texno = 1;
    }
    destroyModel(model);
    std::remove(path.c_str());
}

int main(int argumentCount, char **arguments)
{
    if (argumentCount == 3 && std::strcmp(arguments[1], "--obj") == 0)
    {
        checkObjLodBoundaries(arguments[2]);
        return 0;
    }
    animated_model_t * exported = NULL;
    size_t originalSize = 0;
    if (argumentCount > 1)
    {
        exported = loadZTPFile(arguments[1]);
        assert(exported);
        unsigned int sortingCounts[4] = {};
        for (unsigned int mesh = 0; mesh < exported->nbModels; ++mesh)
            for (unsigned int face = 0; face < exported->model[mesh].nbPolygon; ++face)
                ++sortingCounts[exported->model[mesh].pltbl[face].SGL_ATTR.sorting & 3];
        std::cout << "Real model sorting: BFR=" << sortingCounts[SORT_BFR] << ", MIN=" << sortingCounts[SORT_MIN]
            << ", MAX=" << sortingCounts[SORT_MAX] << ", CEN=" << sortingCounts[SORT_CEN] << "\n";
        ifstream source(arguments[1], ios::binary | ios::ate);
        assert(source.good());
        originalSize = static_cast<size_t>(source.tellg());
        if (originalSize >= 32)
        {
            char signature[4] = {};
            source.seekg(0);
            source.read(signature, sizeof(signature));
            if (source.gcount() == sizeof(signature) && std::memcmp(signature, "ZTP_", 4) == 0)
                originalSize -= 32;
        }
    }

    char originalDirectory[MAX_PATH] = {}, temporaryDirectory[MAX_PATH] = {}, temporaryPath[MAX_PATH] = {};
    assert(GetCurrentDirectoryA(MAX_PATH, originalDirectory));
    diagnosticImagePath = string(originalDirectory) + "\\obj\\Debug\\lod_texture_diagnostic.png";
    assert(GetTempPathA(MAX_PATH, temporaryDirectory));
    assert(GetTempFileNameA(temporaryDirectory, "ztp", 0, temporaryPath));
    assert(DeleteFileA(temporaryPath));
    assert(CreateDirectoryA(temporaryPath, NULL));
    assert(SetCurrentDirectoryA(temporaryPath));
    assert(CreateDirectoryA("OUT", NULL));
    checkLegacyRawSorting();
    checkSortingModes();
    checkLodFile(false);
    checkLodFile(true);
    checkNonSglReducedBaseFromLargeSource();
    checkAutomaticLodTextureRebake();
    checkUnusedTextureExport();
    checkFoldedLodUvTransfer();
    checkWideSglLod();
    if (exported)
    {
        checkRoundTrip(exported, COL_16, "real_model_TEST.ZTP", originalSize);
        assert(generateAutomaticLod(exported, 75));
        assert(generateAutomaticLod(exported, 90));
        assert(automaticLodLevelCount(exported) == 2);
        assert(generateAutomaticLod(exported, 95, 2));
        assert(automaticLodLevelCount(exported) == 2 && exported->lodData->levels[1].reduction == 95);
        assert(saveZTPFile("real_lods.ztp", exported));
        animated_model_t * lodModel = loadZTPFile("real_lods.ztp");
        assert(lodModel && automaticLodLevelCount(lodModel) == 2);
        assert(lodModel->lodData->levels[1].reduction == 95);
        assert(selectAutomaticLod(lodModel, 2));
        assert(automaticLodFaceCount() <= exported->model[0].nbPolygon);
        assert(saveZTPFile("real_lods_again.ztp", lodModel));
        assert(readFileBytes("real_lods.ztp") == readFileBytes("real_lods_again.ztp"));
        resetAutomaticLod();
        destroyModel(lodModel);
        std::remove("real_lods.ztp"); std::remove("real_lods_again.ztp");
        destroyModel(exported);
    }

    const string indexedPath = "ztp_indexed_fixture.ztp";
    animated_model_t *indexed = loadFixture(COL_16, indexedPath);
    assert(indexed);
    assert(indexed->nbModels == 1 && indexed->nbTextures == 1 && indexed->nbFrames == 1);
    assert(indexed->model[0].nbPoint == 3 && indexed->model[0].nbPolygon == 1);
    assert(indexed->model[0].pntbl[1].point[X] == 10);
    assert(indexed->model[0].pntbl[0].normal[Z] == 1.0f);
    assert(indexed->model[0].pntbl[0].normalArray == 161);
    assert(indexed->keyFrames[0].cVert[0].normalArray == 7);
    assert(indexed->model[0].pltbl[0].vertIdx[2] == 2);
    assert(indexed->model[0].pltbl[0].vertIdx[3] == 2);
    assert(indexed->model[0].pltbl[0].texture == 0);
    assert(indexed->model[0].pltbl[0].SGL_ATTR.sorting == 2);
    assert(indexed->model[0].pltbl[0].SGL_ATTR.flag == 1);
    assert(indexed->keyFrames[0].cVert[1].point[X] == 20);
    assert(indexed->BoxMin[X] == 0 && indexed->BoxMin[Y] == 0 && indexed->BoxMin[Z] == 0);
    assert(indexed->BoxMax[X] == 20 && indexed->BoxMax[Y] == 20 && indexed->BoxMax[Z] == 0);
    assert(indexed->texture[0].pixel[0].palette_idx == 1);
    assert(indexed->texture[0].pixel[0].rgb == 0x801F);
    checkRoundTrip(indexed, COL_16, "indexed_TEST.ZTP");
    destroyModel(indexed);

    const string rgbPath = "ztp_rgb_fixture.ztp";
    animated_model_t *rgb = loadFixture(COL_32K, rgbPath);
    assert(rgb && rgb->texture[0].pixel[1].rgb == 0x83E0);
    checkRoundTrip(rgb, COL_32K, "rgb_TEST.ZTP");
    destroyModel(rgb);

    const string multiPath = "ztp_multi_fixture.ztp";
    animated_model_t * multi = loadFixture(COL_16, multiPath, 2, 2);
    assert(multi && multi->nbModels == 2);
    checkRoundTrip(multi, COL_16, "multi_TEST.ZTP");
    destroyModel(multi);

    const string staticPath = "ztp_static_fixture.ztp";
    animated_model_t * staticModel = loadFixture(COL_32K, staticPath, 1, 0);
    assert(staticModel && staticModel->nbFrames == 0);
    checkRoundTrip(staticModel, COL_32K, "static_TEST.ZTP");
    destroyModel(staticModel);

    vector<uint8_t> truncated = makeFixture(COL_16);
    truncated.resize(truncated.size() - 2);
    const string truncatedPath = "ztp_truncated_fixture.ztp";
    ofstream truncatedFile(truncatedPath.c_str(), ios::binary);
    truncatedFile.write(reinterpret_cast<const char *>(truncated.data()), truncated.size());
    truncatedFile.close();
    assert(loadZTPFile(truncatedPath) == NULL);
    std::remove(indexedPath.c_str());
    std::remove(rgbPath.c_str());
    std::remove(truncatedPath.c_str());
    std::remove(multiPath.c_str());
    std::remove(staticPath.c_str());
    std::remove("OUT/TEXTURES.TXT");
    assert(RemoveDirectoryA("OUT"));
    assert(SetCurrentDirectoryA(originalDirectory));
    assert(RemoveDirectoryA(temporaryPath));
    std::cout << "ZTP reader/writer round-trip tests passed\n";
    return 0;
}