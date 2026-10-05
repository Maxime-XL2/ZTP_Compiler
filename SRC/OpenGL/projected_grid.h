#ifndef ZTP_PROJECTED_GRID_H
#define ZTP_PROJECTED_GRID_H

#include <algorithm>
#include <cmath>

namespace projected_grid
{
struct Vertex
{
    double position[3];
    float uv[2];
    float color[4];
};

inline bool project(const float * point, const double * modelView, const double * projection, double * result)
{
    double eye[4] = {}, clip[4] = {};
    for (unsigned int row = 0; row < 4; ++row)
    {
        eye[row] = modelView[12 + row];
        for (unsigned int axis = 0; axis < 3; ++axis)
            eye[row] += modelView[axis * 4 + row] * point[axis];
    }
    for (unsigned int row = 0; row < 4; ++row)
    {
        for (unsigned int axis = 0; axis < 4; ++axis)
            clip[row] += projection[axis * 4 + row] * eye[axis];
        if (!std::isfinite(clip[row])) return false;
    }
    if (clip[3] <= 0.000001 || clip[2] < -clip[3]) return false;
    for (unsigned int axis = 0; axis < 3; ++axis)
    {
        result[axis] = clip[axis] / clip[3];
        if (!std::isfinite(result[axis])) return false;
    }
    return true;
}

inline Vertex interpolate(const Vertex * corners, float horizontal, float vertical)
{
    const float weights[4] = {
        (1 - horizontal) * (1 - vertical), horizontal * (1 - vertical),
        horizontal * vertical, (1 - horizontal) * vertical
    };
    Vertex result = {};
    for (unsigned int corner = 0; corner < 4; ++corner)
    {
        for (unsigned int axis = 0; axis < 3; ++axis)
            result.position[axis] += corners[corner].position[axis] * weights[corner];
        for (unsigned int axis = 0; axis < 2; ++axis)
            result.uv[axis] += corners[corner].uv[axis] * weights[corner];
        for (unsigned int channel = 0; channel < 4; ++channel)
            result.color[channel] += corners[corner].color[channel] * weights[corner];
    }
    return result;
}

inline unsigned int subdivisions(const Vertex * corners, int width, int height)
{
    const double horizontal = (corners[0].position[0] - corners[1].position[0] +
        corners[2].position[0] - corners[3].position[0]) * width * 0.5;
    const double vertical = (corners[0].position[1] - corners[1].position[1] +
        corners[2].position[1] - corners[3].position[1]) * height * 0.5;
    double requiredCells = std::hypot(horizontal, vertical) / 2.0;
    for (unsigned int channel = 0; channel < 4; ++channel)
    {
        const double difference = std::fabs(corners[0].color[channel] - corners[1].color[channel] +
            corners[2].color[channel] - corners[3].color[channel]);
        requiredCells = std::max(requiredCells, difference * 255.0 / 4.0);
    }
    return static_cast<unsigned int>(std::max(1.0, std::min(64.0, std::ceil(std::sqrt(requiredCells)))));
}

inline unsigned int columns(const Vertex * corners, int width, int height)
{
    return subdivisions(corners, width, height);
}

inline unsigned int rows(const Vertex * corners, int width, int height)
{
    return subdivisions(corners, width, height);
}
}

#endif