#include "../SRC/COMMON.H"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

int main()
{
    const unsigned int rings = 12, sides = 12;
    std::vector<vertex_t> points(rings * sides);
    std::vector<polygon_t> faces(rings * sides);
    for (unsigned int ring = 0; ring < rings; ++ring)
        for (unsigned int side = 0; side < sides; ++side)
        {
            unsigned int index = ring * sides + side;
            float longitude = ring * 2.0f * M_PI / rings;
            float latitude = side * 2.0f * M_PI / sides;
            points[index].point[X] = (4.0f + std::cos(latitude)) * std::cos(longitude);
            points[index].point[Y] = std::sin(latitude);
            points[index].point[Z] = (4.0f + std::cos(latitude)) * std::sin(longitude);
            faces[index] = {};
            faces[index].vertIdx[0] = index;
            faces[index].vertIdx[1] = ((ring + 1) % rings) * sides + side;
            faces[index].vertIdx[2] = ((ring + 1) % rings) * sides + (side + 1) % sides;
            faces[index].vertIdx[3] = ring * sides + (side + 1) % sides;
        }
    std::vector<vertex_t> originalPoints = points;
    std::vector<polygon_t> originalFaces = faces;
    model_t mesh = {};
    mesh.nbPoint = points.size();
    mesh.nbPolygon = faces.size();
    mesh.pntbl = points.data();
    mesh.pltbl = faces.data();
    animated_model_t model = {};
    model.nbModels = 1;
    model.model = &mesh;
    pixel_t pixels[3] = {};
    pixels[0].r = 255;
    pixels[0].a = 255;
    pixels[1].b = 255;
    pixels[1].a = 255;
    pixels[2].g = 255;
    texture_t texture;
    texture.width = 3;
    texture.height = 1;
    texture.pixel = pixels;
    model.nbTextures = 1;
    model.texture = &texture;
    assert(generateAutomaticLod(&model, 0));
    assert(automaticLodFaceCount() == faces.size());
    assert(generateAutomaticLod(&model, 75));
    unsigned int count = 0;
    const polygon_t * lod = automaticLodFaces(&model, 0, &count);
    printf("Static torus: %u -> %u faces, %u -> %u points\n", mesh.nbPolygon, count, mesh.nbPoint, automaticLodPointCount());
    assert(lod && count > 0 && count < faces.size());
    for (unsigned int face = 0; face < count; ++face)
    {
        assert(lod[face].color.r == 128 && lod[face].color.g == 0 && lod[face].color.b == 128);
        assert(lod[face].color.a == 255);
        assert(lod[face].vertIdx[3] == lod[face].vertIdx[2]);
    }
    for (unsigned int face = 0; face < count; ++face)
        for (unsigned int corner = 0; corner < 3; ++corner)
        {
            assert(lod[face].vertIdx[corner] < mesh.nbPoint);
            for (unsigned int other = corner + 1; other < 3; ++other)
                assert(lod[face].vertIdx[corner] != lod[face].vertIdx[other]);
        }
    assert(std::memcmp(points.data(), originalPoints.data(), points.size() * sizeof(vertex_t)) == 0);
    assert(std::memcmp(faces.data(), originalFaces.data(), faces.size() * sizeof(polygon_t)) == 0);
    std::vector<vertex_t> nextPose = points;
    for (vertex_t & point : nextPose) point.point[Y] *= 1.5f;
    key_frame_t frames[2] = {};
    frames[0].cVert = points.data();
    frames[1].cVert = nextPose.data();
    model.nbFrames = 2;
    model.keyFrames = frames;
    assert(generateAutomaticLod(&model, 75));
    printf("Animated torus: %u -> %u faces, %u -> %u points\n", mesh.nbPolygon, automaticLodFaceCount(), mesh.nbPoint, automaticLodPointCount());
    assert(automaticLodFaceCount() < faces.size());
    unsigned int animatedCount = automaticLodFaceCount();
    assert(generateAutomaticLod(&model, 99));
    printf("Animated 99%% target: %u faces, %u points\n", automaticLodFaceCount(), automaticLodPointCount());
    assert(automaticLodFaceCount() <= animatedCount);
    assert(automaticLodLevelCount(&model) == 2);
    assert(selectAutomaticLod(&model, 1));
    assert(automaticLodFaceCount() == animatedCount);
    assert(selectAutomaticLod(&model, 2));
    assert(!selectAutomaticLod(&model, 3));
    assert(automaticLodPointCount() < mesh.nbPoint);
    lod = automaticLodFaces(&model, 0, &count);
    for (unsigned int face = 0; face < count; ++face)
        for (unsigned int corner = 0; corner < 3; ++corner)
        {
            assert(lod[face].vertIdx[corner] < mesh.nbPoint);
            for (unsigned int other = corner + 1; other < 3; ++other)
                assert(lod[face].vertIdx[corner] != lod[face].vertIdx[other]);
        }
    assert(std::memcmp(points.data(), originalPoints.data(), points.size() * sizeof(vertex_t)) == 0);
    assert(std::memcmp(faces.data(), originalFaces.data(), faces.size() * sizeof(polygon_t)) == 0);
    std::vector<vertex_t> combinedPoints(3), combinedNext(3);
    const uint32_t revision = model.lodData->revision;
    assert(generateAutomaticLod(&model, 90, 2));
    assert(automaticLodLevelCount(&model) == 2);
    assert(model.lodData->levels[0].reduction == 75 && model.lodData->levels[1].reduction == 90);
    assert(model.lodData->revision == revision + 1);
    assert(!generateAutomaticLod(&model, 75, 2));
    assert(!generateAutomaticLod(&model, 0, 2));
    assert(!generateAutomaticLod(&model, 80, 3));
    assert(model.lodData->revision == revision + 1);
    assert(!deleteAutomaticLod(&model, 0));
    assert(!deleteAutomaticLod(&model, 3));
    assert(deleteAutomaticLod(&model, 1));
    assert(automaticLodLevelCount(&model) == 1 && model.lodData->levels[0].reduction == 90);
    assert(selectAutomaticLod(&model, 1));
    assert(deleteAutomaticLod(&model, 1));
    assert(automaticLodLevelCount(&model) == 0 && !showAutomaticLod);
    assert(model.lodData->revision == revision + 3);
    assert(std::memcmp(points.data(), originalPoints.data(), points.size() * sizeof(vertex_t)) == 0);
    assert(std::memcmp(faces.data(), originalFaces.data(), faces.size() * sizeof(polygon_t)) == 0);
    combinedPoints.insert(combinedPoints.end(), points.begin(), points.end());
    combinedNext.insert(combinedNext.end(), nextPose.begin(), nextPose.end());
    model_t meshes[2] = {};
    meshes[0].nbPoint = 3;
    meshes[0].pntbl = combinedPoints.data();
    meshes[1] = mesh;
    key_frame_t combinedFrames[2] = {};
    combinedFrames[0].cVert = combinedPoints.data();
    combinedFrames[1].cVert = combinedNext.data();
    animated_model_t multiModel = model;
    multiModel.lodData = NULL;
    multiModel.nbModels = 2;
    multiModel.model = meshes;
    multiModel.keyFrames = combinedFrames;
    assert(generateAutomaticLod(&multiModel, 75));
    assert(automaticLodFaceCount() == animatedCount);
    assert(automaticLodFaces(&multiModel, 0, &count) == NULL && count == 0);
    assert(automaticLodFaces(&multiModel, 1, &count) != NULL && count == animatedCount);
    animated_model_t other = {};
    assert(automaticLodFaces(&other, 0, &count) == NULL && count == 0);
    showAutomaticLod = true;
    resetAutomaticLod();
    assert(!showAutomaticLod && automaticLodFaceCount() == 0);
    polygon_t triangle = {};
    triangle.vertIdx[0] = 0;
    triangle.vertIdx[1] = 1;
    triangle.vertIdx[2] = triangle.vertIdx[3] = 2;
    model_t triangleMesh = mesh;
    triangleMesh.nbPolygon = 1;
    triangleMesh.pltbl = &triangle;
    animated_model_t triangleModel = model;
    triangleModel.lodData = NULL;
    triangleModel.model = &triangleMesh;
    assert(generateAutomaticLod(&triangleModel, 95));
    assert(automaticLodFaceCount() == 1);
    lod = automaticLodFaces(&triangleModel, 0, &count);
    assert(std::memcmp(lod->vertIdx, triangle.vertIdx, sizeof(triangle.vertIdx)) == 0);
    faces[0].vertIdx[0] = mesh.nbPoint;
    assert(!generateAutomaticLod(&model, 75));
    assert(automaticLodFaceCount() == 0);
    assert(!generateAutomaticLod(NULL, 75));
    delete model.lodData;
    delete multiModel.lodData;
    delete triangleModel.lodData;
    puts("LOD tests passed");
}