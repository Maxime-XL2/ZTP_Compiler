#include "../SRC/COMMON.H"
#include <cassert>
#include <cstdio>
#include <vector>
#include <iostream>
#include <iterator>

animated_model_t *loadZTPFile(const string &filePath);
void write_model_binary(ofstream *, animated_model_t *, bool);
uv_t texture_coord[maxPolygons * 4];
uint32_t tot_tex_coord = 0;
uint32_t useUV = 0;
uint32_t specialOption = 0;
extern int USE_SGL;
bool bQuit = false;
void recalculateNormals(model_t *) {}
void pointNormals(model_t *) {}
int ReadTGAFile(string, texture_t *, string) { return 0; }
void createFakeTexture(texture_t *) {}
void createTexture(polygon_t *, animated_model_t *, const texture_t &) {}
void calcLight(animated_model_t *) {}

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
    assert(outputSize == (expectedSize ? expectedSize : makeFixture(depth, original->nbModels, original->nbFrames).size()));
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

int main(int argumentCount, char **arguments)
{
    animated_model_t * exported = NULL;
    size_t originalSize = 0;
    if (argumentCount > 1)
    {
        exported = loadZTPFile(arguments[1]);
        assert(exported);
        ifstream source(arguments[1], ios::binary | ios::ate);
        assert(source.good());
        originalSize = static_cast<size_t>(source.tellg());
    }

    char originalDirectory[MAX_PATH] = {}, temporaryDirectory[MAX_PATH] = {}, temporaryPath[MAX_PATH] = {};
    assert(GetCurrentDirectoryA(MAX_PATH, originalDirectory));
    assert(GetTempPathA(MAX_PATH, temporaryDirectory));
    assert(GetTempFileNameA(temporaryDirectory, "ztp", 0, temporaryPath));
    assert(DeleteFileA(temporaryPath));
    assert(CreateDirectoryA(temporaryPath, NULL));
    assert(SetCurrentDirectoryA(temporaryPath));
    assert(CreateDirectoryA("OUT", NULL));
    if (exported)
    {
        checkRoundTrip(exported, COL_16, "real_model_TEST.ZTP", originalSize);
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