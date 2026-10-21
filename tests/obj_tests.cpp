#include "../SRC/COMMON.H"
#include <cassert>
#include <cmath>
#include <cstdio>

uv_t texture_coord[maxPolygons * 4];
uint32_t tot_tex_coord = 0;
uint32_t useUV = 1;
int USE_SGL = 0;
uint32_t specialOption = 0;
uint32_t maxGeneratedTextureWidth = 512;
uint32_t maxGeneratedTextureHeight = 256;
string fbxLoadError;

bool load_OBJ_to_mesh(ifstream *file, float *scalefactor, animated_model_t *aModel, int keyFrameID);
animated_model_t * loadingConfigured(unsigned int, int, float, uint32_t, uint32_t, string, string, bool);
bool loadFBX(const string &, float *, animated_model_t *, int, int, int, float, float, float) { return false; }
void rotateModelData(animated_model_t *, int, int, int);
void translateModelData(animated_model_t *, float, float, float);
void findScale(animated_model_t *, bool);
extern string fileOutName;
unsigned int paletteWrites = 0;

void recalculateNormals(model_t *) {}
void pointNormals(model_t *) {}
bool bQuit = false;

int ReadTGAFile(string, texture_t *, string) { return 0; }
void createFakeTexture(texture_t *) {}
void createTexture(polygon_t *, animated_model_t *, const texture_t &) {}
void writePalette(animated_model_t *) { ++paletteWrites; }
void calculateBoundingBox(float *, float *, vertex_t *, unsigned int) {}
void calcLight(animated_model_t *) {}
void cacheAutomaticLodSourceTextures(animated_model_t *, const texture_t *, unsigned int) {}

static animated_model_t *loadOBJ(const string &path, bool &loaded)
{
    animated_model_t *model = new animated_model_t();
    ZeroMemory(model, sizeof(animated_model_t));
    model->model = new model_t[256]();
    model->texture = new texture_t[2048]();
    float scale[3] = {1, 1, 1};
    ifstream file(path.c_str());
    loaded = load_OBJ_to_mesh(&file, scale, model, -1);
    return model;
}

static void destroyModel(animated_model_t *model)
{
    for (unsigned int frame = 0; frame < model->nbFrames; ++frame)
        delete [] model->keyFrames[frame].cVert;
    delete [] model->keyFrames;
    for (unsigned int mesh = 0; mesh < 256; ++mesh)
    {
        delete [] model->model[mesh].pntbl;
        delete [] model->model[mesh].pltbl;
    }
    delete [] model->model;
    delete [] model->texture;
    delete model;
}

static double triangleArea(const model_t &mesh, const polygon_t &polygon)
{
    unsigned int indices[3];
    unsigned int count = 0;
    for (unsigned int corner = 0; corner < 4; ++corner)
    {
        bool found = false;
        for (unsigned int existing = 0; existing < count; ++existing)
            if (indices[existing] == polygon.vertIdx[corner]) found = true;
        if (!found) indices[count++] = polygon.vertIdx[corner];
    }
    assert(count == 3);
    const float *a = mesh.pntbl[indices[0]].point;
    const float *b = mesh.pntbl[indices[1]].point;
    const float *c = mesh.pntbl[indices[2]].point;
    return std::fabs((b[X] - a[X]) * (c[Y] - a[Y]) - (b[Y] - a[Y]) * (c[X] - a[X])) * 0.5;
}

