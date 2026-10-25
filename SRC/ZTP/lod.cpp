#include "../COMMON.H"
#include "../../third_party/meshoptimizer/meshoptimizer.h"
#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>

void createTexture(polygon_t * polygon, animated_model_t * model, const texture_t & sourceTexture);

bool showAutomaticLod = false;

namespace
{
const animated_model_t * lodSource = NULL;
std::vector<std::vector<polygon_t> > lodMeshes;
unsigned int lodPointCount = 0;

struct BaseMeshSnapshot
{
    model_t mesh;
    std::vector<vertex_t> points;
    std::vector<polygon_t> faces;
};

struct AutomaticBaseSnapshot
{
    const animated_model_t * owner = NULL;
    std::vector<BaseMeshSnapshot> meshes;
    std::vector<std::vector<vertex_t> > frames;
    bool hadLodData = false;
    std::vector<lod_level_t> lodLevels;
    std::vector<std::vector<uint8_t> > sglFrameNormals;
    uint32_t lodRevision = 0;
    unsigned int textureCount = 0;
};

AutomaticBaseSnapshot originalBaseSnapshot;

struct CachedSourceTexture
{
    texture_t texture;
    std::vector<pixel_t> pixels;
};

struct CachedSourceFace
{
    int texture = -1;
    unsigned int vertIdx[4];
    uv_t uv[4];
};

struct LodSourceTextureCache
{
    const animated_model_t * owner = NULL;
    std::vector<CachedSourceTexture> textures;
    std::vector<std::vector<CachedSourceFace> > faces;
};

std::vector<LodSourceTextureCache> lodSourceTextureCaches;

LodSourceTextureCache * findSourceTextureCache(const animated_model_t * model)
{
    for (LodSourceTextureCache & cache : lodSourceTextureCaches)
        if (cache.owner == model) return &cache;
    return NULL;
}

int sourceMaterialIndex(const LodSourceTextureCache * cache, unsigned int mesh, unsigned int face,
                        int fallback)
{
    if (!cache || mesh >= cache->faces.size() || face >= cache->faces[mesh].size()) return fallback;
    return cache->faces[mesh][face].texture;
}

bool bakeCachedSourceTexture(animated_model_t * model, polygon_t * face)
{
    LodSourceTextureCache * cache = findSourceTextureCache(model);
    if (!cache || !useUV) return true;
    if (!face || !model->texture || face->texture < 0 ||
        face->texture >= static_cast<int>(cache->textures.size()) || model->nbTextures >= 2048) return false;
    CachedSourceTexture & source = cache->textures[face->texture];
    if (source.pixels.empty()) return false;
    texture_t texture = source.texture;
    texture.pixel = source.pixels.data();
    createTexture(face, model, texture);
    return face->texture >= 0 && face->texture < static_cast<int>(model->nbTextures);
}

void clearBaseSnapshot()
{
    originalBaseSnapshot = AutomaticBaseSnapshot();
}

bool captureBaseSnapshot(animated_model_t * model)
{
    if (!model || !model->model || (model->nbFrames && !model->keyFrames)) return false;
    AutomaticBaseSnapshot snapshot;
    snapshot.owner = model;
    snapshot.meshes.resize(model->nbModels);
    size_t totalPoints = 0;
    for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
    {
        const model_t & mesh = model->model[meshIndex];
        if ((mesh.nbPoint && !mesh.pntbl) || (mesh.nbPolygon && !mesh.pltbl)) return false;
        BaseMeshSnapshot & saved = snapshot.meshes[meshIndex];
        saved.mesh = mesh;
        if (mesh.nbPoint) saved.points.assign(mesh.pntbl, mesh.pntbl + mesh.nbPoint);
        if (mesh.nbPolygon) saved.faces.assign(mesh.pltbl, mesh.pltbl + mesh.nbPolygon);
        totalPoints += mesh.nbPoint;
    }
    snapshot.frames.resize(model->nbFrames);
    for (unsigned int frame = 0; frame < model->nbFrames; ++frame)
    {
        if (totalPoints && !model->keyFrames[frame].cVert) return false;
        if (totalPoints)
            snapshot.frames[frame].assign(model->keyFrames[frame].cVert,
                                          model->keyFrames[frame].cVert + totalPoints);
    }
    if (model->lodData)
    {
        snapshot.hadLodData = true;
        snapshot.lodLevels = model->lodData->levels;
        snapshot.sglFrameNormals = model->lodData->sglFrameNormals;
        snapshot.lodRevision = model->lodData->revision;
    }
    snapshot.textureCount = model->nbTextures;
    originalBaseSnapshot = std::move(snapshot);
    return true;
}

bool restoreBaseSnapshotData(animated_model_t * model)
{
    if (!model || originalBaseSnapshot.owner != model || !model->model ||
        model->nbModels != originalBaseSnapshot.meshes.size() ||
        model->nbFrames != originalBaseSnapshot.frames.size() ||
        (model->nbFrames && !model->keyFrames) ||
        (originalBaseSnapshot.textureCount && !model->texture)) return false;

    std::vector<vertex_t *> points(model->nbModels, NULL);
    std::vector<polygon_t *> faces(model->nbModels, NULL);
    std::vector<vertex_t *> frames(model->nbFrames, NULL);
    const auto discardArrays = [&]()
    {
        for (vertex_t * array : points) delete [] array;
        for (polygon_t * array : faces) delete [] array;
        for (vertex_t * array : frames) delete [] array;
    };
    size_t totalPoints = 0;
    for (const BaseMeshSnapshot & mesh : originalBaseSnapshot.meshes) totalPoints += mesh.points.size();
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
    {
        const BaseMeshSnapshot & saved = originalBaseSnapshot.meshes[mesh];
        if (!saved.points.empty())
        {
            points[mesh] = new (std::nothrow) vertex_t[saved.points.size()];
            if (!points[mesh]) { discardArrays(); return false; }
            std::copy(saved.points.begin(), saved.points.end(), points[mesh]);
        }
        if (!saved.faces.empty())
        {
            faces[mesh] = new (std::nothrow) polygon_t[saved.faces.size()];
            if (!faces[mesh]) { discardArrays(); return false; }
            std::copy(saved.faces.begin(), saved.faces.end(), faces[mesh]);
        }
    }
    for (unsigned int frame = 0; frame < model->nbFrames; ++frame)
    {
        const std::vector<vertex_t> & saved = originalBaseSnapshot.frames[frame];
        if (!saved.empty())
        {
            frames[frame] = new (std::nothrow) vertex_t[saved.size()];
            if (!frames[frame]) { discardArrays(); return false; }
            std::copy(saved.begin(), saved.end(), frames[frame]);
        }
    }
    model_lod_data_t * restoredLodData = NULL;
    if (originalBaseSnapshot.hadLodData && !model->lodData)
    {
        restoredLodData = new (std::nothrow) model_lod_data_t;
        if (!restoredLodData) { discardArrays(); return false; }
    }

    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
    {
        delete [] model->model[mesh].pntbl;
        delete [] model->model[mesh].pltbl;
        model->model[mesh] = originalBaseSnapshot.meshes[mesh].mesh;
        model->model[mesh].pntbl = points[mesh];
        model->model[mesh].pltbl = faces[mesh];
        points[mesh] = NULL;
        faces[mesh] = NULL;
    }
    for (unsigned int frame = 0; frame < model->nbFrames; ++frame)
    {
        delete [] model->keyFrames[frame].cVert;
        model->keyFrames[frame].cVert = frames[frame];
        frames[frame] = NULL;
    }
    if (originalBaseSnapshot.hadLodData)
    {
        if (restoredLodData) model->lodData = restoredLodData;
        model->lodData->levels = originalBaseSnapshot.lodLevels;
        model->lodData->sglFrameNormals = originalBaseSnapshot.sglFrameNormals;
        model->lodData->revision = originalBaseSnapshot.lodRevision;
    }
    else
    {
        delete model->lodData;
        model->lodData = NULL;
    }
    for (unsigned int texture = originalBaseSnapshot.textureCount; texture < model->nbTextures; ++texture)
    {
        delete [] model->texture[texture].pixel;
        model->texture[texture].pixel = NULL;
    }
    model->nbTextures = originalBaseSnapshot.textureCount;
    setFbxBonePreviewPointRemap(model, std::vector<unsigned int>());
    discardArrays();
    return true;
}

struct ReducedMeshData
{
    std::vector<unsigned int> sourcePoints;
    std::vector<vertex_t> points;
    std::vector<polygon_t> faces;
};

uint64_t undirectedEdgeKey(unsigned int first, unsigned int second)
{
    const unsigned int low = MIN(first, second), high = MAX(first, second);
    return (uint64_t(low) << 32) | high;
}

bool sameUv(const uv_t & first, const uv_t & second)
{
    return std::fabs(first.U - second.U) <= 1e-5f && std::fabs(first.V - second.V) <= 1e-5f;
}

float pointTriangleDistanceSquared(const float * point, const float * first,
                                   const float * second, const float * third)
{
    float firstEdge[3], secondEdge[3], toPoint[3];
    for (unsigned int axis = 0; axis < 3; ++axis)
    {
        firstEdge[axis] = second[axis] - first[axis];
        secondEdge[axis] = third[axis] - first[axis];
        toPoint[axis] = point[axis] - first[axis];
    }
    const auto dot = [](const float * a, const float * b)
    {
        return a[X] * b[X] + a[Y] * b[Y] + a[Z] * b[Z];
    };
    const float d1 = dot(firstEdge, toPoint), d2 = dot(secondEdge, toPoint);
    float closest[3];
    if (d1 <= 0.0f && d2 <= 0.0f) std::copy(first, first + 3, closest);
    else
    {
        float toSecond[3];
        for (unsigned int axis = 0; axis < 3; ++axis) toSecond[axis] = point[axis] - second[axis];
        const float d3 = dot(firstEdge, toSecond), d4 = dot(secondEdge, toSecond);
        if (d3 >= 0.0f && d4 <= d3) std::copy(second, second + 3, closest);
        else
        {
            const float vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
            {
                const float fraction = d1 / (d1 - d3);
                for (unsigned int axis = 0; axis < 3; ++axis) closest[axis] = first[axis] + fraction * firstEdge[axis];
            }
            else
            {
                float toThird[3];
                for (unsigned int axis = 0; axis < 3; ++axis) toThird[axis] = point[axis] - third[axis];
                const float d5 = dot(firstEdge, toThird), d6 = dot(secondEdge, toThird);
                if (d6 >= 0.0f && d5 <= d6) std::copy(third, third + 3, closest);
                else
                {
                    const float vb = d5 * d2 - d1 * d6;
                    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
                    {
                        const float fraction = d2 / (d2 - d6);
                        for (unsigned int axis = 0; axis < 3; ++axis) closest[axis] = first[axis] + fraction * secondEdge[axis];
                    }
                    else
                    {
                        const float va = d3 * d6 - d5 * d4;
                        if (va <= 0.0f && d4 - d3 >= 0.0f && d5 - d6 >= 0.0f)
                        {
                            const float fraction = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                            for (unsigned int axis = 0; axis < 3; ++axis)
                                closest[axis] = second[axis] + fraction * (third[axis] - second[axis]);
                        }
                        else
                        {
                            const float inverse = 1.0f / (va + vb + vc);
                            const float secondWeight = vb * inverse, thirdWeight = vc * inverse;
                            for (unsigned int axis = 0; axis < 3; ++axis)
                                closest[axis] = first[axis] + firstEdge[axis] * secondWeight + secondEdge[axis] * thirdWeight;
                        }
                    }
                }
            }
        }
    }
    float distanceSquared = 0.0f;
    for (unsigned int axis = 0; axis < 3; ++axis)
    {
        const float delta = point[axis] - closest[axis];
        distanceSquared += delta * delta;
    }
    return distanceSquared;
}

float pointFaceDistanceSquared(const float * point, const model_t & mesh, const polygon_t & face)
{
    const float * first = mesh.pntbl[face.vertIdx[0]].point;
    const float * second = mesh.pntbl[face.vertIdx[1]].point;
    const float * third = mesh.pntbl[face.vertIdx[2]].point;
    float distance = pointTriangleDistanceSquared(point, first, second, third);
    if (face.vertIdx[2] != face.vertIdx[3])
        distance = MIN(distance, pointTriangleDistanceSquared(point, first, third,
            mesh.pntbl[face.vertIdx[3]].point));
    return distance;
}

bool closestTriangleBarycentrics(const float * point, const float * first,
                                 const float * second, const float * third, float weights[3])
{
    float firstEdge[3], secondEdge[3], toPoint[3];
    for (unsigned int axis = 0; axis < 3; ++axis)
    {
        firstEdge[axis] = second[axis] - first[axis];
        secondEdge[axis] = third[axis] - first[axis];
        toPoint[axis] = point[axis] - first[axis];
    }
    const auto dot = [](const float * a, const float * b)
    {
        return a[X] * b[X] + a[Y] * b[Y] + a[Z] * b[Z];
    };
    const float d1 = dot(firstEdge, toPoint), d2 = dot(secondEdge, toPoint);
    if (d1 <= 0.0f && d2 <= 0.0f) { weights[0] = 1.0f; weights[1] = weights[2] = 0.0f; return true; }

    float toSecond[3];
    for (unsigned int axis = 0; axis < 3; ++axis) toSecond[axis] = point[axis] - second[axis];
    const float d3 = dot(firstEdge, toSecond), d4 = dot(secondEdge, toSecond);
    if (d3 >= 0.0f && d4 <= d3) { weights[0] = 0.0f; weights[1] = 1.0f; weights[2] = 0.0f; return true; }

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        const float fraction = d1 / (d1 - d3);
        weights[0] = 1.0f - fraction; weights[1] = fraction; weights[2] = 0.0f;
        return true;
    }

