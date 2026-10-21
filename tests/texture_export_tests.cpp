#include "../SRC/COMMON.H"
#include "../SRC/ZTP/mtl.h"
#include <cassert>
#include <cstdio>
#include <algorithm>
#include <cstring>

uint32_t useUV = 1;
uint32_t specialOption = 0;
texture_t skin = {};
bool bQuit = false;
uv_t texture_coord[maxPolygons * 4];
uint32_t tot_tex_coord = 0;
void calcLight(animated_model_t *) {}
void rotatePol(polygon_t * polygon)
{
    const polygon_t original = *polygon;
    for (unsigned int corner = 0; corner < 4; ++corner)
    {
        polygon->vertIdx[corner] = original.vertIdx[(corner + 1) & 3];
        polygon->vertUV[corner] = original.vertUV[(corner + 1) & 3];
    }
}
void textureFlip(polygon_t * polygon)
{
    polygon->SGL_ATTR.dir = (polygon->SGL_ATTR.dir & 0xFFCF) | ((polygon->SGL_ATTR.dir + (1 << 4)) & 0x0030);
}
extern int USE_SGL;
extern animated_model_t * loadZTPFile(const string &);
extern bool load_OBJ_to_mesh(ifstream *, float *, animated_model_t *, int);
extern bool loadFBX(const string &, float *, animated_model_t *, int, int, int, float, float, float);
extern string fbxLoadError;
extern int ReadTGAFile(string, texture_t *, string);
extern void createTexture(polygon_t *, animated_model_t *, const texture_t &);
extern void write_model_binary(ofstream *, animated_model_t *, bool);
extern void discardAutomaticLodSourceTextures(const animated_model_t *);
extern void discardAutomaticLodBaseSnapshot(const animated_model_t *);

static void checkExport(animated_model_t & model, const string & path, bool preserve)
{
    const uint64_t expectedBytes = calculateTextureExportSize(&model, false);
    ofstream output(path, ios::binary);
    write_model_binary(&output, &model, preserve);
    output.close();
    assert(output.good());
    animated_model_t * loaded = loadZTPFile(path);
    assert(loaded);
    vector<bool> used(loaded->nbTextures, false);
    for (unsigned int mesh = 0; mesh < loaded->nbModels; ++mesh)
        for (unsigned int face = 0; face < loaded->model[mesh].nbPolygon; ++face)
            used.at(loaded->model[mesh].pltbl[face].texture) = true;
    assert(std::find(used.begin(), used.end(), false) == used.end());
    assert(calculateTextureExportSize(loaded, true) == expectedBytes);
    unsigned int directColorCount = 0;
    for (unsigned int texture = 0; texture < loaded->nbTextures; ++texture)
        if (loaded->texture[texture].colorDepth == COL_32K) ++directColorCount;
    ifstream bytes(path, ios::binary | ios::ate);
    std::fprintf(stderr, "Export: %u used / %u resident textures, %llu texture bytes, %lld file bytes\n",
        loaded->nbTextures, model.nbTextures, static_cast<unsigned long long>(expectedBytes),
        static_cast<long long>(bytes.tellg()));
    std::fprintf(stderr, "Texture formats: %u direct-color, %u palette; preserve=%u\n",
        directColorCount, loaded->nbTextures - directColorCount, preserve ? 1 : 0);
    bytes.close();
    std::remove(path.c_str());
}

