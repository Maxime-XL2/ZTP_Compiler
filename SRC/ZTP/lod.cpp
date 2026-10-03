#include "../COMMON.H"
#include "../../third_party/meshoptimizer/meshoptimizer.h"
#include <algorithm>
#include <array>
#include <limits>
#include <vector>

bool showAutomaticLod = false;

namespace
{
const animated_model_t * lodSource = NULL;
std::vector<std::vector<polygon_t> > lodMeshes;
unsigned int lodPointCount = 0;

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

unsigned int faceCorners(const polygon_t & face, unsigned int * indices)
{
    unsigned int count = 0;
    for (unsigned int corner = 0; corner < 4; ++corner)
    {
        unsigned int vertex = face.vertIdx[corner];
        if (std::find(indices, indices + count, vertex) == indices + count) indices[count++] = vertex;
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

void resetAutomaticLod()
{
    showAutomaticLod = false;
    lodSource = NULL;
    lodMeshes.clear();
    lodPointCount = 0;
}

bool generateAutomaticLod(animated_model_t * model, unsigned int reductionPercent)
{
    resetAutomaticLod();
    if (!model || !model->model || model->nbModels == 0) return false;
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
        std::vector<unsigned int> indices;
        std::vector<std::array<float, 3> > centers;
        std::vector<std::array<float, 3> > vertexColors(mesh.nbPoint, {{0.0f, 0.0f, 0.0f}});
        std::vector<unsigned int> colorUses(mesh.nbPoint, 0);
        for (unsigned int faceIndex = 0; faceIndex < mesh.nbPolygon; ++faceIndex)
        {
            polygon_t face = mesh.pltbl[faceIndex];
            for (unsigned int corner = 0; corner < 4; ++corner)
                if (face.vertIdx[corner] >= mesh.nbPoint)
                {
                    resetAutomaticLod();
                    return false;
                }
            unsigned int corners[4];
            unsigned int count = faceCorners(face, corners);
            if (count < 3) continue;
            if (count == 3)
            {
                for (unsigned int corner = 0; corner < 3; ++corner) face.vertIdx[corner] = corners[corner];
                face.vertIdx[3] = face.vertIdx[2];
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
        std::vector<polygon_t> & faces = lodMeshes[meshIndex];
        if (reductionPercent == 0 || indices.empty()) faces = source;
        else
        {
            float extent = MAX(meshopt_simplifyScale(reference[0].point, mesh.nbPoint, sizeof(vertex_t)), 1e-6f);
            unsigned int motionSamples = MIN(model->nbFrames, 9u);
            unsigned int attributeCount = 3 + motionSamples * 3;
            std::vector<float> attributes(size_t(mesh.nbPoint) * attributeCount, 0.0f);
            std::vector<float> weights(attributeCount, 0.3f);
            for (unsigned int attribute = 3; attribute < attributeCount; ++attribute) weights[attribute] = 0.5f / extent;
            for (unsigned int vertex = 0; vertex < mesh.nbPoint; ++vertex)
            {
                for (unsigned int channel = 0; channel < 3; ++channel)
                    attributes[size_t(vertex) * attributeCount + channel] = colorUses[vertex] ? vertexColors[vertex][channel] / colorUses[vertex] : 0.0f;
                for (unsigned int sample = 0; sample < motionSamples; ++sample)
                {
                    unsigned int frame = sample * model->nbFrames / motionSamples;
                    for (unsigned int axis = 0; axis < 3; ++axis)
                        attributes[size_t(vertex) * attributeCount + 3 + sample * 3 + axis] =
                            model->keyFrames[frame].cVert[offset + vertex].point[axis] - reference[vertex].point[axis];
                }
            }
            size_t targetFaces = MAX(size_t(4), (source.size() * (100u - reductionPercent) + 99u) / 100u);
            size_t targetIndices = MIN(indices.size(), targetFaces * 3);
            std::vector<unsigned int> simplified(indices.size());
            float minimumAreaSquared = MAX(1e-20f, extent * extent * extent * extent * 1e-14f);
            while (true)
            {
                size_t count = meshopt_simplifyWithAttributes(simplified.data(), indices.data(), indices.size(),
                    reference[0].point, mesh.nbPoint, sizeof(vertex_t), attributes.data(), attributeCount * sizeof(float),
                    weights.data(), attributeCount, NULL, targetIndices, std::numeric_limits<float>::max(),
                    meshopt_SimplifyPermissive | meshopt_SimplifyRegularizeLight, NULL);
                simplified.resize(count);
                if (!simplified.empty() && validAnimation(simplified, model, offset, mesh, minimumAreaSquared)) break;
                if (targetIndices == indices.size())
                {
                    simplified.clear();
                    break;
                }
                targetIndices = MIN(indices.size(), MAX(targetIndices + MAX(size_t(3), indices.size() / 30 * 3), count + 3));
                simplified.resize(indices.size());
            }
            if (simplified.empty() || simplified.size() / 3 >= source.size()) faces = source;
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
                        float distance = 0.0f;
                        for (unsigned int axis = 0; axis < 3; ++axis)
                        {
                            float delta = center[axis] - centers[sourceFace][axis];
                            distance += delta * delta;
                        }
                        if (distance < nearestDistance)
                        {
                            nearest = sourceFace;
                            nearestDistance = distance;
                        }
                    }
                    polygon_t face = source[nearest];
                    for (unsigned int corner = 0; corner < 3; ++corner) face.vertIdx[corner] = simplified[triangle + corner];
                    face.vertIdx[3] = face.vertIdx[2];
                    faces.push_back(face);
                }
        }
        std::vector<bool> used(mesh.nbPoint, false);
        for (const polygon_t & face : faces)
            for (unsigned int corner = 0; corner < 4; ++corner) used[face.vertIdx[corner]] = true;
        lodPointCount += std::count(used.begin(), used.end(), true);
        offset += mesh.nbPoint;
    }
    lodSource = model;
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