    float toThird[3];
    for (unsigned int axis = 0; axis < 3; ++axis) toThird[axis] = point[axis] - third[axis];
    const float d5 = dot(firstEdge, toThird), d6 = dot(secondEdge, toThird);
    if (d6 >= 0.0f && d5 <= d6) { weights[0] = 0.0f; weights[1] = 0.0f; weights[2] = 1.0f; return true; }

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        const float fraction = d2 / (d2 - d6);
        weights[0] = 1.0f - fraction; weights[1] = 0.0f; weights[2] = fraction;
        return true;
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && d4 - d3 >= 0.0f && d5 - d6 >= 0.0f)
    {
        const float fraction = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        weights[0] = 0.0f; weights[1] = 1.0f - fraction; weights[2] = fraction;
        return true;
    }

    const float denominator = va + vb + vc;
    if (std::fabs(denominator) <= 1e-20f) return false;
    weights[1] = vb / denominator;
    weights[2] = vc / denominator;
    weights[0] = 1.0f - weights[1] - weights[2];
    return true;
}

bool interpolateTriangleUv(const float * point, const model_t & mesh, const polygon_t & face,
                           unsigned int firstCorner, unsigned int secondCorner, unsigned int thirdCorner,
                           uv_t & uv)
{
    float weights[3];
    if (!closestTriangleBarycentrics(point,
            mesh.pntbl[face.vertIdx[firstCorner]].point,
            mesh.pntbl[face.vertIdx[secondCorner]].point,
            mesh.pntbl[face.vertIdx[thirdCorner]].point, weights)) return false;
    const uv_t & firstUv = face.vertUV[firstCorner];
    const uv_t & secondUv = face.vertUV[secondCorner];
    const uv_t & thirdUv = face.vertUV[thirdCorner];
    uv.U = weights[0] * firstUv.U + weights[1] * secondUv.U + weights[2] * thirdUv.U;
    uv.V = weights[0] * firstUv.V + weights[1] * secondUv.V + weights[2] * thirdUv.V;
    return true;
}

bool interpolateFaceUv(const float * point, const model_t & mesh, const polygon_t & face, uv_t & uv)
{
    if (face.vertIdx[2] == face.vertIdx[3])
        return interpolateTriangleUv(point, mesh, face, 0, 1, 2, uv);

    const float * first = mesh.pntbl[face.vertIdx[0]].point;
    const float * second = mesh.pntbl[face.vertIdx[1]].point;
    const float * third = mesh.pntbl[face.vertIdx[2]].point;
    const float * fourth = mesh.pntbl[face.vertIdx[3]].point;
    const float firstDistance = pointTriangleDistanceSquared(point, first, second, third);
    const float secondDistance = pointTriangleDistanceSquared(point, first, third, fourth);
    if (firstDistance <= secondDistance)
        return interpolateTriangleUv(point, mesh, face, 0, 1, 2, uv) ||
               interpolateTriangleUv(point, mesh, face, 0, 2, 3, uv);
    return interpolateTriangleUv(point, mesh, face, 0, 2, 3, uv) ||
           interpolateTriangleUv(point, mesh, face, 0, 1, 2, uv);
}

int faceCornerForPoint(const polygon_t & face, unsigned int point)
{
    for (unsigned int corner = 0; corner < 3; ++corner)
        if (face.vertIdx[corner] == point) return static_cast<int>(corner);
    return -1;
}

bool sameMergeAttributes(const polygon_t & first, const polygon_t & second, bool rebakeTexture,
                         bool sameSourceMaterial)
{
    if (rebakeTexture ? !sameSourceMaterial : first.texture != second.texture)
        return false;
    if (useUV)
    {
        for (unsigned int firstCorner = 0; firstCorner < 3; ++firstCorner)
            for (unsigned int secondCorner = 0; secondCorner < 3; ++secondCorner)
                if (first.vertIdx[firstCorner] == second.vertIdx[secondCorner] &&
                    (std::fabs(first.vertUV[firstCorner].U - second.vertUV[secondCorner].U) > 1e-4f ||
                     std::fabs(first.vertUV[firstCorner].V - second.vertUV[secondCorner].V) > 1e-4f))
                    return false;
    }
    if (rebakeTexture)
        return true;
    if (first.SGL_ATTR.flag != second.SGL_ATTR.flag ||
        first.SGL_ATTR.sorting != second.SGL_ATTR.sorting || first.SGL_ATTR.atrb != second.SGL_ATTR.atrb)
        return false;
    return first.SGL_ATTR.texno == second.SGL_ATTR.texno &&
        first.SGL_ATTR.colno == second.SGL_ATTR.colno && first.SGL_ATTR.gstb == second.SGL_ATTR.gstb &&
        first.SGL_ATTR.dir == second.SGL_ATTR.dir && first.color.rgb == second.color.rgb &&
        first.color.r == second.color.r && first.color.g == second.color.g &&
        first.color.b == second.color.b && first.color.a == second.color.a;
}

