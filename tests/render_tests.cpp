#include "../SRC/OpenGL/projected_grid.h"
#include "../SRC/COMMON.H"
#include <cassert>
#include <iostream>
#include <limits>

void calculateBoundingBox(float * boxMin, float * boxMax, vertex_t * vertexArray, unsigned int totalVertices);
extern bool enableSelfShadowing;
float lightDir[3] = {};

int main()
{
    vertex_t animatedPoints[2] = {};
    animatedPoints[1].point[X] = 10;
    key_frame_t frames[2] = {};
    frames[0].cVert = &animatedPoints[0];
    frames[1].cVert = &animatedPoints[1];
    vertex_t displayedPoint = {};
    model_t mesh = {};
    mesh.nbPoint = 1;
    mesh.pntbl = &displayedPoint;
    animated_model_t animated = {};
    animated.nbModels = 1;
    animated.model = &mesh;
    animated.nbFrames = 2;
    animated.keyFrames = frames;
    animationControl_t control = {};
    control.fps = 2;
    control.endFrm = 2;
    control.currentFrm = 2;
    assert(applyAnimationPose(&animated, control, true));
    assert(displayedPoint.point[X] == 5);
    control.currentFrm = 6;
    assert(applyAnimationPose(&animated, control, true));
    assert(displayedPoint.point[X] == 10);
    assert(applyAnimationPose(&animated, control, false));
    assert(displayedPoint.point[X] == 5);
    control.currentFrm = 100;
    assert(applyAnimationPose(&animated, control, true));
    assert(displayedPoint.point[X] == 10);
    control.currentFrm = 2;
    assert(applyAnimationPose(&animated, control, true, false));
    assert(displayedPoint.point[X] == 0);
    animated.nbFrames = 1;
    assert(applyAnimationPose(&animated, control, true));
    assert(displayedPoint.point[X] == 0);
    animated.nbFrames = 0;
    assert(!applyAnimationPose(&animated, control, true));
    assert(animatedPoints[1].point[X] == 10);

    vertex_t receiver = {};
    receiver.normal[Z] = 1;
    vertex_t occluderPoints[4] = {};
    const float occluderCorners[4][2] = {{-2, -2}, {2, -2}, {2, 2}, {-2, 2}};
    polygon_t occluderFace = {};
    for (unsigned int corner = 0; corner < 4; ++corner)
    {
        occluderPoints[corner].point[X] = occluderCorners[corner][0];
        occluderPoints[corner].point[Y] = occluderCorners[corner][1];
        occluderPoints[corner].point[Z] = 1;
        occluderPoints[corner].normal[Z] = 1;
        occluderFace.vertIdx[corner] = corner;
    }
    model_t shadowMeshes[2] = {};
    shadowMeshes[0].pntbl = &receiver;
    shadowMeshes[0].nbPoint = 1;
    shadowMeshes[1].pntbl = occluderPoints;
    shadowMeshes[1].nbPoint = 4;
    shadowMeshes[1].pltbl = &occluderFace;
    shadowMeshes[1].nbPolygon = 1;
    animated_model_t shadowScene = {};
    shadowScene.model = shadowMeshes;
    shadowScene.nbModels = 1;
    enableSelfShadowing = true;
    resetSelfShadowing();
    updateSelfShadowing(&shadowScene, true);
    assert(selfShadowFactor(0, 0) == 1);
    shadowScene.nbModels = 2;
    updateSelfShadowing(&shadowScene, true);
    assert(selfShadowFactor(0, 0) == 0);
    assert(selfShadowFactor(1, 0) == 1);
    for (vertex_t & point : occluderPoints) point.point[Z] = -1;
    updateSelfShadowing(&shadowScene, true);
    assert(selfShadowFactor(0, 0) == 1);
    shadowScene.nbModels = 1;
    resetSelfShadowing();
    updateSelfShadowing(&shadowScene, false);
    assert(selfShadowFactor(0, 0) == 1);
    enableSelfShadowing = false;

    vertex_t descending[3] = {};
    for (unsigned int vertexIndex = 0; vertexIndex < 3; ++vertexIndex)
        for (unsigned int axis = 0; axis < 3; ++axis)
            descending[vertexIndex].point[axis] = 30.0f - vertexIndex * 10.0f;
    float minimum[3] = {65536, 65536, 65536};
    float maximum[3] = {-65536, -65536, -65536};
    calculateBoundingBox(minimum, maximum, descending, 3);
    for (unsigned int axis = 0; axis < 3; ++axis)
        assert(minimum[axis] == 10 && maximum[axis] == 30);
    vertex_t replacement[1] = {};
    replacement[0].point[1] = -50;
    for (unsigned int axis = 0; axis < 3; ++axis)
    {
        minimum[axis] = 65536;
        maximum[axis] = -65536;
    }
    calculateBoundingBox(minimum, maximum, replacement, 1);
    for (unsigned int axis = 0; axis < 3; ++axis)
        assert(minimum[axis] == replacement[0].point[axis] && maximum[axis] == replacement[0].point[axis]);

    const double identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const double perspective[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -11.0 / 9.0, -1, 0, 0, -20.0 / 9.0, 0};
    const float points[4][3] = {{-1, -1, -2}, {1, -1, -4}, {1, 1, -4}, {-1, 1, -2}};
    projected_grid::Vertex corners[4] = {};
    const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (unsigned int corner = 0; corner < 4; ++corner)
    {
        assert(projected_grid::project(points[corner], identity, perspective, corners[corner].position));
        for (unsigned int axis = 0; axis < 2; ++axis) corners[corner].uv[axis] = uv[corner][axis];
        corners[corner].color[corner] = 1;
        const projected_grid::Vertex sample = projected_grid::interpolate(corners, uv[corner][0], uv[corner][1]);
        for (unsigned int axis = 0; axis < 3; ++axis) assert(sample.position[axis] == corners[corner].position[axis]);
    }
    projected_grid::Vertex center = projected_grid::interpolate(corners, 0.5f, 0.5f);
    assert(std::fabs(center.position[0] + 0.125) < 0.000001);
    assert(center.uv[0] == 0.5f && center.uv[1] == 0.5f);
    for (unsigned int channel = 0; channel < 4; ++channel) assert(center.color[channel] == 0.25f);
    const float worldCenter[3] = {0, 0, -3};
    double projectedCenter[3];
    assert(projected_grid::project(worldCenter, identity, perspective, projectedCenter));
    assert(projectedCenter[0] != center.position[0]);
    assert(projected_grid::columns(corners, 352, 224) >= 1);
    assert(projected_grid::rows(corners, 352, 224) >= 1);
    assert(projected_grid::columns(corners, 100000, 100000) == 64);
    assert(projected_grid::rows(corners, 100000, 100000) == 64);

    projected_grid::Vertex affine[4] = {};
    for (unsigned int corner = 0; corner < 4; ++corner)
    {
        affine[corner].position[0] = uv[corner][0] * 200 - 100;
        affine[corner].position[1] = uv[corner][1] * 200 - 100;
        for (unsigned int channel = 0; channel < 4; ++channel) affine[corner].color[channel] = 1;
    }
    assert(projected_grid::subdivisions(affine, 1920, 1080) == 1);
    affine[2].position[0] += 1;
    const unsigned int detail = projected_grid::subdivisions(affine, 1920, 1080);
    assert(detail > 1 && detail < 64);
    const double maximumPositionError = 1920.0 * 0.5 / (4 * detail * detail);
    assert(maximumPositionError <= 0.5);
    affine[2].position[0] -= 1;
    affine[2].color[0] = 0;
    const unsigned int colorDetail = projected_grid::subdivisions(affine, 1920, 1080);
    assert(colorDetail > 1 && colorDetail < 64);
    assert(1.0 / (4 * colorDetail * colorDetail) <= 1.0 / 255.0);

    for (unsigned int direction = 0; direction < 4; ++direction)
    {
        for (unsigned int corner = 0; corner < 4; ++corner)
        {
            corners[corner].uv[0] = direction & 1 ? 1 - uv[corner][0] : uv[corner][0];
            corners[corner].uv[1] = direction & 2 ? 1 - uv[corner][1] : uv[corner][1];
        }
        const projected_grid::Vertex sample = projected_grid::interpolate(corners, 0.25f, 0.75f);
        assert(sample.uv[0] == (direction & 1 ? 0.75f : 0.25f));
        assert(sample.uv[1] == (direction & 2 ? 0.25f : 0.75f));
    }
    for (unsigned int collapsedEdge = 0; collapsedEdge < 4; ++collapsedEdge)
    {
        projected_grid::Vertex triangle[4];
        for (unsigned int corner = 0; corner < 4; ++corner) triangle[corner] = corners[corner];
        const unsigned int duplicate = (collapsedEdge + 1) & 3;
        for (unsigned int axis = 0; axis < 3; ++axis)
            triangle[duplicate].position[axis] = triangle[collapsedEdge].position[axis];
        for (unsigned int step = 0; step <= 16; ++step)
        {
            const float fraction = step / 16.0f;
            const float horizontal = collapsedEdge == 0 ? fraction : collapsedEdge == 1 ? 1 : collapsedEdge == 2 ? 1 - fraction : 0;
            const float vertical = collapsedEdge == 0 ? 0 : collapsedEdge == 1 ? fraction : collapsedEdge == 2 ? 1 : 1 - fraction;
            const projected_grid::Vertex sample = projected_grid::interpolate(triangle, horizontal, vertical);
            for (unsigned int axis = 0; axis < 3; ++axis)
                assert(sample.position[axis] == triangle[collapsedEdge].position[axis]);
        }
    }
    projected_grid::Vertex collapsed[4] = {};
    assert(projected_grid::columns(collapsed, 352, 224) == 1);
    assert(projected_grid::rows(collapsed, 352, 224) == 1);
    const float behind[3] = {0, 0, 1}, nearPoint[3] = {0, 0, -0.5f}, eye[3] = {0, 0, 0};
    assert(!projected_grid::project(behind, identity, perspective, projectedCenter));
    assert(!projected_grid::project(nearPoint, identity, perspective, projectedCenter));
    assert(!projected_grid::project(eye, identity, perspective, projectedCenter));
    const float invalid[3] = {std::numeric_limits<float>::infinity(), 0, -2};
    assert(!projected_grid::project(invalid, identity, perspective, projectedCenter));
    double translated[16];
    for (unsigned int entry = 0; entry < 16; ++entry) translated[entry] = identity[entry];
    translated[12] = 2;
    assert(projected_grid::project(points[0], translated, perspective, projectedCenter));
    assert(projectedCenter[0] == 0.5);
    std::cout << "Projected grid tests passed\n";
}