int main()
{
    const int previousSgl = USE_SGL;
    USE_SGL = 0;
    vertex_t basePoints[2] = {};
    basePoints[0].point[X] = 2.0f;
    basePoints[1].point[X] = 4.0f;
    vertex_t animatedPoints[2] = {};
    animatedPoints[0].point[X] = 5.0f;
    animatedPoints[1].point[X] = 40.0f;
    animatedPoints[1].point[Y] = 20.0f;
    model_t scaleMeshes[2] = {};
    scaleMeshes[0].nbPoint = 1;
    scaleMeshes[0].pntbl = &basePoints[0];
    scaleMeshes[1].nbPoint = 1;
    scaleMeshes[1].pntbl = &basePoints[1];
    key_frame_t scaleFrame = {};
    scaleFrame.cVert = animatedPoints;
    animated_model_t scaleModel = {};
    scaleModel.nbModels = 2;
    scaleModel.model = scaleMeshes;
    scaleModel.nbFrames = 1;
    scaleModel.keyFrames = &scaleFrame;
    findScale(&scaleModel, false);
    assert(scaleModel.scale[X] == 1.0f && animatedPoints[1].point[X] == 40.0f);
    findScale(&scaleModel, true);
    assert(std::fabs(scaleModel.scale[X] - 40.0f / 126.5f) < 0.000001f);
    assert(std::fabs(scaleModel.scale[Y] - 20.0f / 126.5f) < 0.000001f);
    assert(std::fabs(animatedPoints[1].point[X] - 126.5f) < 0.001f);
    assert(std::fabs(animatedPoints[1].point[Y] - 126.5f) < 0.001f);
    assert(std::fabs(basePoints[1].point[X] - 4.0f / scaleModel.scale[X]) < 0.001f);
    USE_SGL = previousSgl;

    vertex_t basePoint = {};
    basePoint.point[X] = 1;
    basePoint.point[Y] = 2;
    basePoint.point[Z] = 3;
    basePoint.normal[Y] = 1;
    vertex_t animationPoint = basePoint;
    polygon_t face = {};
    face.normal[Y] = 1;
    model_t mesh = {};
    mesh.nbPoint = 1;
    mesh.nbPolygon = 1;
    mesh.pntbl = &basePoint;
    mesh.pltbl = &face;
    key_frame_t frame = {};
    frame.cVert = &animationPoint;
    animated_model_t rotated = {};
    rotated.nbModels = 1;
    rotated.nbFrames = 1;
    rotated.model = &mesh;
    rotated.keyFrames = &frame;
    rotateModelData(&rotated, 1, 1, 1);
    assert(basePoint.point[X] == 3 && basePoint.point[Y] == 2 && basePoint.point[Z] == -1);
    assert(animationPoint.point[X] == 3 && animationPoint.point[Y] == 2 && animationPoint.point[Z] == -1);
    assert(basePoint.normal[X] == 0 && basePoint.normal[Y] == 1 && basePoint.normal[Z] == 0);
    assert(animationPoint.normal[X] == 0 && animationPoint.normal[Y] == 1 && animationPoint.normal[Z] == 0);
    assert(face.normal[X] == 0 && face.normal[Y] == 1 && face.normal[Z] == 0);
    translateModelData(&rotated, 10, -2, 3);
    assert(basePoint.point[X] == 13 && basePoint.point[Y] == 0 && basePoint.point[Z] == 2);
    assert(animationPoint.point[X] == 13 && animationPoint.point[Y] == 0 && animationPoint.point[Z] == 2);
    assert(mesh.BoxMin[X] == 13 && mesh.BoxMin[Y] == 0 && mesh.BoxMin[Z] == 2);
    assert(mesh.BoxMax[X] == 13 && mesh.BoxMax[Y] == 0 && mesh.BoxMax[Z] == 2);
    assert(rotated.BoxMin[X] == 13 && rotated.BoxMin[Y] == 0 && rotated.BoxMin[Z] == 2);
    assert(rotated.BoxMax[X] == 13 && rotated.BoxMax[Y] == 0 && rotated.BoxMax[Z] == 2);

    bool loaded = false;
    animated_model_t *brute = loadOBJ("tests/Brute1.obj", loaded);
    assert(loaded);
    assert(brute->nbModels == 2);
    assert(brute->model[0].nbPoint == 233);
    unsigned int bruteFaceCount = 0;
    for (unsigned int mesh = 0; mesh < brute->nbModels; ++mesh)
    {
        bruteFaceCount += brute->model[mesh].nbPolygon;
        for (unsigned int face = 0; face < brute->model[mesh].nbPolygon; ++face)
            for (unsigned int corner = 0; corner < 4; ++corner)
                assert(brute->model[mesh].pltbl[face].vertIdx[corner] < brute->model[mesh].nbPoint);
    }
    assert(bruteFaceCount == 454);
    destroyModel(brute);

    fileOutName = "working-model";
    animated_model_t * reference = loadingConfigured(0, 2, 1.0f, 0, 0, "tests/", "Brute1", true);
    assert(reference->nbModels == 2);
    assert(fileOutName == "working-model");
    assert(paletteWrites == 0);
    destroyModel(reference);
    reference = loadingConfigured(1025, 0, 1.0f, 0, 0, "tests/", "missing-reference-file", true);
    assert(reference->nbModels == 0);
    assert(reference->nbFrames == ZTP_MAX_ANIMATION_FRAMES);
    assert(fileOutName == "working-model");
    destroyModel(reference);
    USE_SGL = 0;
    useUV = 1;
    specialOption = 0;
    tot_tex_coord = 0;

    const string fixturePath = "obj/Debug/obj_loader_fixture.obj";
    {
        ofstream fixture(fixturePath.c_str());
        fixture << "v 0 0 0\n"
               "v 2 0 0\n"
               "v 1 0.5 0\n"
               "v 2 2 0\n"
               "v 0 2 0\n"
                   "vt 0 0\n"
                   "vt 1 0\n"
                   "vt 1 1\n"
                   "vt 0.5 1\n"
                   "vt 0 1\n"
                   "vn 0 0 1\n"
                   "g pentagon\n"
                   "f -5/-5/-1 -4//-1 -3/-3/-1 -2/-2/-1 -1/-1/-1\n";
        assert(fixture.good());
    }
    animated_model_t *pentagon = loadOBJ(fixturePath, loaded);
    assert(loaded);
    assert(pentagon->nbModels == 1);
    assert(pentagon->model[0].nbPoint == 5);
    assert(pentagon->model[0].nbPolygon == 3);
    double pentagonArea = 0;
    for (unsigned int face = 0; face < pentagon->model[0].nbPolygon; ++face)
    {
        const polygon_t &polygon = pentagon->model[0].pltbl[face];
        assert(polygon.vertIdx[2] == polygon.vertIdx[3]);
        pentagonArea += triangleArea(pentagon->model[0], polygon);
        for (unsigned int corner = 0; corner < 4; ++corner)
            assert(polygon.vertIdx[corner] < pentagon->model[0].nbPoint);
    }
    assert(std::fabs(pentagonArea - 3.0) < 0.0001);
    destroyModel(pentagon);
    assert(std::remove(fixturePath.c_str()) == 0);

    {
        ofstream fixture(fixturePath.c_str());
        fixture << "v 0 0 0\n"
                   "v 2 0 0\n"
                   "v 1 0.5 0\n"
                   "v 0 2 0\n"
                   "g concave_quad\n"
                   "f 1 2 3 4\n";
        assert(fixture.good());
    }
    animated_model_t *quad = loadOBJ(fixturePath, loaded);
    assert(loaded);
    assert(quad->model[0].nbPolygon == 2);
    double quadArea = 0;
    for (unsigned int face = 0; face < quad->model[0].nbPolygon; ++face)
        quadArea += triangleArea(quad->model[0], quad->model[0].pltbl[face]);
    assert(std::fabs(quadArea - 1.5) < 0.0001);
    destroyModel(quad);
    assert(std::remove(fixturePath.c_str()) == 0);

    {
        ofstream fixture(fixturePath.c_str());
        fixture << "v 0 0 0\n"
                   "v 1 0 0\n"
                   "v 1 1 0\n"
                   "v 0 1 0\n"
                   "g first\n"
                   "f 1 2 3\n"
                   "g second\n"
                   "f 4 1 2\n";
        assert(fixture.good());
    }
    animated_model_t *animated = loadOBJ(fixturePath, loaded);
    assert(loaded && animated->nbModels == 2);
    assert(animated->model[0].nbPoint == 4);
    assert(animated->model[1].nbPoint == 3);
    animated->nbFrames = 1;
    animated->keyFrames = new key_frame_t[1]();
    animated->keyFrames[0].cVert = new vertex_t[7]();
    {
        ofstream fixture(fixturePath.c_str());
        fixture << "v 10 0 0\n"
                   "v 11 0 0\n"
                   "v 11 1 0\n"
                   "v 13 1 0\n";
        assert(fixture.good());
    }
    {
        ifstream frameFile(fixturePath.c_str());
        float scale[3] = {1, 1, 1};
        assert(load_OBJ_to_mesh(&frameFile, scale, animated, 0));
    }
    assert(animated->keyFrames[0].cVert[0].point[X] == 10);
    assert(animated->keyFrames[0].cVert[4].point[X] == 13);
    assert(animated->keyFrames[0].cVert[5].point[X] == 10);
    destroyModel(animated);
    assert(std::remove(fixturePath.c_str()) == 0);

    {
        ofstream fixture(fixturePath.c_str());
        fixture << "v 0 0 0\n"
                   "v 1 0 0\n"
                   "v 0 1 0\n"
                   "f 1 0 -1\n";
        assert(fixture.good());
    }
    animated_model_t *invalid = loadOBJ(fixturePath, loaded);
    assert(!loaded);
    destroyModel(invalid);
    assert(std::remove(fixturePath.c_str()) == 0);

    cout << "OBJ loader tests passed\n";
}