bool mergeTrianglePair(const model_t & mesh, const polygon_t & first, const polygon_t & second,
                       bool rebakeTexture, bool sameSourceMaterial, polygon_t & quad)
{
    if (first.vertIdx[2] != first.vertIdx[3] || second.vertIdx[2] != second.vertIdx[3] ||
        !sameMergeAttributes(first, second, rebakeTexture, sameSourceMaterial)) return false;
    for (unsigned int edge = 0; edge < 3; ++edge)
    {
        const unsigned int firstStart = first.vertIdx[edge];
        const unsigned int firstEnd = first.vertIdx[(edge + 1) % 3];
        const unsigned int firstThird = first.vertIdx[(edge + 2) % 3];
        for (unsigned int otherEdge = 0; otherEdge < 3; ++otherEdge)
        {
            if (second.vertIdx[otherEdge] != firstEnd ||
                second.vertIdx[(otherEdge + 1) % 3] != firstStart) continue;
            const unsigned int secondThird = second.vertIdx[(otherEdge + 2) % 3];
            if (firstStart == firstEnd || firstThird == secondThird ||
                firstStart == firstThird || firstEnd == firstThird ||
                firstStart == secondThird || firstEnd == secondThird) continue;
            const int firstStartCorner = faceCornerForPoint(first, firstStart);
            const int firstEndCorner = faceCornerForPoint(first, firstEnd);
            const int secondStartCorner = faceCornerForPoint(second, firstStart);
            const int secondEndCorner = faceCornerForPoint(second, firstEnd);
            const int secondThirdCorner = faceCornerForPoint(second, secondThird);
            const int firstThirdCorner = faceCornerForPoint(first, firstThird);
            if (firstStartCorner < 0 || firstEndCorner < 0 || secondStartCorner < 0 ||
                secondEndCorner < 0 || secondThirdCorner < 0 || firstThirdCorner < 0 ||
                !sameUv(first.vertUV[firstStartCorner], second.vertUV[secondStartCorner]) ||
                !sameUv(first.vertUV[firstEndCorner], second.vertUV[secondEndCorner])) continue;

            const float * a = mesh.pntbl[firstStart].point;
            const float * b = mesh.pntbl[firstEnd].point;
            const float * c = mesh.pntbl[firstThird].point;
            const float * d = mesh.pntbl[secondThird].point;
            float normalA[3], normalB[3];
            float edgeAB[3], edgeAC[3], edgeBA[3], edgeBD[3];
            for (unsigned int axis = 0; axis < 3; ++axis)
            {
                edgeAB[axis] = b[axis] - a[axis];
                edgeAC[axis] = c[axis] - a[axis];
                edgeBA[axis] = a[axis] - b[axis];
                edgeBD[axis] = d[axis] - b[axis];
            }
            normalA[X] = edgeAB[Y] * edgeAC[Z] - edgeAB[Z] * edgeAC[Y];
            normalA[Y] = edgeAB[Z] * edgeAC[X] - edgeAB[X] * edgeAC[Z];
            normalA[Z] = edgeAB[X] * edgeAC[Y] - edgeAB[Y] * edgeAC[X];
            normalB[X] = edgeBA[Y] * edgeBD[Z] - edgeBA[Z] * edgeBD[Y];
            normalB[Y] = edgeBA[Z] * edgeBD[X] - edgeBA[X] * edgeBD[Z];
            normalB[Z] = edgeBA[X] * edgeBD[Y] - edgeBA[Y] * edgeBD[X];
            float lengthA = std::sqrt(normalA[X] * normalA[X] + normalA[Y] * normalA[Y] + normalA[Z] * normalA[Z]);
            float lengthB = std::sqrt(normalB[X] * normalB[X] + normalB[Y] * normalB[Y] + normalB[Z] * normalB[Z]);
            if (lengthA <= 1e-10f || lengthB <= 1e-10f) continue;
            float normalDot = (normalA[X] * normalB[X] + normalA[Y] * normalB[Y] + normalA[Z] * normalB[Z]) /
                (lengthA * lengthB);
            if (normalDot < 0.70710678f) continue;

            const unsigned int perimeter[] = {firstStart, secondThird, firstEnd, firstThird};
            float cornerCross[2][3] = {};
            const unsigned int cornerIndices[2] = {0, 2};
            for (unsigned int corner = 0; corner < 2; ++corner)
            {
                const unsigned int currentIndex = cornerIndices[corner];
                const float * current = mesh.pntbl[perimeter[currentIndex]].point;
                const float * next = mesh.pntbl[perimeter[(currentIndex + 1) & 3]].point;
                const float * previous = mesh.pntbl[perimeter[(currentIndex + 3) & 3]].point;
                float firstEdge[3], secondEdge[3];
                for (unsigned int axis = 0; axis < 3; ++axis)
                {
                    firstEdge[axis] = next[axis] - current[axis];
                    secondEdge[axis] = previous[axis] - current[axis];
                }
                cornerCross[corner][X] = firstEdge[Y] * secondEdge[Z] - firstEdge[Z] * secondEdge[Y];
                cornerCross[corner][Y] = firstEdge[Z] * secondEdge[X] - firstEdge[X] * secondEdge[Z];
                cornerCross[corner][Z] = firstEdge[X] * secondEdge[Y] - firstEdge[Y] * secondEdge[X];
            }
            const float crossDot = cornerCross[0][X] * cornerCross[1][X] +
                cornerCross[0][Y] * cornerCross[1][Y] + cornerCross[0][Z] * cornerCross[1][Z];
            if (crossDot <= 0.0f) continue;

            quad = first;
            for (unsigned int corner = 0; corner < 4; ++corner) quad.vertIdx[corner] = perimeter[corner];
            quad.vertUV[0] = first.vertUV[firstStartCorner];
            quad.vertUV[1] = second.vertUV[secondThirdCorner];
            quad.vertUV[2] = first.vertUV[firstEndCorner];
            quad.vertUV[3] = first.vertUV[firstThirdCorner];
            return true;
        }
    }
    return false;
}

float sharedEdgeLength(const model_t & mesh, const polygon_t & first, const polygon_t & second)
{
    for (unsigned int edge = 0; edge < 3; ++edge)
    {
        const unsigned int start = first.vertIdx[edge];
        const unsigned int end = first.vertIdx[(edge + 1) % 3];
        for (unsigned int other = 0; other < 3; ++other)
            if (second.vertIdx[other] == end && second.vertIdx[(other + 1) % 3] == start)
            {
                const float * a = mesh.pntbl[start].point;
                const float * b = mesh.pntbl[end].point;
                float lengthSquared = 0.0f;
                for (unsigned int axis = 0; axis < 3; ++axis)
                {
                    const float delta = b[axis] - a[axis];
                    lengthSquared += delta * delta;
                }
                return std::sqrt(lengthSquared);
            }
    }
    return 0.0f;
}

class MaximumCardinalityMatching
{
public:
    explicit MaximumCardinalityMatching(const std::vector<std::vector<int> > & graph)
                : graph(graph), match(graph.size(), -1), parent(graph.size()), base(graph.size()), queue(graph.size()),
                    used(graph.size()), blossom(graph.size())
    {
    }

    std::vector<int> solve()
    {
        for (int vertex = 0; vertex < static_cast<int>(graph.size()); ++vertex)
            if (match[vertex] == -1) findAugmentingPath(vertex);
        return match;
    }

private:
    const std::vector<std::vector<int> > & graph;
    std::vector<int> match, parent, base, queue;
    std::vector<bool> used, blossom;

    int leastCommonAncestor(int first, int second)
    {
        std::vector<bool> path(graph.size(), false);
        while (true)
        {
            first = base[first];
            path[first] = true;
            if (match[first] == -1) break;
            first = parent[match[first]];
        }
        while (true)
        {
            second = base[second];
            if (path[second]) return second;
            second = parent[match[second]];
        }
    }

    void markBlossomPath(int vertex, int root, int child)
    {
        while (base[vertex] != root)
        {
            blossom[base[vertex]] = true;
            blossom[base[match[vertex]]] = true;
            parent[vertex] = child;
            child = match[vertex];
            vertex = parent[match[vertex]];
        }
    }

