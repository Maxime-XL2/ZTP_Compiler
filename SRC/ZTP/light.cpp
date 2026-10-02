#include "../COMMON.H"
#include <vector>
#include <limits>

bool enableSelfShadowing = false;

namespace
{
std::vector<std::vector<float> > shadowFactors;
bool shadowCacheDirty = true;
animated_model_t * shadowCacheModel = NULL;
animated_model_t * lightAnchorModel = NULL;
float lightCenter[3] = {};
float lightDistance = 1.0f;
float lightMarkerRadius = 0.04f;

struct PreviewColor
{
    float target[3];
    float current[3];
    float visibility;
};

std::vector<std::vector<PreviewColor> > previewColors;
animated_model_t * previewColorModel = NULL;

float interpolateColor(float previous, float target)
{
    float blended = (previous + target) * 0.5f;
    return std::fabs(blended - target) < 0.0001f ? target : blended;
}

bool rayIntersectsTriangle(const float * origin, const float * direction, const float * first,
                           const float * second, const float * third, float minimumDistance)
{
    double edgeOne[3], edgeTwo[3], offset[3];
    for (int axis = 0; axis < XYZ; ++axis)
    {
        edgeOne[axis] = second[axis] - first[axis];
        edgeTwo[axis] = third[axis] - first[axis];
        offset[axis] = origin[axis] - first[axis];
    }
    double crossDirection[3] = {
        direction[Y] * edgeTwo[Z] - direction[Z] * edgeTwo[Y],
        direction[Z] * edgeTwo[X] - direction[X] * edgeTwo[Z],
        direction[X] * edgeTwo[Y] - direction[Y] * edgeTwo[X]
    };
    double determinant = edgeOne[X] * crossDirection[X] + edgeOne[Y] * crossDirection[Y] +
                         edgeOne[Z] * crossDirection[Z];
    double scale = std::sqrt((edgeOne[X]*edgeOne[X] + edgeOne[Y]*edgeOne[Y] + edgeOne[Z]*edgeOne[Z]) *
                             (edgeTwo[X]*edgeTwo[X] + edgeTwo[Y]*edgeTwo[Y] + edgeTwo[Z]*edgeTwo[Z]));
    if (scale == 0.0 || std::fabs(determinant) <= 1e-8 * scale) return false;
    double inverse = 1.0 / determinant;
    double barycentricOne = (offset[X]*crossDirection[X] + offset[Y]*crossDirection[Y] +
                             offset[Z]*crossDirection[Z]) * inverse;
    if (barycentricOne < -1e-7 || barycentricOne > 1.0 + 1e-7) return false;
    double crossOffset[3] = {
        offset[Y]*edgeOne[Z] - offset[Z]*edgeOne[Y],
        offset[Z]*edgeOne[X] - offset[X]*edgeOne[Z],
        offset[X]*edgeOne[Y] - offset[Y]*edgeOne[X]
    };
    double barycentricTwo = (direction[X]*crossOffset[X] + direction[Y]*crossOffset[Y] +
                             direction[Z]*crossOffset[Z]) * inverse;
    if (barycentricTwo < -1e-7 || barycentricOne + barycentricTwo > 1.0 + 1e-7) return false;
    double distance = (edgeTwo[X]*crossOffset[X] + edgeTwo[Y]*crossOffset[Y] +
                       edgeTwo[Z]*crossOffset[Z]) * inverse;
    return distance > minimumDistance;
}

struct ShadowBounds
{
    float minimum[3];
    float maximum[3];
};

bool rayIntersectsBounds(const float * origin, const float * direction, const ShadowBounds & bounds)
{
    float nearDistance = 0.0f;
    float farDistance = std::numeric_limits<float>::max();
    for (int axis = 0; axis < XYZ; ++axis)
    {
        if (std::fabs(direction[axis]) < 1e-8f)
        {
            if (origin[axis] < bounds.minimum[axis] || origin[axis] > bounds.maximum[axis]) return false;
            continue;
        }
        float first = (bounds.minimum[axis] - origin[axis]) / direction[axis];
        float second = (bounds.maximum[axis] - origin[axis]) / direction[axis];
        if (first > second) std::swap(first, second);
        nearDistance = MAX(nearDistance, first);
        farDistance = MIN(farDistance, second);
        if (nearDistance > farDistance) return false;
    }
    return true;
}
}

