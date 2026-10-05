#include "../SRC/COMMON.H"
#include <cassert>
#include <cstdio>

uv_t texture_coord[maxPolygons * 4];
uint32_t tot_tex_coord = 0;
uint32_t useUV = 1;
int USE_SGL = 0;
uint32_t specialOption = 0;

bool load_OBJ_to_mesh(ifstream *file, float *scalefactor, animated_model_t *aModel, int keyFrameID);
animated_model_t * loadingConfigured(unsigned int, int, float, uint32_t, uint32_t, string, string, bool);
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
    reference = loadingConfigured(0, 0, 1.0f, 0, 0, "tests/", "missing-reference-file", true);
    assert(reference->nbModels == 0);
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