    bool findAugmentingPath(int root)
    {
        std::fill(used.begin(), used.end(), false);
        std::fill(parent.begin(), parent.end(), -1);
        for (int vertex = 0; vertex < static_cast<int>(graph.size()); ++vertex) base[vertex] = vertex;
        int head = 0, tail = 0;
        queue[tail++] = root;
        used[root] = true;
        while (head < tail)
        {
            const int vertex = queue[head++];
            for (int next : graph[vertex])
            {
                if (base[vertex] == base[next] || match[vertex] == next) continue;
                if (next == root || (match[next] != -1 && parent[match[next]] != -1))
                {
                    const int rootBase = leastCommonAncestor(vertex, next);
                    std::fill(blossom.begin(), blossom.end(), false);
                    markBlossomPath(vertex, rootBase, next);
                    markBlossomPath(next, rootBase, vertex);
                    for (int item = 0; item < static_cast<int>(graph.size()); ++item)
                        if (blossom[base[item]])
                        {
                            base[item] = rootBase;
                            if (!used[item])
                            {
                                used[item] = true;
                                queue[tail++] = item;
                            }
                        }
                }
                else if (parent[next] == -1)
                {
                    parent[next] = vertex;
                    if (match[next] == -1)
                    {
                        int current = next;
                        while (current != -1)
                        {
                            const int previous = parent[current];
                            const int following = previous == -1 ? -1 : match[previous];
                            match[current] = previous;
                            if (previous != -1) match[previous] = current;
                            current = following;
                        }
                        return true;
                    }
                    const int partner = match[next];
                    used[partner] = true;
                    queue[tail++] = partner;
                }
            }
        }
        return false;
    }
};

void mergeAdjacentTriangles(const model_t & mesh, std::vector<polygon_t> & faces, bool rebakeTexture,
                            const std::vector<int> * sourceMaterials = NULL,
                            std::vector<bool> * mergedFlags = NULL,
                            std::vector<int> * resultMaterials = NULL)
{
    struct Candidate
    {
        unsigned int first;
        unsigned int second;
        float weight;
        polygon_t quad;
    };
    for (polygon_t & face : faces)
    {
        if (face.vertIdx[2] == face.vertIdx[3]) continue;
        for (unsigned int edge = 0; edge < 4; ++edge)
            if (face.vertIdx[edge] == face.vertIdx[(edge + 1) & 3])
            {
                const polygon_t source = face;
                for (unsigned int corner = 0; corner < 3; ++corner)
                {
                    const unsigned int sourceCorner = (edge + 1 + corner) & 3;
                    face.vertIdx[corner] = source.vertIdx[sourceCorner];
                    face.vertUV[corner] = source.vertUV[sourceCorner];
                }
                face.vertIdx[3] = face.vertIdx[2];
                face.vertUV[3] = face.vertUV[2];
                break;
            }
    }
    std::unordered_map<uint64_t, std::vector<unsigned int> > edgeFaces;
    for (unsigned int face = 0; face < faces.size(); ++face)
        if (faces[face].vertIdx[2] == faces[face].vertIdx[3])
            for (unsigned int edge = 0; edge < 3; ++edge)
                edgeFaces[undirectedEdgeKey(faces[face].vertIdx[edge], faces[face].vertIdx[(edge + 1) % 3])].push_back(face);

    std::vector<Candidate> candidates;
    std::unordered_map<uint64_t, size_t> candidateByFaces;
    for (unsigned int face = 0; face < faces.size(); ++face)
    {
        if (faces[face].vertIdx[2] != faces[face].vertIdx[3]) continue;
        for (unsigned int edge = 0; edge < 3; ++edge)
        {
            const uint64_t key = undirectedEdgeKey(faces[face].vertIdx[edge], faces[face].vertIdx[(edge + 1) % 3]);
            const std::vector<unsigned int> & linkedFaces = edgeFaces[key];
            if (linkedFaces.size() != 2) continue;
            for (unsigned int other : linkedFaces)
            {
                if (other <= face) continue;
                const bool sameMaterial = sourceMaterials ?
                    (*sourceMaterials)[face] == (*sourceMaterials)[other] :
                    faces[face].texture == faces[other].texture;
                polygon_t quad;
                if (!mergeTrianglePair(mesh, faces[face], faces[other], rebakeTexture, sameMaterial, quad)) continue;
                if (sourceMaterials) quad.texture = (*sourceMaterials)[face];
                const uint64_t pair = (uint64_t(face) << 32) | other;
                const float weight = sharedEdgeLength(mesh, faces[face], faces[other]);
                const auto existing = candidateByFaces.find(pair);
                if (existing == candidateByFaces.end())
                {
                    candidateByFaces[pair] = candidates.size();
                    candidates.push_back({face, other, weight, quad});
                }
                else if (weight > candidates[existing->second].weight)
                {
                    candidates[existing->second].weight = weight;
                    candidates[existing->second].quad = quad;
                }
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate & first, const Candidate & second) {
        if (first.weight != second.weight) return first.weight > second.weight;
        if (first.first != second.first) return first.first < second.first;
        return first.second < second.second;
    });
    std::vector<std::vector<int> > graph(faces.size());
    candidateByFaces.clear();
    for (size_t candidate = 0; candidate < candidates.size(); ++candidate)
    {
        const Candidate & pair = candidates[candidate];
        graph[pair.first].push_back(static_cast<int>(pair.second));
        graph[pair.second].push_back(static_cast<int>(pair.first));
        candidateByFaces[(uint64_t(pair.first) << 32) | pair.second] = candidate;
    }
    const std::vector<int> matching = MaximumCardinalityMatching(graph).solve();
    std::vector<polygon_t> merged;
    merged.reserve(faces.size());
    for (unsigned int face = 0; face < faces.size(); ++face)
    {
        const int other = matching[face];
        if (other > static_cast<int>(face))
        {
            merged.push_back(candidates[candidateByFaces[(uint64_t(face) << 32) | static_cast<unsigned int>(other)]].quad);
            if (mergedFlags) mergedFlags->push_back(true);
            if (resultMaterials) resultMaterials->push_back(sourceMaterials ? (*sourceMaterials)[face] : faces[face].texture);
        }
        else if (other == -1)
        {
            merged.push_back(faces[face]);
            if (resultMaterials) resultMaterials->push_back(sourceMaterials ? (*sourceMaterials)[face] : faces[face].texture);
        }
        if (other == -1 && mergedFlags) mergedFlags->push_back(false);
    }
    faces.swap(merged);
}

bool buildReducedBase(animated_model_t * model)
{
    if (!model || lodMeshes.size() != model->nbModels) return false;
    std::vector<ReducedMeshData> reduced(model->nbModels);
    size_t totalPoints = 0;
    for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
    {
        const model_t & mesh = model->model[meshIndex];
        ReducedMeshData & output = reduced[meshIndex];
        output.faces = lodMeshes[meshIndex];
        const bool rebakeTexture = useUV && findSourceTextureCache(model);
        mergeAdjacentTriangles(mesh, output.faces, rebakeTexture);
        std::vector<bool> used(mesh.nbPoint, false);
        for (const polygon_t & face : output.faces)
            for (unsigned int corner = 0; corner < 4; ++corner)
            {
                if (face.vertIdx[corner] >= mesh.nbPoint) return false;
                used[face.vertIdx[corner]] = true;
            }
        std::vector<unsigned int> remap(mesh.nbPoint, 0xFFFFFFFFu);
        for (unsigned int point = 0; point < mesh.nbPoint; ++point)
            if (used[point])
            {
                remap[point] = output.sourcePoints.size();
                output.sourcePoints.push_back(point);
                output.points.push_back(mesh.pntbl[point]);
            }
        if (output.sourcePoints.empty() || lodMeshes[meshIndex].empty()) return false;
        if (output.sourcePoints.size() > (model->sourceSgl ? 65535u : 256u)) return false;
        if (rebakeTexture)
            for (polygon_t & face : output.faces)
                if (!bakeCachedSourceTexture(model, &face)) return false;
        for (polygon_t & face : output.faces)
            for (unsigned int corner = 0; corner < 4; ++corner)
                face.vertIdx[corner] = remap[face.vertIdx[corner]];
        totalPoints += output.points.size();
    }

    std::vector<std::vector<vertex_t> > framePoints(model->nbFrames,
        std::vector<vertex_t>(totalPoints));
    size_t sourceOffset = 0, destinationOffset = 0;
    for (unsigned int frame = 0; frame < model->nbFrames; ++frame)
    {
        sourceOffset = 0;
        destinationOffset = 0;
        for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
        {
            for (unsigned int point : reduced[mesh].sourcePoints)
                framePoints[frame][destinationOffset++] = model->keyFrames[frame].cVert[sourceOffset + point];
            sourceOffset += model->model[mesh].nbPoint;
        }
    }
    std::vector<unsigned int> previewPointRemap;
    previewPointRemap.reserve(totalPoints);
    sourceOffset = 0;
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
    {
        for (unsigned int point : reduced[mesh].sourcePoints)
            previewPointRemap.push_back(static_cast<unsigned int>(sourceOffset + point));
        sourceOffset += model->model[mesh].nbPoint;
    }

    std::vector<vertex_t *> pointArrays(model->nbModels, NULL);
    std::vector<polygon_t *> faceArrays(model->nbModels, NULL);
    std::vector<vertex_t *> frameArrays(model->nbFrames, NULL);
    const auto discardArrays = [&]()
    {
        for (vertex_t * array : pointArrays) delete [] array;
        for (polygon_t * array : faceArrays) delete [] array;
        for (vertex_t * array : frameArrays) delete [] array;
    };
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
    {
        if (!reduced[mesh].points.empty())
        {
            pointArrays[mesh] = new (std::nothrow) vertex_t[reduced[mesh].points.size()];
            if (!pointArrays[mesh]) { discardArrays(); return false; }
            std::copy(reduced[mesh].points.begin(), reduced[mesh].points.end(), pointArrays[mesh]);
        }
        if (!reduced[mesh].faces.empty())
        {
            faceArrays[mesh] = new (std::nothrow) polygon_t[reduced[mesh].faces.size()];
            if (!faceArrays[mesh]) { discardArrays(); return false; }
            std::copy(reduced[mesh].faces.begin(), reduced[mesh].faces.end(), faceArrays[mesh]);
        }
    }
    for (unsigned int frame = 0; frame < model->nbFrames; ++frame)
    {
        if (totalPoints)
        {
            frameArrays[frame] = new (std::nothrow) vertex_t[totalPoints];
            if (!frameArrays[frame]) { discardArrays(); return false; }
            std::copy(framePoints[frame].begin(), framePoints[frame].end(), frameArrays[frame]);
        }
    }
    if (!setFbxBonePreviewPointRemap(model, previewPointRemap))
    {
        discardArrays();
        return false;
    }
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
    {
        delete [] model->model[mesh].pntbl;
        delete [] model->model[mesh].pltbl;
        model->model[mesh].pntbl = pointArrays[mesh];
        model->model[mesh].pltbl = faceArrays[mesh];
        model->model[mesh].nbPoint = reduced[mesh].points.size();
        model->model[mesh].nbPolygon = reduced[mesh].faces.size();
        pointArrays[mesh] = NULL;
        faceArrays[mesh] = NULL;
    }
    for (unsigned int frame = 0; frame < model->nbFrames; ++frame)
    {
        delete [] model->keyFrames[frame].cVert;
        model->keyFrames[frame].cVert = frameArrays[frame];
        frameArrays[frame] = NULL;
    }
    model->lodData->levels.clear();
    model->lodData->sglFrameNormals.clear();
    model->lodData->revision = originalBaseSnapshot.lodRevision + 1;
    discardArrays();
    return true;
}
pixel_t averageTextureColor(const texture_t & texture)
{
    pixel_t color = {};
    color.r = color.g = color.b = 192;
    color.a = 255;
    double red = 0.0, green = 0.0, blue = 0.0, weight = 0.0;
    if (texture.pixel)
        for (size_t index = 0; index < size_t(texture.width) * texture.height; ++index)
        {
            const pixel_t & pixel = texture.pixel[index];
            red += pixel.r * double(pixel.a);
            green += pixel.g * double(pixel.a);
            blue += pixel.b * double(pixel.a);
            weight += pixel.a;
        }
    if (weight > 0.0)
    {
        color.r = static_cast<uint8_t>(red / weight + 0.5);
        color.g = static_cast<uint8_t>(green / weight + 0.5);
        color.b = static_cast<uint8_t>(blue / weight + 0.5);
    }
    color.rgb = C_RGB(color.r / 8, color.g / 8, color.b / 8);
    return color;
}

unsigned int faceCorners(const polygon_t & face, unsigned int * indices, unsigned int * cornerSlots)
{
    unsigned int count = 0;
    for (unsigned int corner = 0; corner < 4; ++corner)
    {
        unsigned int vertex = face.vertIdx[corner];
        if (std::find(indices, indices + count, vertex) == indices + count)
        {
            indices[count] = vertex;
            cornerSlots[count++] = corner;
        }
    }
    return count;
}

bool triangleHasArea(const unsigned int * indices, const vertex_t * points, float minimumAreaSquared)
{
    const float * first = points[indices[0]].point;
    const float * second = points[indices[1]].point;
    const float * third = points[indices[2]].point;
    float edge[3], next[3];
    for (unsigned int axis = 0; axis < 3; ++axis)
    {
        edge[axis] = second[axis] - first[axis];
        next[axis] = third[axis] - first[axis];
    }
    float cross[3] = {edge[Y] * next[Z] - edge[Z] * next[Y],
                      edge[Z] * next[X] - edge[X] * next[Z],
                      edge[X] * next[Y] - edge[Y] * next[X]};
    float areaSquared = cross[X] * cross[X] + cross[Y] * cross[Y] + cross[Z] * cross[Z];
    return std::isfinite(areaSquared) && areaSquared > minimumAreaSquared;
}

bool validAnimation(const std::vector<unsigned int> & indices, const animated_model_t * model,
                    unsigned int offset, const model_t & mesh, float minimumAreaSquared)
{
    for (size_t triangle = 0; triangle < indices.size(); triangle += 3)
    {
        const unsigned int * corners = &indices[triangle];
        if (!triangleHasArea(corners, mesh.pntbl, minimumAreaSquared)) return false;
        for (unsigned int frame = 0; frame < model->nbFrames; ++frame)
        {
            const vertex_t * points = model->keyFrames[frame].cVert + offset;
            const vertex_t * next = model->keyFrames[(frame + 1) % model->nbFrames].cVert + offset;
            if (!triangleHasArea(corners, points, minimumAreaSquared)) return false;
            const unsigned int localIndices[] = {0, 1, 2};
            vertex_t interpolated[3] = {};
            for (unsigned int sample = 1; sample < 4; ++sample)
            {
                float fraction = sample * 0.25f;
                for (unsigned int corner = 0; corner < 3; ++corner)
                    for (unsigned int axis = 0; axis < 3; ++axis)
                    {
                        float start = points[corners[corner]].point[axis];
                        interpolated[corner].point[axis] = start + (next[corners[corner]].point[axis] - start) * fraction;
                    }
                if (!triangleHasArea(localIndices, interpolated, minimumAreaSquared)) return false;
            }
        }
    }
    return true;
}
}

