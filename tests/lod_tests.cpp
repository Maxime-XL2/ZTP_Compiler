#include "../SRC/COMMON.H"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

uint32_t useUV = 0;
void createTexture(polygon_t *, animated_model_t *, const texture_t &) {}
bool setFbxBonePreviewPointRemap(animated_model_t *, const std::vector<unsigned int> &) { return true; }

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
            for (unsigned int corner = 0; corner < 4; ++corner)
            {
                const float sourcePoint = static_cast<float>(faces[index].vertIdx[corner]);
                faces[index].vertUV[corner].U = sourcePoint;
                faces[index].vertUV[corner].V = sourcePoint + 0.25f;
            }
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
    animated_model_t tinyReductionModel = model;
    tinyReductionModel.lodData = NULL;
    assert(generateAutomaticLod(&tinyReductionModel, 1));
    assert(automaticLodFaceCount() > 0 && automaticLodFaceCount() <= faces.size());
    delete tinyReductionModel.lodData;
    resetAutomaticLod();
    assert(generateAutomaticLod(&model, 0));
    assert(automaticLodFaceCount() == faces.size());
    assert(generateAutomaticLod(&model, 75));
    unsigned int count = 0;
    const polygon_t * lod = automaticLodFaces(&model, 0, &count);
    printf("Static torus: %u -> %u faces, %u -> %u points\n", mesh.nbPolygon, count, mesh.nbPoint, automaticLodPointCount());
    assert(lod && count > 0 && count < faces.size());
    unsigned int generatedQuadCount = 0;
    for (unsigned int face = 0; face < count; ++face)
    {
        assert(lod[face].color.r == 128 && lod[face].color.g == 0 && lod[face].color.b == 128);
        assert(lod[face].color.a == 255);
        if (lod[face].vertIdx[3] != lod[face].vertIdx[2]) ++generatedQuadCount;
        for (unsigned int corner = 0; corner < 4; ++corner)
        {
            const float expectedUv = static_cast<float>(lod[face].vertIdx[corner]);
            assert(lod[face].vertUV[corner].U == expectedUv);
            assert(lod[face].vertUV[corner].V == expectedUv + 0.25f);
        }
    }
    assert(generatedQuadCount > 0);
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
    lod = automaticLodFaces(&model, 0, &count);
    bool animatedHasQuad = false;
    for (unsigned int face = 0; face < count; ++face)
        if (lod[face].vertIdx[2] != lod[face].vertIdx[3]) animatedHasQuad = true;
    assert(animatedHasQuad);
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
    triangle.vertIdx[0] = triangle.vertIdx[1] = 2;
    triangle.vertIdx[2] = 1;
    triangle.vertIdx[3] = 0;
    triangle.vertUV[0] = triangle.vertUV[1] = {2.0f, 0.25f};
    triangle.vertUV[2] = {1.0f, 0.5f};
    triangle.vertUV[3] = {0.0f, 0.75f};
    model_t triangleMesh = mesh;
    triangleMesh.nbPolygon = 1;
    triangleMesh.pltbl = &triangle;
    animated_model_t triangleModel = model;
    triangleModel.lodData = NULL;
    triangleModel.model = &triangleMesh;
    assert(generateAutomaticLod(&triangleModel, 0));
    assert(automaticLodFaceCount() == 1);
    lod = automaticLodFaces(&triangleModel, 0, &count);
    assert(lod->vertIdx[0] == 2 && lod->vertIdx[1] == 1 && lod->vertIdx[2] == 0 && lod->vertIdx[3] == 0);
    assert(lod->vertUV[0].U == 2.0f && lod->vertUV[0].V == 0.25f);
    assert(lod->vertUV[1].U == 1.0f && lod->vertUV[1].V == 0.5f);
    assert(lod->vertUV[2].U == 0.0f && lod->vertUV[2].V == 0.75f);
    assert(lod->vertUV[3].U == lod->vertUV[2].U && lod->vertUV[3].V == lod->vertUV[2].V);
    vertex_t hexagonPoints[5] = {};
    hexagonPoints[0].point[X] = -1.0f; hexagonPoints[0].point[Y] = -1.0f;
    hexagonPoints[1].point[X] = 1.0f; hexagonPoints[1].point[Y] = -1.0f;
    hexagonPoints[2].point[X] = 1.0f; hexagonPoints[2].point[Y] = 1.0f;
    hexagonPoints[3].point[X] = -1.0f; hexagonPoints[3].point[Y] = 1.0f;
    hexagonPoints[4].point[X] = 0.2f; hexagonPoints[4].point[Y] = 0.1f;
    const unsigned int triangleIndices[4][3] = {{4, 0, 1}, {4, 1, 2}, {4, 2, 3}, {4, 3, 0}};
    polygon_t * hexagonFaces = new polygon_t[4]();
    for (unsigned int face = 0; face < 4; ++face)
    {
        for (unsigned int corner = 0; corner < 3; ++corner)
        {
            hexagonFaces[face].vertIdx[corner] = triangleIndices[face][corner];
            hexagonFaces[face].vertUV[corner].U = static_cast<float>(triangleIndices[face][corner]);
        }
        hexagonFaces[face].vertIdx[3] = hexagonFaces[face].vertIdx[2];
        hexagonFaces[face].vertUV[3] = hexagonFaces[face].vertUV[2];
    }
    const std::vector<vertex_t> hexagonOriginalPoints(hexagonPoints, hexagonPoints + 5);
    model_t hexagonMesh = {};
    hexagonMesh.nbPoint = 5;
    hexagonMesh.nbPolygon = 4;
    hexagonMesh.pntbl = hexagonPoints;
    hexagonMesh.pltbl = hexagonFaces;
    key_frame_t hexagonFrame = {};
    hexagonFrame.cVert = hexagonPoints;
    model_lod_data_t * hexagonLodData = new model_lod_data_t;
    hexagonLodData->sglFrameNormals.resize(1);
    hexagonLodData->sglFrameNormals[0].assign(4, 0);
    animated_model_t hexagonModel = {};
    hexagonModel.nbModels = 1;
    hexagonModel.model = &hexagonMesh;
    hexagonModel.nbFrames = 1;
    hexagonModel.keyFrames = &hexagonFrame;
    hexagonModel.lodData = hexagonLodData;
    const uint32_t previousUseUV = useUV;
    useUV = 1;
    assert(mergeBaseTrianglesIntoQuads(&hexagonModel));
    useUV = previousUseUV;
    assert(hexagonMesh.nbPolygon < 4);
    assert(hexagonLodData->sglFrameNormals.empty() && hexagonLodData->revision == 1);
    unsigned int quadCount = 0;
    for (unsigned int face = 0; face < hexagonMesh.nbPolygon; ++face)
        if (hexagonFaces[face].vertIdx[2] != hexagonFaces[face].vertIdx[3]) ++quadCount;
    assert(quadCount > 0);
    assert(std::memcmp(hexagonPoints, hexagonOriginalPoints.data(), sizeof(hexagonPoints)) == 0);
    assert(!mergeBaseTrianglesIntoQuads(&hexagonModel));
    delete [] hexagonMesh.pltbl;
    delete hexagonLodData;
    vertex_t seamPoints[4] = {};
    seamPoints[1].point[X] = 1.0f;
    seamPoints[2].point[X] = 1.0f; seamPoints[2].point[Y] = 1.0f;
    seamPoints[3].point[Y] = 1.0f;
    polygon_t * alternateFaces = new polygon_t[2]();
    const unsigned int alternateIndices[2][4] = {{2, 2, 1, 0}, {3, 3, 2, 0}};
    for (unsigned int face = 0; face < 2; ++face)
        for (unsigned int corner = 0; corner < 4; ++corner)
        {
            alternateFaces[face].vertIdx[corner] = alternateIndices[face][corner];
            alternateFaces[face].vertUV[corner].U = static_cast<float>(alternateIndices[face][corner]);
        }
    model_t alternateMesh = {};
    alternateMesh.nbPoint = 4;
    alternateMesh.nbPolygon = 2;
    alternateMesh.pntbl = seamPoints;
    alternateMesh.pltbl = alternateFaces;
    animated_model_t alternateModel = {};
    alternateModel.nbModels = 1;
    alternateModel.model = &alternateMesh;
    useUV = 1;
    assert(mergeBaseTrianglesIntoQuads(&alternateModel));
    assert(alternateMesh.nbPolygon == 1);
    assert(alternateFaces[0].vertIdx[2] != alternateFaces[0].vertIdx[3]);
    delete [] alternateMesh.pltbl;
    const auto checkBentPair = [&](float height, bool shouldMerge)
    {
        vertex_t bentPoints[4];
        std::copy(seamPoints, seamPoints + 4, bentPoints);
        bentPoints[3].point[Z] = height;
        polygon_t * bentFaces = new polygon_t[2]();
        for (unsigned int face = 0; face < 2; ++face)
            for (unsigned int corner = 0; corner < 4; ++corner)
            {
                bentFaces[face].vertIdx[corner] = alternateIndices[face][corner];
                bentFaces[face].vertUV[corner].U = static_cast<float>(alternateIndices[face][corner]);
            }
        model_t bentMesh = {};
        bentMesh.nbPoint = 4;
        bentMesh.nbPolygon = 2;
        bentMesh.pntbl = bentPoints;
        bentMesh.pltbl = bentFaces;
        animated_model_t bentModel = {};
        bentModel.nbModels = 1;
        bentModel.model = &bentMesh;
        assert(mergeBaseTrianglesIntoQuads(&bentModel) == shouldMerge);
        assert(bentMesh.nbPolygon == (shouldMerge ? 1u : 2u));
        delete [] bentMesh.pltbl;
    };
    checkBentPair(0.5f, true);
    checkBentPair(2.0f, false);
    useUV = previousUseUV;
    polygon_t * seamFaces = new polygon_t[2]();
    const unsigned int seamIndices[2][3] = {{0, 1, 2}, {0, 2, 3}};
    for (unsigned int face = 0; face < 2; ++face)
    {
        for (unsigned int corner = 0; corner < 3; ++corner)
        {
            seamFaces[face].vertIdx[corner] = seamIndices[face][corner];
            seamFaces[face].vertUV[corner].U = static_cast<float>(seamIndices[face][corner]);
        }
        seamFaces[face].vertIdx[3] = seamFaces[face].vertIdx[2];
        seamFaces[face].vertUV[3] = seamFaces[face].vertUV[2];
    }
    seamFaces[1].vertUV[0].U += 1.0f;
    model_t seamMesh = {};
    seamMesh.nbPoint = 4;
    seamMesh.nbPolygon = 2;
    seamMesh.pntbl = seamPoints;
    seamMesh.pltbl = seamFaces;
    animated_model_t seamModel = {};
    seamModel.nbModels = 1;
    seamModel.model = &seamMesh;
    useUV = 1;
    assert(!mergeBaseTrianglesIntoQuads(&seamModel));
    useUV = previousUseUV;
    assert(seamMesh.nbPolygon == 2);
    delete [] seamMesh.pltbl;
    faces[0].vertIdx[0] = mesh.nbPoint;
    assert(!generateAutomaticLod(&model, 75));
    assert(automaticLodFaceCount() == 0);
    assert(!generateAutomaticLod(NULL, 75));
    delete model.lodData;
    delete multiModel.lodData;
    delete triangleModel.lodData;
    puts("LOD tests passed");
}