static size_t countQuadTextureMismatches(const animated_model_t & model, const texture_t & source,
                                         size_t & sampleCount, unsigned int & quadCount)
{
    size_t mismatches = 0;
    sampleCount = 0;
    quadCount = 0;
    for (unsigned int meshIndex = 0; meshIndex < model.nbModels; ++meshIndex)
    {
        const model_t & mesh = model.model[meshIndex];
        for (unsigned int faceIndex = 0; faceIndex < mesh.nbPolygon; ++faceIndex)
        {
            const polygon_t & face = mesh.pltbl[faceIndex];
            if (face.vertIdx[2] == face.vertIdx[3] || face.texture < 0 ||
                face.texture >= static_cast<int>(model.nbTextures)) continue;
            const texture_t & generated = model.texture[face.texture];
            if (!generated.pixel || !generated.width || !generated.height) continue;
            ++quadCount;
            const unsigned int direction = (face.SGL_ATTR.dir >> 4) & 3;
            for (unsigned int y = 0; y < generated.height; ++y)
                for (unsigned int x = 0; x < generated.width; ++x)
                {
                    float horizontal = float(x) / generated.width;
                    float vertical = float(y) / generated.height;
                    if (direction & 1) horizontal = 1.0f - horizontal;
                    if (direction & 2) vertical = 1.0f - vertical;
                    const float leftU = face.vertUV[0].U * (1.0f - vertical) + face.vertUV[3].U * vertical;
                    const float rightU = face.vertUV[1].U * (1.0f - vertical) + face.vertUV[2].U * vertical;
                    const float leftV = face.vertUV[0].V * (1.0f - vertical) + face.vertUV[3].V * vertical;
                    const float rightV = face.vertUV[1].V * (1.0f - vertical) + face.vertUV[2].V * vertical;
                    const unsigned int sourceX = static_cast<unsigned int>(MAX(0, MIN(
                        static_cast<int>(source.width) - 1,
                        static_cast<int>((leftU * (1.0f - horizontal) + rightU * horizontal) * source.width))));
                    const unsigned int sourceY = static_cast<unsigned int>(MAX(0, MIN(
                        static_cast<int>(source.height) - 1,
                        static_cast<int>((leftV * (1.0f - horizontal) + rightV * horizontal) * source.height))));
                    const pixel_t & expected = source.pixel[sourceY * source.width + sourceX];
                    const pixel_t & actual = generated.pixel[y * generated.width + x];
                    ++sampleCount;
                    if (actual.r != expected.r || actual.g != expected.g || actual.b != expected.b)
                        ++mismatches;
                }
        }
    }
    return mismatches;
}

static bool testFbxBaseReplacementTextures(const string & fbxPath, const string & sourceTexturePath)
{
    USE_SGL = 0;
    useUV = 1;
    specialOption = 0;
    texture_t source = {};
    source.name = "Male3 source";
    if (ReadTGAFile("", &source, sourceTexturePath) <= 0 || !source.pixel) return false;

    animated_model_t model = {};
    model.sourceSgl = 0;
    model.model = new model_t[256]();
    float scale[3] = {1.0f, 1.0f, 1.0f};
    if (!loadFBX(fbxPath, scale, &model, 0, 0, 0, 0.0f, 0.0f, 0.0f))
    {
        std::cerr << fbxLoadError << "\n";
        delete [] source.pixel;
        delete [] model.model;
        return false;
    }

    size_t beforeSamples = 0;
    unsigned int beforeQuads = 0;
    const size_t beforeMismatches = countQuadTextureMismatches(
        model, source, beforeSamples, beforeQuads);
    const bool replaced = replaceAutomaticLodBase(&model, 37);
    size_t afterSamples = 0;
    unsigned int afterQuads = 0;
    const size_t afterMismatches = replaced ?
        countQuadTextureMismatches(model, source, afterSamples, afterQuads) : 0;
    std::cout << "Male3 quad texture samples: before " << beforeMismatches << "/" << beforeSamples
              << " mismatches across " << beforeQuads << " quads; after " << afterMismatches
              << "/" << afterSamples << " across " << afterQuads << " quads\n";

    const bool restored = replaced && restoreAutomaticLodBase(&model);
    const bool replacedAgain = restored && replaceAutomaticLodBase(&model, 37);
    const bool restoredAgain = replacedAgain && restoreAutomaticLodBase(&model);
    std::cout << "Male3 replace/restore/replace/restore cycle: "
              << (restoredAgain ? "passed" : "failed") << "\n";

    discardAutomaticLodBaseSnapshot(&model);
    discardAutomaticLodSourceTextures(&model);
    for (unsigned int frame = 0; frame < model.nbFrames; ++frame)
        delete [] model.keyFrames[frame].cVert;
    for (unsigned int mesh = 0; mesh < model.nbModels; ++mesh)
    {
        delete [] model.model[mesh].pntbl;
        delete [] model.model[mesh].pltbl;
    }
    for (unsigned int texture = 0; texture < model.nbTextures; ++texture)
        delete [] model.texture[texture].pixel;
    delete [] model.keyFrames;
    delete [] model.model;
    delete [] model.texture;
    delete [] source.pixel;
    return replaced && beforeQuads > 0 && afterQuads > 0 && beforeSamples > 0 && afterSamples > 0 &&
        afterMismatches * beforeSamples <= beforeMismatches * afterSamples && restoredAgain;
}