bool mergeBaseTrianglesIntoQuads(animated_model_t * model)
{
    if (!model || !model->model || !model->nbModels) return false;
    std::vector<std::vector<polygon_t> > mergedFaces(model->nbModels);
    bool changed = false;
    for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
    {
        const model_t & mesh = model->model[meshIndex];
        if ((mesh.nbPoint && !mesh.pntbl) || (mesh.nbPolygon && !mesh.pltbl)) return false;
        if (mesh.nbPolygon) mergedFaces[meshIndex].assign(mesh.pltbl, mesh.pltbl + mesh.nbPolygon);
        for (const polygon_t & face : mergedFaces[meshIndex])
            for (unsigned int corner = 0; corner < 4; ++corner)
                if (face.vertIdx[corner] >= mesh.nbPoint) return false;
        const size_t originalCount = mergedFaces[meshIndex].size();
        std::vector<int> sourceMaterials;
        const LodSourceTextureCache * cache = findSourceTextureCache(model);
        const bool rebakeTexture = useUV && cache;
        if (rebakeTexture)
        {
            sourceMaterials.resize(mergedFaces[meshIndex].size());
            for (unsigned int face = 0; face < sourceMaterials.size(); ++face)
                sourceMaterials[face] = sourceMaterialIndex(cache, meshIndex, face, mergedFaces[meshIndex][face].texture);
        }
        std::vector<bool> mergedFlags;
        std::vector<int> mergedMaterials;
        mergeAdjacentTriangles(mesh, mergedFaces[meshIndex], rebakeTexture,
                               rebakeTexture ? &sourceMaterials : NULL, &mergedFlags,
                               rebakeTexture ? &mergedMaterials : NULL);
        changed = changed || mergedFaces[meshIndex].size() != originalCount;
        if (rebakeTexture)
        {
            const unsigned int textureCountBefore = model->nbTextures;
            for (unsigned int face = 0; face < mergedFaces[meshIndex].size(); ++face)
                if (mergedFlags[face] && !bakeCachedSourceTexture(model, &mergedFaces[meshIndex][face]))
                {
                    if (model->texture)
                        for (unsigned int texture = textureCountBefore; texture < model->nbTextures; ++texture)
                            delete [] model->texture[texture].pixel;
                    model->nbTextures = textureCountBefore;
                    return false;
                }
            LodSourceTextureCache * mutableCache = findSourceTextureCache(model);
            if (mutableCache)
            {
                std::vector<CachedSourceFace> updated(mergedFaces[meshIndex].size());
                for (unsigned int face = 0; face < mergedFaces[meshIndex].size(); ++face)
                {
                    updated[face].texture = mergedMaterials[face];
                    std::copy(mergedFaces[meshIndex][face].vertIdx,
                              mergedFaces[meshIndex][face].vertIdx + 4, updated[face].vertIdx);
                    std::copy(mergedFaces[meshIndex][face].vertUV,
                              mergedFaces[meshIndex][face].vertUV + 4, updated[face].uv);
                }
                mutableCache->faces[meshIndex].swap(updated);
            }
        }
    }
    if (!changed) return false;

    std::vector<polygon_t *> faceArrays(model->nbModels, NULL);
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
        if (!mergedFaces[mesh].empty())
        {
            faceArrays[mesh] = new (std::nothrow) polygon_t[mergedFaces[mesh].size()];
            if (!faceArrays[mesh])
            {
                for (polygon_t * array : faceArrays) delete [] array;
                return false;
            }
            std::copy(mergedFaces[mesh].begin(), mergedFaces[mesh].end(), faceArrays[mesh]);
        }
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
    {
        delete [] model->model[mesh].pltbl;
        model->model[mesh].pltbl = faceArrays[mesh];
        model->model[mesh].nbPolygon = mergedFaces[mesh].size();
    }
    if (model->lodData)
    {
        model->lodData->sglFrameNormals.clear();
        ++model->lodData->revision;
    }
    return true;
}