bool rayIntersectsPolygon(const float * origin, const float * direction, const model_t * mesh,
                          const polygon_t * polygon, float minimumDistance)
{
    if (!origin || !direction || !mesh || !mesh->pntbl || !polygon) return false;
    for (int corner = 0; corner < 4; ++corner)
        if (polygon->vertIdx[corner] >= mesh->nbPoint) return false;
    const float * first = mesh->pntbl[polygon->vertIdx[0]].point;
    const float * second = mesh->pntbl[polygon->vertIdx[1]].point;
    const float * third = mesh->pntbl[polygon->vertIdx[2]].point;
    const float * fourth = mesh->pntbl[polygon->vertIdx[3]].point;
    return rayIntersectsTriangle(origin, direction, first, second, third, minimumDistance) ||
           rayIntersectsTriangle(origin, direction, first, third, fourth, minimumDistance);
}

void resetSelfShadowing()
{
    shadowCacheDirty = true;
    shadowCacheModel = NULL;
    shadowFactors.clear();
}


float defLight[3]={0, 0, -1.0f};

void initializeLightSource(animated_model_t * model)
{
    lightAnchorModel = NULL;
    if (!model) return;
    float minimum[3] = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    float maximum[3] = {-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
    bool hasVertices = false;
    for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
        for (unsigned int vertexIndex = 0; vertexIndex < model->model[meshIndex].nbPoint; ++vertexIndex)
        {
            hasVertices = true;
            for (int axis = 0; axis < XYZ; ++axis)
            {
                float coordinate = model->model[meshIndex].pntbl[vertexIndex].point[axis];
                minimum[axis] = MIN(minimum[axis], coordinate);
                maximum[axis] = MAX(maximum[axis], coordinate);
            }
        }
    if (!hasVertices) return;
    float diagonalSquared = 0.0f;
    for (int axis = 0; axis < XYZ; ++axis)
    {
        lightCenter[axis] = (minimum[axis] + maximum[axis]) * 0.5f;
        float extent = maximum[axis] - minimum[axis];
        diagonalSquared += extent * extent;
    }
    float diagonal = MAX(std::sqrt(diagonalSquared), 1.0f);
    lightDistance = diagonal * 4.0f;
    lightMarkerRadius = diagonal * 0.08f;
    lightAnchorModel = model;
    resetSelfShadowing();
    resetPreviewColors();
}

bool getLightSourcePosition(animated_model_t * model, float * position, float * markerRadius)
{
    if (!model || !position || !markerRadius) return false;
    if (lightAnchorModel != model) initializeLightSource(model);
    if (lightAnchorModel != model) return false;
    for (int axis = 0; axis < XYZ; ++axis) position[axis] = lightCenter[axis] - defLight[axis] * lightDistance;
    *markerRadius = lightMarkerRadius;
    return true;
}

void setLightOrientation(float azimuthDegrees, float elevationDegrees)
{
    extern float lightDir[3];
    float azimuth = azimuthDegrees * conv;
    float elevation = elevationDegrees * conv;
    defLight[X] = -std::sin(azimuth) * std::cos(elevation);
    defLight[Y] = std::sin(elevation);
    defLight[Z] = -std::cos(azimuth) * std::cos(elevation);
    for (int axis = 0; axis < XYZ; ++axis) lightDir[axis] = defLight[axis];
    shadowCacheDirty = true;
}

void updateSelfShadowing(animated_model_t * model, bool geometryChanged)
{
    if (geometryChanged) shadowCacheDirty = true;
    if (!enableSelfShadowing || !model) return;
    if (!shadowCacheDirty && shadowCacheModel == model) return;

    shadowFactors.clear();
    shadowFactors.resize(model->nbModels);
    std::vector<std::vector<ShadowBounds> > bounds(model->nbModels);
    float maximumExtent = 0.0f;
    for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
    {
        const model_t & mesh = model->model[meshIndex];
        shadowFactors[meshIndex].assign(mesh.nbPoint, 1.0f);
        bounds[meshIndex].resize(mesh.nbPolygon);
        for (unsigned int faceIndex = 0; faceIndex < mesh.nbPolygon; ++faceIndex)
        {
            ShadowBounds & faceBounds = bounds[meshIndex][faceIndex];
            for (int axis = 0; axis < XYZ; ++axis)
            {
                faceBounds.minimum[axis] = std::numeric_limits<float>::max();
                faceBounds.maximum[axis] = -std::numeric_limits<float>::max();
                for (int corner = 0; corner < 4; ++corner)
                {
                    float coordinate = mesh.pntbl[mesh.pltbl[faceIndex].vertIdx[corner]].point[axis];
                    faceBounds.minimum[axis] = MIN(faceBounds.minimum[axis], coordinate);
                    faceBounds.maximum[axis] = MAX(faceBounds.maximum[axis], coordinate);
                }
                maximumExtent = MAX(maximumExtent, faceBounds.maximum[axis] - faceBounds.minimum[axis]);
            }
        }
    }
    float bias = MAX(maximumExtent * 1e-5f, 1e-6f);
    float towardLight[3] = {-defLight[X], -defLight[Y], -defLight[Z]};
    for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
    {
        const model_t & mesh = model->model[meshIndex];
        for (unsigned int vertexIndex = 0; vertexIndex < mesh.nbPoint; ++vertexIndex)
        {
            const vertex_t & vertex = mesh.pntbl[vertexIndex];
            float intensity = vertex.normal[X]*towardLight[X] + vertex.normal[Y]*towardLight[Y] +
                              vertex.normal[Z]*towardLight[Z];
            if (intensity <= 0.0f)
            {
                shadowFactors[meshIndex][vertexIndex] = 0.0f;
                continue;
            }
            float origin[3];
            for (int axis = 0; axis < XYZ; ++axis) origin[axis] = vertex.point[axis] + towardLight[axis]*bias;

            bool blocked = false;
            for (unsigned int otherMesh = 0; otherMesh < model->nbModels && !blocked; ++otherMesh)
            {
                const model_t & occluder = model->model[otherMesh];
                for (unsigned int otherFace = 0; otherFace < occluder.nbPolygon; ++otherFace)
                {
                    if (otherMesh == meshIndex)
                    {
                        bool incident = false;
                        for (int corner = 0; corner < 4; ++corner)
                            if (occluder.pltbl[otherFace].vertIdx[corner] == vertexIndex) incident = true;
                        if (incident) continue;
                    }
                    if (!rayIntersectsBounds(origin, towardLight, bounds[otherMesh][otherFace])) continue;
                    if (rayIntersectsPolygon(origin, towardLight, &occluder, &occluder.pltbl[otherFace], bias))
                    {
                        blocked = true;
                        break;
                    }
                }
            }
            if (blocked) shadowFactors[meshIndex][vertexIndex] = 0.0f;
        }
    }
    shadowCacheModel = model;
    shadowCacheDirty = false;
}

float selfShadowFactor(unsigned int modelIndex, unsigned int vertexIndex)
{
    if (!enableSelfShadowing || modelIndex >= shadowFactors.size() || vertexIndex >= shadowFactors[modelIndex].size())
        return 1.0f;
    return shadowFactors[modelIndex][vertexIndex];
}

void resetPreviewColors()
{
    previewColors.clear();
    previewColorModel = NULL;
}

void updatePreviewColors(animated_model_t * model)
{
    if (!model) return;
    if (previewColorModel != model || previewColors.size() != model->nbModels)
    {
        resetPreviewColors();
        previewColors.resize(model->nbModels);
        previewColorModel = model;
    }
    for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
    {
        const model_t & mesh = model->model[meshIndex];
        bool initialize = previewColors[meshIndex].size() != mesh.nbPoint;
        if (initialize) previewColors[meshIndex].resize(mesh.nbPoint);
        for (unsigned int vertexIndex = 0; vertexIndex < mesh.nbPoint; ++vertexIndex)
        {
            PreviewColor & color = previewColors[meshIndex][vertexIndex];
            float visibility = selfShadowFactor(meshIndex, vertexIndex);
            color.visibility = initialize ? visibility : interpolateColor(color.visibility, visibility);
            for (int channel = 0; channel < 3; ++channel)
            {
                color.target[channel] = MIN(1.0f, MAX(0.0f,
                    (mesh.pntbl[vertexIndex].color[channel] * 0.5f + 0.5f) * visibility));
                color.current[channel] = initialize ? color.target[channel] :
                    interpolateColor(color.current[channel], color.target[channel]);
            }
        }
    }
}

const float * previewVertexColor(unsigned int modelIndex, unsigned int vertexIndex)
{
    if (modelIndex >= previewColors.size() || vertexIndex >= previewColors[modelIndex].size()) return NULL;
    return previewColors[modelIndex][vertexIndex].current;
}

float previewVertexVisibility(unsigned int modelIndex, unsigned int vertexIndex)
{
    if (modelIndex >= previewColors.size() || vertexIndex >= previewColors[modelIndex].size()) return 1.0f;
    return previewColors[modelIndex][vertexIndex].visibility;
}

void calcLight(animated_model_t * aModel)
{
    for (uint32_t i=0; i<aModel->nbModels; i++ )
    {
       model_t * mod =  &aModel->model[i];
       for (uint32_t ii=0; ii<mod->nbPoint; ii++)
       {
            vertex_t * v = &mod->pntbl[ii];
            float d = -dotProduct((float*)v->normal, (float*)defLight);
            v->color[0]=d;
            v->color[1]=d;
            v->color[2]=d;

       }
    }
}