int main(int argumentCount, char ** arguments)
{
    if (argumentCount == 4 && std::strcmp(arguments[1], "--fbx") == 0)
        return testFbxBaseReplacementTextures(arguments[2], arguments[3]) ? 0 : 1;
    assert(argumentCount == 2 || argumentCount == 3);
    if (argumentCount == 3) specialOption = 1;
    const string path = arguments[1];
    const string folder = ztp_mtl::directory(path);
    ifstream input(path);
    assert(input.good());
    const auto materials = ztp_mtl::load(input, path, cout);
    animated_model_t model = {};
    model.model = new model_t[256]();
    model.texture = new texture_t[2048]();
    float scale[3] = {1, 1, 1};
    assert(load_OBJ_to_mesh(&input, scale, &model, -1));
    vector<texture_t> sources(model.texture, model.texture + model.nbTextures);
    for (texture_t & source : sources)
    {
        const auto material = materials.find(source.name);
        ReadTGAFile(folder, &source, material == materials.end() ? "" : material->second);
        assert(source.pixel && source.width && source.height);
    }
    cacheAutomaticLodSourceTextures(&model, sources.data(), sources.size());
    model.nbTextures = 0;
    for (unsigned int mesh = 0; mesh < model.nbModels; ++mesh)
        for (unsigned int face = 0; face < model.model[mesh].nbPolygon; ++face)
        {
            polygon_t & polygon = model.model[mesh].pltbl[face];
            createTexture(&polygon, &model, sources.at(polygon.texture));
        }
    USE_SGL = 1;
    model.sourceSgl = 1;
    char temporaryFolder[MAX_PATH] = {}, temporaryPath[MAX_PATH] = {};
    assert(GetTempPathA(MAX_PATH, temporaryFolder));
    assert(GetTempFileNameA(temporaryFolder, "ztp", 0, temporaryPath));
    checkExport(model, temporaryPath, false);
    USE_SGL = 0;
    model.sourceSgl = 0;
    assert(mergeBaseTrianglesIntoQuads(&model));
    unsigned int quadFaceCount = model.model[0].nbPolygon;
    const bool replaced = replaceAutomaticLodBase(&model, 1);
    std::fprintf(stderr, "Non-SGL 1%% replacement: %s, %u points, %u faces (from %u quads)\n",
        replaced ? "success" : "failed", model.model[0].nbPoint, model.model[0].nbPolygon, quadFaceCount);
    assert(replaced);
    assert(model.model[0].nbPoint <= 256);
    assert(model.model[0].nbPolygon >= quadFaceCount * 9 / 10);
    for (unsigned int face = 0; face < model.model[0].nbPolygon; ++face)
        for (unsigned int corner = 0; corner < 4; ++corner)
            assert(model.model[0].pltbl[face].vertIdx[corner] < model.model[0].nbPoint);
    checkExport(model, temporaryPath, false);
    checkExport(model, temporaryPath, true);
    std::fprintf(stderr, "Real texture export tests passed\n");
}