void resetAutomaticLod()
{
    showAutomaticLod = false;
    lodSource = NULL;
    lodMeshes.clear();
    lodPointCount = 0;
}

void cacheAutomaticLodSourceTextures(animated_model_t * model, const texture_t * textures,
                                    unsigned int textureCount)
{
    discardAutomaticLodSourceTextures(model);
    if (!model || !model->model || !textures || !textureCount) return;
    LodSourceTextureCache cache;
    cache.owner = model;
    cache.textures.resize(textureCount);
    for (unsigned int index = 0; index < textureCount; ++index)
    {
        CachedSourceTexture & saved = cache.textures[index];
        saved.texture = textures[index];
        saved.texture.pixel = NULL;
        const size_t pixelCount = size_t(textures[index].width) * textures[index].height;
        if (textures[index].pixel && pixelCount)
        {
            saved.pixels.assign(textures[index].pixel, textures[index].pixel + pixelCount);
            saved.texture.pixel = saved.pixels.data();
        }
    }
    cache.faces.resize(model->nbModels);
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
    {
        const model_t & part = model->model[mesh];
        cache.faces[mesh].resize(part.nbPolygon);
        for (unsigned int face = 0; face < part.nbPolygon; ++face)
        {
            CachedSourceFace & saved = cache.faces[mesh][face];
            const polygon_t & polygon = part.pltbl[face];
            saved.texture = polygon.texture >= 0 && polygon.texture < static_cast<int>(textureCount) ?
                polygon.texture : static_cast<int>(textureCount - 1);
            std::copy(polygon.vertIdx, polygon.vertIdx + 4, saved.vertIdx);
            std::copy(polygon.vertUV, polygon.vertUV + 4, saved.uv);
        }
    }
    lodSourceTextureCaches.push_back(std::move(cache));
}

void discardAutomaticLodSourceTextures(const animated_model_t * model)
{
    lodSourceTextureCaches.erase(std::remove_if(lodSourceTextureCaches.begin(), lodSourceTextureCaches.end(),
        [&](const LodSourceTextureCache & cache) { return cache.owner == model; }), lodSourceTextureCaches.end());
}

bool getCachedSourceTextureForSurface(const animated_model_t * model, unsigned int meshIndex,
                                     unsigned int faceIndex, texture_t * sourceTexture)
{
    if (!sourceTexture) return false;
    LodSourceTextureCache * cache = findSourceTextureCache(model);
    if (!cache || meshIndex >= cache->faces.size() ||
        faceIndex >= cache->faces[meshIndex].size()) return false;
    const int textureIndex = cache->faces[meshIndex][faceIndex].texture;
    if (textureIndex < 0 || textureIndex >= static_cast<int>(cache->textures.size())) return false;
    CachedSourceTexture & cached = cache->textures[textureIndex];
    if (cached.pixels.empty() || !cached.texture.width || !cached.texture.height) return false;
    *sourceTexture = cached.texture;
    sourceTexture->pixel = cached.pixels.data();
    return true;
}

