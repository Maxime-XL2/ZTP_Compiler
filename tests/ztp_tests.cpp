#include "../SRC/COMMON.H"
#include <cassert>
#include <cstdio>
#include <vector>
#include <iostream>
#include <iterator>
#include <cstring>
#include <cstddef>

static_assert(sizeof(ATTR) == 12, "ATTR serialized layout must remain 12 bytes");
static_assert(offsetof(ATTR, sorting) == 1, "Sorting must remain the second attribute byte");

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

int main(int argumentCount, char **arguments)
{
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
    }

    char originalDirectory[MAX_PATH] = {}, temporaryDirectory[MAX_PATH] = {}, temporaryPath[MAX_PATH] = {};
    assert(GetCurrentDirectoryA(MAX_PATH, originalDirectory));
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