bool generateAutomaticLod(animated_model_t * model, unsigned int reductionPercent, unsigned int replaceLevel)
{
    if (!model || !model->model || model->nbModels == 0) return false;
    reductionPercent = MIN(reductionPercent, 99u);
    if (replaceLevel)
    {
        if (!reductionPercent || replaceLevel > automaticLodLevelCount(model)) return false;
        for (unsigned int level = 0; level < model->lodData->levels.size(); ++level)
            if (level + 1 != replaceLevel && model->lodData->levels[level].reduction == reductionPercent)
                return false;
    }
    resetAutomaticLod();
    if (model->nbFrames && !model->keyFrames) return false;
    for (unsigned int frame = 0; frame < model->nbFrames; ++frame)
        if (!model->keyFrames[frame].cVert) return false;
    reductionPercent = MIN(reductionPercent, 99u);
    std::vector<pixel_t> textureColors(model->nbTextures);
    if (model->nbTextures && !model->texture) return false;
    for (unsigned int texture = 0; texture < model->nbTextures; ++texture)
        textureColors[texture] = averageTextureColor(model->texture[texture]);
    lodMeshes.resize(model->nbModels);
    unsigned int offset = 0;
    for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
    {
        const model_t & mesh = model->model[meshIndex];
        const LodSourceTextureCache * textureCache = findSourceTextureCache(model);
        const bool textureAware = useUV && textureCache;
        if ((mesh.nbPoint && !mesh.pntbl) || (mesh.nbPolygon && !mesh.pltbl))
        {
            resetAutomaticLod();
            return false;
        }
        const vertex_t * reference = model->nbFrames ? model->keyFrames[0].cVert + offset : mesh.pntbl;
        for (unsigned int vertex = 0; vertex < mesh.nbPoint; ++vertex)
            for (unsigned int pose = 0; pose <= model->nbFrames; ++pose)
            {
                const vertex_t * points = pose == model->nbFrames ? mesh.pntbl : model->keyFrames[pose].cVert + offset;
                for (unsigned int axis = 0; axis < 3; ++axis)
                    if (!std::isfinite(points[vertex].point[axis]))
                    {
                        resetAutomaticLod();
                        return false;
                    }
            }
        std::vector<polygon_t> source;
        std::vector<unsigned int> sourceFaceIndices;
        std::vector<unsigned int> indices;
        std::vector<std::pair<int, std::vector<unsigned int> > > materialIndexGroups;
        std::vector<std::array<float, 3> > centers;
        std::vector<std::array<float, 3> > vertexColors(mesh.nbPoint, {{0.0f, 0.0f, 0.0f}});
        std::vector<unsigned int> colorUses(mesh.nbPoint, 0);
        for (unsigned int faceIndex = 0; faceIndex < mesh.nbPolygon; ++faceIndex)
        {
            polygon_t face = mesh.pltbl[faceIndex];
            if (useUV && textureCache && meshIndex < textureCache->faces.size() &&
                faceIndex < textureCache->faces[meshIndex].size())
            {
                const CachedSourceFace & original = textureCache->faces[meshIndex][faceIndex];
                std::copy(original.vertIdx, original.vertIdx + 4, face.vertIdx);
                std::copy(original.uv, original.uv + 4, face.vertUV);
            }
            for (unsigned int corner = 0; corner < 4; ++corner)
                if (face.vertIdx[corner] >= mesh.nbPoint)
                {
                    resetAutomaticLod();
                    return false;
                }
            unsigned int corners[4], cornerSlots[4];
            unsigned int count = faceCorners(face, corners, cornerSlots);
            if (count < 3) continue;
            if (count == 3)
            {
                uv_t originalUv[4];
                std::copy(face.vertUV, face.vertUV + 4, originalUv);
                for (unsigned int corner = 0; corner < 3; ++corner)
                {
                    face.vertIdx[corner] = corners[corner];
                    face.vertUV[corner] = originalUv[cornerSlots[corner]];
                }
                face.vertIdx[3] = face.vertIdx[2];
                face.vertUV[3] = face.vertUV[2];
            }
            if (face.texture >= 0 && static_cast<unsigned int>(face.texture) < textureColors.size())
                face.color = textureColors[face.texture];
            else if (face.color.a == 0)
            {
                face.color.r = face.color.g = face.color.b = 192;
                face.color.a = 255;
            }
            face.color.rgb = C_RGB(face.color.r / 8, face.color.g / 8, face.color.b / 8);
            source.push_back(face);
            sourceFaceIndices.push_back(faceIndex);
            std::array<float, 3> center = {{0.0f, 0.0f, 0.0f}};
            for (unsigned int corner = 0; corner < count; ++corner)
            {
                unsigned int vertex = corners[corner];
                for (unsigned int axis = 0; axis < 3; ++axis) center[axis] += reference[vertex].point[axis] / count;
                vertexColors[vertex][0] += face.color.r / 255.0f;
                vertexColors[vertex][1] += face.color.g / 255.0f;
                vertexColors[vertex][2] += face.color.b / 255.0f;
                ++colorUses[vertex];
            }
            centers.push_back(center);
            indices.insert(indices.end(), {corners[0], corners[1], corners[2]});
            if (count == 4) indices.insert(indices.end(), {corners[0], corners[2], corners[3]});
        }
        std::vector<unsigned int> sourceCharts(source.size());
        std::vector<unsigned char> topologyLocks(mesh.nbPoint, 0);
        for (unsigned int face = 0; face < source.size(); ++face) sourceCharts[face] = face;
        const auto chartRoot = [&](unsigned int face)
        {
            while (sourceCharts[face] != face)
            {
                sourceCharts[face] = sourceCharts[sourceCharts[face]];
                face = sourceCharts[face];
            }
            return face;
        };
        if (textureAware)
        {
            std::unordered_map<uint64_t, std::vector<unsigned int> > edgeFaces;
            for (unsigned int face = 0; face < source.size(); ++face)
            {
                const polygon_t & polygon = source[face];
                const unsigned int count = polygon.vertIdx[2] == polygon.vertIdx[3] ? 3u : 4u;
                for (unsigned int edge = 0; edge < count; ++edge)
                {
                    const unsigned int start = polygon.vertIdx[edge];
                    const unsigned int end = polygon.vertIdx[(edge + 1) % count];
                    auto & neighbors = edgeFaces[undirectedEdgeKey(start, end)];
                    for (unsigned int neighbor : neighbors)
                    {
                        if (sourceMaterialIndex(textureCache, meshIndex, sourceFaceIndices[face], polygon.texture) !=
                            sourceMaterialIndex(textureCache, meshIndex, sourceFaceIndices[neighbor], source[neighbor].texture)) continue;
                        bool continuous = true;
                        for (unsigned int corner = 0; corner < 4; ++corner)
                            if (source[neighbor].vertIdx[corner] == start || source[neighbor].vertIdx[corner] == end)
                            {
                                const uv_t & first = polygon.vertUV[source[neighbor].vertIdx[corner] == start ? edge : (edge + 1) % count];
                                const uv_t & second = source[neighbor].vertUV[corner];
                                if (std::fabs(first.U - second.U) > 1e-4f || std::fabs(first.V - second.V) > 1e-4f)
                                    continuous = false;
                            }
                        if (continuous) sourceCharts[chartRoot(face)] = chartRoot(neighbor);
                    }
                    neighbors.push_back(face);
                }
            }
            for (const auto & edge : edgeFaces)
                if (edge.second.size() > 2)
                    for (unsigned int face : edge.second)
                        for (unsigned int corner = 0; corner < 4; ++corner)
                            topologyLocks[source[face].vertIdx[corner]] = meshopt_SimplifyVertex_Lock;
            for (unsigned int face = 0; face < source.size(); ++face)
            {
                const unsigned int chart = chartRoot(face);
                sourceCharts[face] = chart;
                auto group = std::find_if(materialIndexGroups.begin(), materialIndexGroups.end(),
                    [&](const std::pair<int, std::vector<unsigned int> > & candidate)
                    { return candidate.first == static_cast<int>(chart); });
                if (group == materialIndexGroups.end())
                {
                    materialIndexGroups.push_back(std::make_pair(static_cast<int>(chart), std::vector<unsigned int>()));
                    group = materialIndexGroups.end() - 1;
                }
                const polygon_t & polygon = source[face];
                group->second.insert(group->second.end(), {polygon.vertIdx[0], polygon.vertIdx[1], polygon.vertIdx[2]});
                if (polygon.vertIdx[2] != polygon.vertIdx[3])
                    group->second.insert(group->second.end(), {polygon.vertIdx[0], polygon.vertIdx[2], polygon.vertIdx[3]});
            }
        }
        std::vector<std::vector<std::pair<size_t, unsigned int> > > sourceCornersByPoint(mesh.nbPoint);
        for (size_t sourceFace = 0; sourceFace < source.size(); ++sourceFace)
            for (unsigned int corner = 0; corner < 4; ++corner)
                sourceCornersByPoint[source[sourceFace].vertIdx[corner]].push_back(std::make_pair(sourceFace, corner));
        std::vector<polygon_t> & faces = lodMeshes[meshIndex];
        const auto useSourceFaces = [&]()
        {
            faces = source;
            if (useUV && textureCache)
                for (size_t face = 0; face < faces.size(); ++face)
                {
                    const unsigned int sourceFace = sourceFaceIndices[face];
                    faces[face].texture = sourceMaterialIndex(textureCache, meshIndex, sourceFace, faces[face].texture);
                }
        };
        if (reductionPercent == 0 || indices.empty()) useSourceFaces();
        else
        {
            float extent = MAX(meshopt_simplifyScale(reference[0].point, mesh.nbPoint, sizeof(vertex_t)), 1e-6f);
            unsigned int motionSamples = MIN(model->nbFrames, 9u);
            const unsigned int motionOffset = 3;
            unsigned int attributeCount = motionOffset + motionSamples * 3;
            std::vector<float> attributes(size_t(mesh.nbPoint) * attributeCount, 0.0f);
            std::vector<float> weights(attributeCount, 0.3f);
            for (unsigned int attribute = motionOffset; attribute < attributeCount; ++attribute)
                weights[attribute] = 0.5f / extent;
            for (unsigned int vertex = 0; vertex < mesh.nbPoint; ++vertex)
            {
                for (unsigned int channel = 0; channel < 3; ++channel)
                    attributes[size_t(vertex) * attributeCount + channel] = colorUses[vertex] ? vertexColors[vertex][channel] / colorUses[vertex] : 0.0f;
                for (unsigned int sample = 0; sample < motionSamples; ++sample)
                {
                    unsigned int frame = sample * model->nbFrames / motionSamples;
                    for (unsigned int axis = 0; axis < 3; ++axis)
                        attributes[size_t(vertex) * attributeCount + motionOffset + sample * 3 + axis] =
                            model->keyFrames[frame].cVert[offset + vertex].point[axis] - reference[vertex].point[axis];
                }
            }
            const size_t sourceTriangles = indices.size() / 3;
            const size_t minimumTriangles = MIN(size_t(4), sourceTriangles);
            const size_t targetFaces = MIN(source.size(),
                MAX(size_t(4), (source.size() * (100u - reductionPercent) + 99u) / 100u));
            const size_t targetTriangles = MIN(sourceTriangles,
                MAX(minimumTriangles, (targetFaces * sourceTriangles + source.size() - 1) / source.size()));
            const size_t targetIndices = MIN(indices.size(), targetTriangles * 3);
            float minimumAreaSquared = MAX(1e-20f, extent * extent * extent * extent * 1e-14f);
            const float maximumError = std::numeric_limits<float>::max();
            const unsigned int simplifyOptions = meshopt_SimplifyRegularizeLight | meshopt_SimplifyLockBorder |
                (textureAware ? 0u : meshopt_SimplifyPermissive);
            const auto simplifyIndices = [&](const std::vector<unsigned int> & input, size_t initialTarget)
            {
                std::vector<unsigned int> result(input.size());
                std::vector<unsigned char> vertexLocks = topologyLocks;
                if (textureAware)
                {
                    std::unordered_map<uint64_t, unsigned int> edgeCounts;
                    for (size_t triangle = 0; triangle < input.size(); triangle += 3)
                        for (unsigned int edge = 0; edge < 3; ++edge)
                            ++edgeCounts[undirectedEdgeKey(input[triangle + edge], input[triangle + (edge + 1) % 3])];
                    for (size_t triangle = 0; triangle < input.size(); triangle += 3)
                        for (unsigned int edge = 0; edge < 3; ++edge)
                        {
                            const unsigned int first = input[triangle + edge];
                            const unsigned int second = input[triangle + (edge + 1) % 3];
                            const unsigned int uses = edgeCounts[undirectedEdgeKey(first, second)];
                            if (uses != 2)
                                vertexLocks[first] = vertexLocks[second] = meshopt_SimplifyVertex_Lock;
                            if (uses > 2)
                                for (unsigned int corner = 0; corner < 3; ++corner)
                                    vertexLocks[input[triangle + corner]] = meshopt_SimplifyVertex_Lock;
                        }
                }
                size_t budget = MIN(input.size(), initialTarget);
                while (true)
                {
                    const size_t count = meshopt_simplifyWithAttributes(result.data(), input.data(), input.size(),
                        reference[0].point, mesh.nbPoint, sizeof(vertex_t), attributes.data(), attributeCount * sizeof(float),
                        weights.data(), attributeCount, textureAware ? vertexLocks.data() : NULL,
                        budget, maximumError, simplifyOptions, NULL);
                    result.resize(count);
                    if (!result.empty() && validAnimation(result, model, offset, mesh, minimumAreaSquared)) break;
                    if (budget == input.size())
                    {
                        result.clear();
                        break;
                    }
                    budget = MIN(input.size(), MAX(budget + MAX(size_t(3), input.size() / 30 * 3), count + 3));
                    result.resize(input.size());
                }
                return result;
            };
            std::vector<unsigned int> simplified;
            std::vector<unsigned int> simplifiedCharts;
            if (textureAware)
            {
                for (const auto & group : materialIndexGroups)
                {
                    const size_t totalTriangles = indices.size() / 3;
                    const size_t groupTriangles = group.second.size() / 3;
                    const size_t groupTargetTriangles = MAX(size_t(1),
                        (targetTriangles * groupTriangles + totalTriangles - 1) / totalTriangles);
                    std::vector<unsigned int> reduced = simplifyIndices(group.second, groupTargetTriangles * 3);
                    const std::vector<unsigned int> & result = reduced.empty() ? group.second : reduced;
                    simplified.insert(simplified.end(), result.begin(), result.end());
                    simplifiedCharts.insert(simplifiedCharts.end(), result.size() / 3, group.first);
                }
            }
            else simplified = simplifyIndices(indices, targetIndices);
            if (simplified.empty() || simplified.size() >= indices.size()) useSourceFaces();
            else
                for (size_t triangle = 0; triangle < simplified.size(); triangle += 3)
                {
                    std::array<float, 3> center = {{0.0f, 0.0f, 0.0f}};
                    for (unsigned int corner = 0; corner < 3; ++corner)
                        for (unsigned int axis = 0; axis < 3; ++axis)
                            center[axis] += reference[simplified[triangle + corner]].point[axis] / 3.0f;
                    size_t nearest = 0;
                    float nearestDistance = std::numeric_limits<float>::max();
                    for (size_t sourceFace = 0; sourceFace < source.size(); ++sourceFace)
                    {
                        if (textureAware && sourceCharts[sourceFace] != simplifiedCharts[triangle / 3]) continue;
                        const float distance = pointFaceDistanceSquared(center.data(), mesh, source[sourceFace]);
                        if (distance < nearestDistance)
                        {
                            nearest = sourceFace;
                            nearestDistance = distance;
                        }
                    }
                    const polygon_t sourceFace = source[nearest];
                    const unsigned int sourceFaceIndex = sourceFaceIndices[nearest];
                    const int sourceMaterial = sourceMaterialIndex(textureCache, meshIndex, sourceFaceIndex,
                                                                    sourceFace.texture);
                    polygon_t face = sourceFace;
                    for (unsigned int corner = 0; corner < 3; ++corner)
                    {
                        const unsigned int point = simplified[triangle + corner];
                        face.vertIdx[corner] = point;
                        if (useUV && textureCache)
                        {
                            size_t exactSourceFace = source.size();
                            unsigned int exactSourceCorner = 0;
                            float exactDistance = std::numeric_limits<float>::max();
                            for (const auto & occurrence : sourceCornersByPoint[point])
                            {
                                if (sourceCharts[occurrence.first] != simplifiedCharts[triangle / 3]) continue;
                                float distance = 0.0f;
                                for (unsigned int axis = 0; axis < 3; ++axis)
                                {
                                    const float delta = center[axis] - centers[occurrence.first][axis];
                                    distance += delta * delta;
                                }
                                if (distance < exactDistance)
                                {
                                    exactDistance = distance;
                                    exactSourceFace = occurrence.first;
                                    exactSourceCorner = occurrence.second;
                                }
                            }
                            if (exactSourceFace < source.size())
                            {
                                face.vertUV[corner] = source[exactSourceFace].vertUV[exactSourceCorner];
                                continue;
                            }
                            size_t uvSourceFace = source.size();
                            float nearestUvDistance = std::numeric_limits<float>::max();
                            for (size_t candidateIndex = 0; candidateIndex < source.size(); ++candidateIndex)
                            {
                                if (sourceCharts[candidateIndex] != simplifiedCharts[triangle / 3]) continue;
                                const int candidateMaterial = sourceMaterialIndex(textureCache, meshIndex,
                                    sourceFaceIndices[candidateIndex], source[candidateIndex].texture);
                                if (candidateMaterial != sourceMaterial) continue;
                                const float distance = pointFaceDistanceSquared(reference[point].point,
                                    mesh, source[candidateIndex]);
                                if (distance < nearestUvDistance)
                                {
                                    uvSourceFace = candidateIndex;
                                    nearestUvDistance = distance;
                                }
                            }
                            const polygon_t & uvFace = uvSourceFace < source.size() ? source[uvSourceFace] : sourceFace;
                            if (!interpolateFaceUv(reference[point].point, mesh, uvFace, face.vertUV[corner]))
                                face.vertUV[corner] = uvFace.vertUV[0];
                        }
                        else
                        {
                            size_t uvSourceFace = source.size();
                            unsigned int uvSourceCorner = 0;
                            float nearestUvFaceDistance = std::numeric_limits<float>::max();
                            for (const std::pair<size_t, unsigned int> & occurrence : sourceCornersByPoint[point])
                            {
                                const polygon_t & candidate = source[occurrence.first];
                                const int candidateMaterial = sourceMaterialIndex(textureCache, meshIndex,
                                    sourceFaceIndices[occurrence.first], candidate.texture);
                                if (candidateMaterial != sourceMaterial) continue;
                                float distance = 0.0f;
                                for (unsigned int axis = 0; axis < 3; ++axis)
                                {
                                    const float delta = center[axis] - centers[occurrence.first][axis];
                                    distance += delta * delta;
                                }
                                if (distance < nearestUvFaceDistance)
                                {
                                    uvSourceFace = occurrence.first;
                                    uvSourceCorner = occurrence.second;
                                    nearestUvFaceDistance = distance;
                                }
                            }
                            face.vertUV[corner] = uvSourceFace < source.size() ?
                                source[uvSourceFace].vertUV[uvSourceCorner] : sourceFace.vertUV[corner];
                        }
                    }
                    if (useUV && textureCache) face.texture = sourceMaterial;
                    face.vertIdx[3] = face.vertIdx[2];
                    face.vertUV[3] = face.vertUV[2];
                    faces.push_back(face);
                }
        }
                mergeAdjacentTriangles(mesh, faces, false);
        std::vector<bool> used(mesh.nbPoint, false);
        for (const polygon_t & face : faces)
            for (unsigned int corner = 0; corner < 4; ++corner) used[face.vertIdx[corner]] = true;
        lodPointCount += std::count(used.begin(), used.end(), true);
        offset += mesh.nbPoint;
    }
    lodSource = model;
    if (reductionPercent)
    {
        if (!model->lodData) model->lodData = new model_lod_data_t;
        lod_level_t level;
        level.reduction = reductionPercent;
        level.meshes.resize(model->nbModels);
        for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
        {
            level.meshes[mesh].faces = lodMeshes[mesh];
            for (const polygon_t & face : lodMeshes[mesh])
                for (unsigned int corner = 0; corner < 4; ++corner)
                    level.meshes[mesh].pointCount = MAX(level.meshes[mesh].pointCount, face.vertIdx[corner] + 1);
        }
        auto & levels = model->lodData->levels;
        auto existing = replaceLevel ? levels.begin() + replaceLevel - 1 : std::find_if(levels.begin(), levels.end(), [&](const lod_level_t & value) {
            return value.reduction == reductionPercent;
        });
        if (existing != levels.end()) *existing = level;
        else if (levels.size() < 63) levels.push_back(level);
        else return false;
        std::sort(levels.begin(), levels.end(), [](const lod_level_t & first, const lod_level_t & second) {
            return first.reduction < second.reduction;
        });
        ++model->lodData->revision;
    }
    return true;
}

bool hasAutomaticLodBaseSnapshot(const animated_model_t * model)
{
    return model && originalBaseSnapshot.owner == model;
}

void discardAutomaticLodBaseSnapshot(const animated_model_t * model)
{
    if (model && originalBaseSnapshot.owner == model) clearBaseSnapshot();
}

bool restoreAutomaticLodBase(animated_model_t * model)
{
    if (!hasAutomaticLodBaseSnapshot(model) || !restoreBaseSnapshotData(model)) return false;
    clearBaseSnapshot();
    resetAutomaticLod();
    return true;
}

bool replaceAutomaticLodBase(animated_model_t * model, unsigned int reductionPercent)
{
    if (!model || !reductionPercent || reductionPercent > 99 ||
        (originalBaseSnapshot.owner && originalBaseSnapshot.owner != model)) return false;
    const bool alreadyReplaced = hasAutomaticLodBaseSnapshot(model);
    if (alreadyReplaced)
    {
        if (!restoreBaseSnapshotData(model)) return false;
    }
    else if (!captureBaseSnapshot(model)) return false;

    if (!generateAutomaticLod(model, reductionPercent) || !buildReducedBase(model))
    {
        resetAutomaticLod();
        restoreBaseSnapshotData(model);
        clearBaseSnapshot();
        return false;
    }
    resetAutomaticLod();
    return true;
}

unsigned int automaticLodLevelCount(const animated_model_t * model)
{
    return model && model->lodData ? static_cast<unsigned int>(model->lodData->levels.size()) : 0;
}

bool deleteAutomaticLod(animated_model_t * model, unsigned int level)
{
    if (!level || level > automaticLodLevelCount(model)) return false;
    model->lodData->levels.erase(model->lodData->levels.begin() + level - 1);
    ++model->lodData->revision;
    if (lodSource == model) resetAutomaticLod();
    return true;
}

bool selectAutomaticLod(animated_model_t * model, unsigned int level)
{
    if (!level || level > automaticLodLevelCount(model)) return false;
    resetAutomaticLod();
    lodSource = model;
    lodMeshes.resize(model->nbModels);
    lodPointCount = 0;
    const lod_level_t & selected = model->lodData->levels[level - 1];
    if (selected.meshes.size() != model->nbModels) return false;
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
    {
        lodMeshes[mesh] = selected.meshes[mesh].faces;
        std::vector<bool> used(model->model[mesh].nbPoint, false);
        for (const polygon_t & face : lodMeshes[mesh])
            for (unsigned int corner = 0; corner < 4; ++corner)
                if (face.vertIdx[corner] < used.size()) used[face.vertIdx[corner]] = true;
        lodPointCount += std::count(used.begin(), used.end(), true);
    }
    showAutomaticLod = true;
    return true;
}

const polygon_t * automaticLodFaces(const animated_model_t * model, unsigned int meshIndex, unsigned int * count)
{
    if (!count) return NULL;
    *count = 0;
    if (model != lodSource || meshIndex >= lodMeshes.size()) return NULL;
    *count = lodMeshes[meshIndex].size();
    return lodMeshes[meshIndex].empty() ? NULL : lodMeshes[meshIndex].data();
}

unsigned int automaticLodFaceCount()
{
    unsigned int count = 0;
    for (const std::vector<polygon_t> & faces : lodMeshes) count += faces.size();
    return count;
}

unsigned int automaticLodPointCount()
{
    return lodPointCount;
}