#include "../COMMON.H"
#include "../../third_party/ufbx/ufbx.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <climits>
#include <limits>
#include <memory>
#include <map>
#include <set>
#include <vector>

string fbxLoadError;

extern void rotateModelData(animated_model_t *, int, int, int);
extern void translateModelData(animated_model_t *, float, float, float);
extern void findScale(animated_model_t *, bool);
extern void recalculateNormals(model_t *);
extern void pointNormals(model_t *);
extern void calcLight(animated_model_t *);
extern uint32_t useUV;
extern int ReadTGAFile(string, texture_t *, string);
extern void createFakeTexture(texture_t *);
extern void createTexture(polygon_t *, animated_model_t *, const texture_t &);
extern void cacheAutomaticLodSourceTextures(animated_model_t *, const texture_t *, unsigned int);
extern void specialConditions(unsigned short, unsigned short, animated_model_t *);

namespace
{
struct BonePreviewPoint
{
    float source[3];
    uint16_t cluster;
};

struct BonePreviewData
{
    const animated_model_t * owner = NULL;
    bool enabled = false;
    unsigned int frameCount = 0;
    unsigned int clusterCount = 0;
    std::vector<BonePreviewPoint> points;
    std::vector<unsigned int> activePointRemap;
    std::vector<std::array<float, 12> > matrices;
    float importScale[3] = {1.0f, 1.0f, 1.0f};
    float importTranslation[3] = {};
    float inverseModelScale[3] = {1.0f, 1.0f, 1.0f};
    int importRotation[3] = {};
};

struct FbxSkeletonNode
{
    string name;
    int parentIndex = -1;
    bool isBone = false;
    size_t sourceNodeIndex = 0;
    std::array<double, 12> sourceLocalMatrix;
    std::array<int32_t, 12> restMatrix;
};

struct FbxSkeletonData
{
    const animated_model_t * owner = NULL;
    unsigned int boneCount = 0;
    unsigned int frameCount = 0;
    unsigned int frameRate = 0;
    bool transformsSupported = true;
    float importScale[3] = {1.0f, 1.0f, 1.0f};
    float importTranslation[3] = {};
    int importRotation[3] = {};
    std::vector<FbxSkeletonNode> nodes;
    std::vector<std::array<int32_t, 12> > animationMatrices;
};

std::vector<BonePreviewData> bonePreviewData;
std::vector<FbxSkeletonData> fbxSkeletonData;

BonePreviewData * findBonePreview(animated_model_t * model)
{
    for (BonePreviewData & data : bonePreviewData)
        if (data.owner == model) return &data;
    return NULL;
}

const BonePreviewData * findBonePreview(const animated_model_t * model)
{
    for (const BonePreviewData & data : bonePreviewData)
        if (data.owner == model) return &data;
    return NULL;
}

const FbxSkeletonData * findFbxSkeleton(const animated_model_t * model)
{
    for (const FbxSkeletonData & data : fbxSkeletonData)
        if (data.owner == model) return &data;
    return NULL;
}

void discardFbxSkeletonData(const animated_model_t * model)
{
    fbxSkeletonData.erase(std::remove_if(fbxSkeletonData.begin(), fbxSkeletonData.end(),
        [&](const FbxSkeletonData & data) { return data.owner == model; }), fbxSkeletonData.end());
}

void rotatePreviewPoint(float point[3], int axis, int turns)
{
    turns = ((turns % 4) + 4) % 4;
    for (int turn = 0; turn < turns; ++turn)
    {
        if (axis == X)
        {
            const float y = point[Y];
            point[Y] = -point[Z];
            point[Z] = y;
        }
        else if (axis == Y)
        {
            const float x = point[X];
            point[X] = point[Z];
            point[Z] = -x;
        }
        else
        {
            const float x = point[X];
            point[X] = -point[Y];
            point[Y] = x;
        }
    }
}

void discardBonePreviewData(const animated_model_t * model)
{
    bonePreviewData.erase(std::remove_if(bonePreviewData.begin(), bonePreviewData.end(),
        [&](const BonePreviewData & data) { return data.owner == model; }), bonePreviewData.end());
}
}

bool hasFbxBonePreview(const animated_model_t * model)
{
    const BonePreviewData * data = findBonePreview(model);
    return data && data->clusterCount && data->frameCount;
}

bool hasFbxSkeleton(const animated_model_t * model)
{
    const FbxSkeletonData * data = findFbxSkeleton(model);
    return data && data->boneCount && !data->nodes.empty();
}

void discardFbxSkeleton(const animated_model_t * model)
{
    discardFbxSkeletonData(model);
}

namespace
{
bool fixed16_16(double value, int32_t * result)
{
    if (!std::isfinite(value)) return false;
    const double scaled = std::round(value * 65536.0);
    if (scaled < static_cast<double>(std::numeric_limits<int32_t>::min()) ||
        scaled > static_cast<double>(std::numeric_limits<int32_t>::max())) return false;
    *result = static_cast<int32_t>(scaled);
    return true;
}

bool exportMatrix(const FbxSkeletonData & data, const FbxSkeletonNode & node,
                  const double sourceMatrix[12], int32_t matrix[12])
{
    for (size_t index = 0; index < 12; ++index)
    {
        double value = sourceMatrix[index];
        if (node.parentIndex < 0)
        {
            const unsigned int column = static_cast<unsigned int>(index / 3);
            const unsigned int axis = static_cast<unsigned int>(index % 3);
            double point[3] = {
                sourceMatrix[column * 3],
                sourceMatrix[column * 3 + 1],
                sourceMatrix[column * 3 + 2]
            };
            for (unsigned int component = 0; component < 3; ++component)
                point[component] *= data.importScale[component];
            for (int component = X; component <= Z; ++component)
            {
                int turns = ((data.importRotation[component] % 4) + 4) % 4;
                for (int turn = 0; turn < turns; ++turn)
                {
                    if (component == X)
                    {
                        const double y = point[Y];
                        point[Y] = -point[Z];
                        point[Z] = y;
                    }
                    else if (component == Y)
                    {
                        const double x = point[X];
                        point[X] = point[Z];
                        point[Z] = -x;
                    }
                    else
                    {
                        const double x = point[X];
                        point[X] = -point[Y];
                        point[Y] = x;
                    }
                }
            }
            value = point[axis];
            if (column == 3) value += data.importTranslation[axis];
        }
        if (!fixed16_16(value, &matrix[index])) return false;
    }
    return true;
}

string skeletonNodeName(const FbxSkeletonNode & node, size_t index)
{
    return node.name.empty() ? "node_" + std::to_string(index) : node.name;
}

struct SaturnSkeletonPose
{
    int8_t pos[3];
    int8_t rot[3];
    int8_t scale[3];
};

bool computeTranslationScaleQ16_16(const FbxSkeletonData & data,
                                   uint32_t * scaleQ16_16, string * error)
{
    double maximumPositive = 0.0;
    double minimumNegative = 0.0;
    bool requiresScale = false;
    const auto inspect = [&](const std::array<int32_t, 12> & matrix)
    {
        for (unsigned int axis = 0; axis < 3; ++axis)
        {
            const double value = static_cast<double>(matrix[9 + axis]) / 65536.0;
            maximumPositive = std::max(maximumPositive, value);
            minimumNegative = std::min(minimumNegative, value);
            const double rounded = std::round(value);
            if (rounded < INT8_MIN || rounded > INT8_MAX)
                requiresScale = true;
        }
    };
    for (const FbxSkeletonNode & node : data.nodes)
        inspect(node.restMatrix);
    for (const std::array<int32_t, 12> & matrix : data.animationMatrices)
        inspect(matrix);

    double requestedScale = 1.0;
    if (requiresScale)
        requestedScale = std::max(maximumPositive / INT8_MAX,
                                  -minimumNegative / -static_cast<double>(INT8_MIN));
    const double encodedScale = std::ceil(requestedScale * 65536.0);
    if (!std::isfinite(encodedScale) || encodedScale < 65536.0 ||
        encodedScale > static_cast<double>(UINT32_MAX))
    {
        *error = "The global translation scale cannot be represented as unsigned Q16.16.";
        return false;
    }
    *scaleQ16_16 = static_cast<uint32_t>(encodedScale);

    const double actualScale = static_cast<double>(*scaleQ16_16) / 65536.0;
    const auto fits = [&](const std::array<int32_t, 12> & matrix)
    {
        for (unsigned int axis = 0; axis < 3; ++axis)
        {
            const double value = static_cast<double>(matrix[9 + axis]) / 65536.0;
            const double rounded = std::round(value / actualScale);
            if (rounded < INT8_MIN || rounded > INT8_MAX) return false;
        }
        return true;
    };
    for (const FbxSkeletonNode & node : data.nodes)
        if (!fits(node.restMatrix))
        {
            *error = "Could not fit a rest translation into the selected 8-bit global scale.";
            return false;
        }
    for (const std::array<int32_t, 12> & matrix : data.animationMatrices)
        if (!fits(matrix))
        {
            *error = "Could not fit an animation translation into the selected 8-bit global scale.";
            return false;
        }
    return true;
}

bool matrixToSaturnPose(const std::array<int32_t, 12> & matrix,
                        uint32_t translationScaleQ16_16,
                        SaturnSkeletonPose * pose, string * error)
{
    const double translationScale = static_cast<double>(translationScaleQ16_16) / 65536.0;
    for (unsigned int axis = 0; axis < 3; ++axis)
    {
        const double translated = std::round(
            (static_cast<double>(matrix[9 + axis]) / 65536.0) / translationScale);
        if (translated < INT8_MIN || translated > INT8_MAX)
        {
            *error = "A skeleton translation exceeds the signed 8-bit range after global scaling.";
            return false;
        }
        pose->pos[axis] = static_cast<int8_t>(translated);
    }

    double rotation[3][3];
    double scale[3];
    for (unsigned int column = 0; column < 3; ++column)
    {
        double lengthSquared = 0.0;
        for (unsigned int row = 0; row < 3; ++row)
        {
            const double value = static_cast<double>(matrix[column * 3 + row]) / 65536.0;
            lengthSquared += value * value;
        }
        scale[column] = std::sqrt(lengthSquared);
        if (!std::isfinite(scale[column]) || scale[column] < 1.0e-8)
        {
            *error = "A skeleton transform has a zero or invalid scale and cannot be exported as TRS.";
            return false;
        }
        for (unsigned int row = 0; row < 3; ++row)
            rotation[row][column] =
                (static_cast<double>(matrix[column * 3 + row]) / 65536.0) / scale[column];
    }

    for (unsigned int first = 0; first < 3; ++first)
        for (unsigned int second = first + 1; second < 3; ++second)
        {
            double dot = 0.0;
            for (unsigned int row = 0; row < 3; ++row)
                dot += rotation[row][first] * rotation[row][second];
            if (std::fabs(dot) > 1.0e-3)
            {
                *error = "A skeleton transform contains shear and cannot be exported as TRS.";
                return false;
            }
        }

    const double determinant =
        rotation[0][0] * (rotation[1][1] * rotation[2][2] - rotation[1][2] * rotation[2][1]) -
        rotation[0][1] * (rotation[1][0] * rotation[2][2] - rotation[1][2] * rotation[2][0]) +
        rotation[0][2] * (rotation[1][0] * rotation[2][1] - rotation[1][1] * rotation[2][0]);
    if (!std::isfinite(determinant) || std::fabs(std::fabs(determinant) - 1.0) > 1.0e-3)
    {
        *error = "A skeleton transform is not a valid rotation with scale.";
        return false;
    }
    if (determinant < 0.0)
    {
        scale[0] = -scale[0];
        for (unsigned int row = 0; row < 3; ++row)
            rotation[row][0] = -rotation[row][0];
    }

    const double sineY = std::max(-1.0, std::min(1.0, -rotation[2][0]));
    const double euler[3] = {
        0.0,
        std::asin(sineY),
        0.0
    };
    double angles[3] = { euler[0], euler[1], euler[2] };
    if (std::fabs(std::cos(angles[1])) > 1.0e-6)
    {
        angles[0] = std::atan2(rotation[2][1], rotation[2][2]);
        angles[2] = std::atan2(rotation[1][0], rotation[0][0]);
    }
    else
    {
        angles[0] = std::atan2(-rotation[1][2], rotation[1][1]);
    }
    const double radiansPerTurn = 2.0 * std::acos(-1.0);
    for (unsigned int axis = 0; axis < 3; ++axis)
    {
        long long encoded = std::llround(angles[axis] * 256.0 / radiansPerTurn);
        encoded = ((encoded + 128LL) % 256LL + 256LL) % 256LL - 128LL;
        pose->rot[axis] = static_cast<int8_t>(encoded);
    }

    for (unsigned int axis = 0; axis < 3; ++axis)
    {
        const double encoded = std::round(scale[axis] * 64.0);
        if (encoded < INT8_MIN || encoded > INT8_MAX)
        {
            *error = "A skeleton scale exceeds the signed Q2.6 range.";
            return false;
        }
        pose->scale[axis] = static_cast<int8_t>(encoded);
    }
    return true;
}

enum class SaturnPoseChannel
{
    Position,
    Rotation,
    Scale
};

int poseChannelValue(const SaturnSkeletonPose & pose, SaturnPoseChannel channel,
                     unsigned int component)
{
    if (channel == SaturnPoseChannel::Position)
        return pose.pos[component];
    if (channel == SaturnPoseChannel::Rotation)
        return pose.rot[component];
    return pose.scale[component];
}

void writeRestChannelInitializer(std::ostream & output,
                                 const std::vector<SaturnSkeletonPose> & poses,
                                 SaturnPoseChannel channel)
{
    output << "{\n";
    for (size_t node = 0; node < poses.size(); ++node)
    {
        output << "    { ";
        for (unsigned int component = 0; component < 3; ++component)
        {
            if (component) output << ", ";
            output << poseChannelValue(poses[node], channel, component);
        }
        output << " },\n";
    }
    output << "}";
}

void writeAnimationPoseInitializer(std::ostream & output,
                                   const std::vector<SaturnSkeletonPose> & poses,
                                   size_t nodeCount, size_t frameCount)
{
    output << "{\n";
    for (size_t frame = 0; frame < frameCount; ++frame)
    {
        output << "    {\n";
        for (size_t node = 0; node < nodeCount; ++node)
        {
            const SaturnSkeletonPose & pose = poses[frame * nodeCount + node];
            output << "        { { " << static_cast<int>(pose.rot[0]) << ", "
                   << static_cast<int>(pose.rot[1]) << ", "
                   << static_cast<int>(pose.rot[2]) << " }, { "
                   << static_cast<int>(pose.pos[0]) << ", "
                   << static_cast<int>(pose.pos[1]) << ", "
                   << static_cast<int>(pose.pos[2]) << " } },\n";
        }
        output << "    },\n";
    }
    output << "}";
}

void writeByteArrayInitializer(std::ostream & output,
                               const std::vector<uint8_t> & values)
{
    output << "{\n    ";
    for (size_t index = 0; index < values.size(); ++index)
    {
        output << static_cast<unsigned int>(values[index]) << ", ";
        if ((index + 1) % 16 == 0) output << "\n    ";
    }
    output << "\n}";
}
}

bool exportFbxSkeletonC(const animated_model_t * model, const string & path,
                        unsigned int * boneCount, unsigned int * nodeCount,
                        unsigned int * frameCount, unsigned long long * dataSizeBytes,
                        uint32_t * translationScaleQ16_16)
{
    fbxLoadError.clear();
    if (boneCount) *boneCount = 0;
    if (nodeCount) *nodeCount = 0;
    if (frameCount) *frameCount = 0;
    if (dataSizeBytes) *dataSizeBytes = 0;
    if (translationScaleQ16_16) *translationScaleQ16_16 = 0;
    const FbxSkeletonData * data = findFbxSkeleton(model);
    if (!data || !data->boneCount || data->nodes.empty() ||
        !data->frameCount || !boneCount || !nodeCount || !frameCount || !dataSizeBytes ||
        !translationScaleQ16_16) return false;
    if (data->nodes.size() > UINT16_MAX)
    {
        fbxLoadError = "The skeleton has too many hierarchy nodes for 16-bit parent indices.";
        return false;
    }
    if (!data->transformsSupported ||
        data->animationMatrices.size() != size_t(data->frameCount) * data->nodes.size())
    {
        fbxLoadError = "A skeleton animation transform is outside the signed Q16.16 range.";
        return false;
    }

    uint32_t selectedTranslationScaleQ16_16 = 0;
    string poseError;
    if (!computeTranslationScaleQ16_16(*data, &selectedTranslationScaleQ16_16, &poseError))
    {
        fbxLoadError = poseError;
        return false;
    }
    std::vector<SaturnSkeletonPose> restPoses(data->nodes.size());
    std::vector<SaturnSkeletonPose> animationPoses(data->animationMatrices.size());
    for (size_t node = 0; node < data->nodes.size(); ++node)
    {
        if (!matrixToSaturnPose(data->nodes[node].restMatrix, selectedTranslationScaleQ16_16,
                                &restPoses[node], &poseError))
        {
            fbxLoadError = poseError;
            return false;
        }
    }
    for (size_t sample = 0; sample < data->animationMatrices.size(); ++sample)
        if (!matrixToSaturnPose(data->animationMatrices[sample], selectedTranslationScaleQ16_16,
                                &animationPoses[sample], &poseError))
        {
            fbxLoadError = poseError;
            return false;
        }
    for (size_t sample = 0; sample < animationPoses.size(); ++sample)
    {
        const SaturnSkeletonPose &restPose = restPoses[sample % data->nodes.size()];
        for (unsigned int axis = 0; axis < 3; ++axis)
            if (animationPoses[sample].scale[axis] != restPose.scale[axis])
            {
                fbxLoadError =
                    "Animated scale changes cannot be represented by the compact skeleton export.";
                return false;
            }
    }

    unsigned long long namePoolBytes = 0;
    for (size_t index = 0; index < data->nodes.size(); ++index)
    {
        const size_t nameBytes = skeletonNodeName(data->nodes[index], index).size() + 1;
        if (nameBytes > UINT32_MAX || namePoolBytes > UINT32_MAX - nameBytes)
        {
            fbxLoadError = "The skeleton name table exceeds 32-bit offsets.";
            return false;
        }
        namePoolBytes += nameBytes;
    }
    std::vector<uint8_t> nameData;
    nameData.reserve(static_cast<size_t>(namePoolBytes));
    for (size_t index = 0; index < data->nodes.size(); ++index)
    {
        const string name = skeletonNodeName(data->nodes[index], index);
        for (unsigned char character : name)
            nameData.push_back(character);
        nameData.push_back(0);
    }
    const unsigned long long exportedNodeCount = data->nodes.size();
    const unsigned long long animationPoseCount =
        static_cast<unsigned long long>(data->frameCount) * exportedNodeCount;
    const unsigned long long byteChannelBytes =
        exportedNodeCount * 9ULL + animationPoseCount * 6ULL + namePoolBytes;
    const unsigned int dataAlignmentPaddingBytes =
        static_cast<unsigned int>((4ULL - (byteChannelBytes % 4ULL)) % 4ULL);
    const unsigned long long dataBytes =
        exportedNodeCount * 8ULL + byteChannelBytes + dataAlignmentPaddingBytes;

    char destination[MAX_PATH] = {};
    const DWORD pathLength = GetFullPathNameA(path.c_str(), MAX_PATH, destination, NULL);
    if (!pathLength || pathLength >= MAX_PATH)
    {
        fbxLoadError = "The skeleton export path is invalid or too long.";
        return false;
    }
    const string fullPath(destination);
    const size_t separator = fullPath.find_last_of("\\/");
    if (separator == string::npos)
    {
        fbxLoadError = "Could not determine the skeleton export folder.";
        return false;
    }
    const string folder = fullPath.substr(0, separator + 1);
    char temporaryPath[MAX_PATH] = {};
    if (!GetTempFileNameA(folder.c_str(), "skl", 0, temporaryPath))
    {
        fbxLoadError = "Could not create a temporary skeleton export file.";
        return false;
    }

    std::ofstream output(temporaryPath, std::ios::out | std::ios::trunc);
    bool written = output.is_open();
    if (!written) fbxLoadError = "Could not open the temporary skeleton export file.";
    if (written)
    {
        output << "/* Generated Saturn skeleton data. Byte channels are tightly packed. */\n"
               << "#include <stdint.h>\n\n"
               << "typedef struct SaturnSkeletonPose {\n"
               << "    int8_t rot[3]; /* signed 1/256-turn Euler XYZ */\n"
               << "    int8_t pos[3]; /* integer position times the global unit scale */\n"
               << "} SaturnSkeletonPose;\n\n"
               << "typedef struct SaturnSkeletonNode {\n"
               << "    uint16_t parent_index; /* 0xFFFF for a root */\n"
               << "    uint16_t flags; /* bit 0: skin bone; otherwise hierarchy helper */\n"
               << "    uint32_t name_offset;\n"
               << "} SaturnSkeletonNode;\n\n"
               << "#define SATURN_SKELETON_TRANSLATION_SHIFT 0u\n"
               << "#define SATURN_SKELETON_TRANSLATION_UNIT_SCALE_SHIFT 16u\n"
               << "#define SATURN_SKELETON_TRANSLATION_UNIT_SCALE_Q16_16 "
               << selectedTranslationScaleQ16_16 << "u\n"
               << "#define SATURN_SKELETON_ROTATION_STEPS_PER_TURN 256u\n"
               << "#define SATURN_SKELETON_ROTATION_ORDER_XYZ 1u\n"
               << "#define SATURN_SKELETON_SCALE_SHIFT 6u\n"
               << "#define SATURN_SKELETON_FLAG_BONE 1u\n"
               << "#define SATURN_SKELETON_BONE_COUNT " << data->boneCount << "u\n"
               << "#define SATURN_SKELETON_NODE_COUNT " << data->nodes.size() << "u\n"
               << "#define SATURN_SKELETON_ANIMATION_FRAME_COUNT " << data->frameCount << "u\n"
               << "#define SATURN_SKELETON_ANIMATION_FRAME_RATE " << data->frameRate << "u\n"
               << "#define SATURN_SKELETON_NAME_DATA_SIZE_BYTES " << namePoolBytes << "ULL\n"
               << "#define SATURN_SKELETON_DATA_ALIGNMENT_PADDING_BYTES "
               << dataAlignmentPaddingBytes << "u\n"
               << "#define SATURN_SKELETON_DATA_SIZE_BYTES " << dataBytes << "ULL\n"
               << "typedef struct SaturnSkeletonData {\n"
               << "    SaturnSkeletonNode nodes[SATURN_SKELETON_NODE_COUNT];\n"
               << "    int8_t rest_rotation[SATURN_SKELETON_NODE_COUNT][3];\n"
               << "    int8_t rest_position[SATURN_SKELETON_NODE_COUNT][3];\n"
               << "    int8_t rest_scale[SATURN_SKELETON_NODE_COUNT][3];\n"
               << "    SaturnSkeletonPose animation[SATURN_SKELETON_ANIMATION_FRAME_COUNT]"
                  "[SATURN_SKELETON_NODE_COUNT];\n"
               << "    uint8_t name_data[SATURN_SKELETON_NAME_DATA_SIZE_BYTES];\n";
        if (dataAlignmentPaddingBytes)
            output << "    uint8_t data_alignment_padding["
                   << dataAlignmentPaddingBytes << "];\n";
        output << "} SaturnSkeletonData;\n\n"
               << "typedef char SaturnSkeletonPoseSizeCheck[\n"
               << "    sizeof(SaturnSkeletonPose) == 6 ? 1 : -1];\n"
               << "typedef char SaturnSkeletonNodeSizeCheck[\n"
               << "    sizeof(SaturnSkeletonNode) == 8 ? 1 : -1];\n"
               << "typedef char SaturnSkeletonDataSizeCheck[\n"
               << "    sizeof(SaturnSkeletonData) == SATURN_SKELETON_DATA_SIZE_BYTES ? 1 : -1];\n\n"
               << "const SaturnSkeletonData saturn_skeleton = {\n"
               << "    {\n";
        unsigned long long nameOffset = 0;
        for (size_t index = 0; index < data->nodes.size(); ++index)
        {
            const FbxSkeletonNode & node = data->nodes[index];
            output << "        { " << (node.parentIndex < 0 ? "0xFFFF" :
                std::to_string(node.parentIndex)) << ", "
                   << (node.isBone ? "SATURN_SKELETON_FLAG_BONE" : "0") << ", "
                   << nameOffset << "u },\n";
            nameOffset += skeletonNodeName(node, index).size() + 1;
        }
        output << "    },\n    ";
        writeRestChannelInitializer(output, restPoses, SaturnPoseChannel::Rotation);
        output << ",\n    ";
        writeRestChannelInitializer(output, restPoses, SaturnPoseChannel::Position);
        output << ",\n    ";
        writeRestChannelInitializer(output, restPoses, SaturnPoseChannel::Scale);
        output << ",\n    ";
        writeAnimationPoseInitializer(output, animationPoses, data->nodes.size(),
                                      data->frameCount);
        output << ",\n    ";
        writeByteArrayInitializer(output, nameData);
        if (dataAlignmentPaddingBytes)
        {
            output << ",\n    { ";
            for (unsigned int padding = 0; padding < dataAlignmentPaddingBytes; ++padding)
            {
                if (padding) output << ", ";
                output << "0";
            }
            output << " }";
        }
        output << "\n};\n";
        output.close();
        written = written && output.good();
        if (!written && fbxLoadError.empty())
            fbxLoadError = "Could not finish writing the skeleton export file.";
    }

    if (written && MoveFileExA(temporaryPath, fullPath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        *boneCount = data->boneCount;
        *nodeCount = static_cast<unsigned int>(data->nodes.size());
        *frameCount = data->frameCount;
        *dataSizeBytes = dataBytes;
        *translationScaleQ16_16 = selectedTranslationScaleQ16_16;
        return true;
    }
    if (written) fbxLoadError = "Could not replace the destination with the generated skeleton file.";
    DeleteFileA(temporaryPath);
    return false;
}

bool setFbxBonePreview(animated_model_t * model, bool enabled)
{
    BonePreviewData * data = findBonePreview(model);
    if (!data && enabled) return false;
    if (data) data->enabled = enabled;
    return true;
}

bool setFbxBonePreviewPointRemap(animated_model_t * model,
                                 const std::vector<unsigned int> & sourcePoints)
{
    BonePreviewData * data = findBonePreview(model);
    if (!data) return true;
    if (sourcePoints.empty())
    {
        data->activePointRemap.clear();
        return true;
    }
    for (unsigned int sourcePoint : sourcePoints)
        if (sourcePoint >= data->points.size()) return false;
    data->activePointRemap = sourcePoints;
    return true;
}

void discardFbxBonePreview(const animated_model_t * model)
{
    discardBonePreviewData(model);
}

bool applyFbxBonePreviewPose(animated_model_t * model, unsigned int frame,
                             unsigned int nextFrame, float fraction, bool interpolate)
{
    BonePreviewData * data = findBonePreview(model);
    if (!data || !data->enabled || !model || !model->model || !model->keyFrames ||
        frame >= data->frameCount || nextFrame >= data->frameCount ||
        data->matrices.size() != size_t(data->frameCount) * data->clusterCount) return false;
    if (!interpolate) fraction = 0.0f;

    size_t pointOffset = 0;
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
        for (unsigned int point = 0; point < model->model[mesh].nbPoint; ++point, ++pointOffset)
        {
            const size_t sourcePoint = data->activePointRemap.empty() ? pointOffset :
                (pointOffset < data->activePointRemap.size() ? data->activePointRemap[pointOffset] : data->points.size());
            if (sourcePoint >= data->points.size()) return false;
            const BonePreviewPoint & source = data->points[sourcePoint];
            float position[3];
            if (source.cluster == UINT16_MAX)
            {
                const vertex_t & first = model->keyFrames[frame].cVert[pointOffset];
                const vertex_t & second = model->keyFrames[nextFrame].cVert[pointOffset];
                for (unsigned int axis = 0; axis < 3; ++axis)
                    position[axis] = first.point[axis] + (second.point[axis] - first.point[axis]) * fraction;
            }
            else
            {
                if (source.cluster >= data->clusterCount) return false;
                const std::array<float, 12> & first =
                    data->matrices[size_t(frame) * data->clusterCount + source.cluster];
                const std::array<float, 12> & second =
                    data->matrices[size_t(nextFrame) * data->clusterCount + source.cluster];
                const float input[3] = {source.source[X], source.source[Y], source.source[Z]};
                for (unsigned int axis = 0; axis < 3; ++axis)
                {
                    const size_t row = axis * 4;
                    float firstValue = first[row] * input[X] + first[row + 1] * input[Y] +
                        first[row + 2] * input[Z] + first[row + 3];
                    float secondValue = second[row] * input[X] + second[row + 1] * input[Y] +
                        second[row + 2] * input[Z] + second[row + 3];
                    position[axis] = (firstValue + (secondValue - firstValue) * fraction) * data->importScale[axis];
                }
                for (int axis = X; axis <= Z; ++axis)
                    rotatePreviewPoint(position, axis, data->importRotation[axis]);
                for (unsigned int axis = 0; axis < 3; ++axis)
                    position[axis] = (position[axis] + data->importTranslation[axis]) * data->inverseModelScale[axis];
            }
            for (unsigned int axis = 0; axis < 3; ++axis)
                model->model[mesh].pntbl[point].point[axis] = position[axis];
        }
    return pointOffset == (data->activePointRemap.empty() ? data->points.size() : data->activePointRemap.size());
}

static string ufbxString(ufbx_string value)
{
    return value.data ? string(value.data, value.length) : string();
}

bool loadFBX(const string &filePath, float *scaleFactor, animated_model_t *model,
             int rotationX, int rotationY, int rotationZ,
             float translationX, float translationY, float translationZ)
{
    discardBonePreviewData(model);
    discardFbxSkeletonData(model);
    fbxLoadError = "FBX import failed because its geometry or animation data is unsupported.";
    ufbx_load_opts loadOptions = {};
    loadOptions.load_external_files = true;
    loadOptions.ignore_missing_external_files = true;
    ufbx_error error = {};
    std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> scene(
        ufbx_load_file(filePath.c_str(), &loadOptions, &error), &ufbx_free_scene);
    if (!scene)
    {
        char errorText[512] = {};
        ufbx_format_error(errorText, sizeof(errorText), &error);
        fbxLoadError = "Could not parse FBX file: " + string(errorText);
        cerr << "ERROR READING FBX FILE: " << filePath << "\n" << errorText << "\n";
        return false;
    }

    const ufbx_anim_stack *stack = NULL;
    double stackDuration = 0.0;
    for (size_t stackIndex = 0; stackIndex < scene->anim_stacks.count; ++stackIndex)
    {
        const ufbx_anim_stack *candidate = scene->anim_stacks.data[stackIndex];
        const double duration = candidate->time_end - candidate->time_begin;
        if (duration > stackDuration)
        {
            stack = candidate;
            stackDuration = duration;
        }
    }
    if (!stack && scene->anim_stacks.count) stack = scene->anim_stacks.data[0];
    const ufbx_anim *animation = stack ? stack->anim : scene->anim;
    const double timeBegin = stack ? stack->time_begin : (scene->anim ? scene->anim->time_begin : 0.0);
    const double timeEnd = stack ? stack->time_end : (scene->anim ? scene->anim->time_end : timeBegin);
    const double duration = timeEnd - timeBegin;
    const double sourceFrameRate = scene->settings.frames_per_second;
    if (!std::isfinite(timeBegin) || !std::isfinite(timeEnd) || !std::isfinite(duration) ||
        !std::isfinite(sourceFrameRate) || duration < 0.0 || sourceFrameRate <= 0.0)
    {
        fbxLoadError = "FBX animation has an invalid time range or frame rate.";
        return false;
    }
    const unsigned int frameRate = MAX(1u, static_cast<unsigned int>(std::floor(sourceFrameRate + 0.5)));
    const double requestedFrames = duration > 0.0 ? std::ceil(duration * frameRate) + 1.0 : 1.0;
    const unsigned int frameCount = static_cast<unsigned int>(MIN(
        requestedFrames, static_cast<double>(ZTP_MAX_ANIMATION_FRAMES)));
    if (requestedFrames > ZTP_MAX_ANIMATION_FRAMES)
        cout << "FBX animation requires " << requestedFrames << " frames; sampling at "
             << frameCount << " frames to respect the limit.\n";
    model->nbFrames = frameCount;
    model->framerate = frameRate;
    model->keyFrames = new key_frame_t[frameCount]();

    ufbx_evaluate_opts evaluateOptions = {};
    evaluateOptions.evaluate_skinning = true;
    evaluateOptions.evaluate_caches = true;
    std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> basePose(
        ufbx_evaluate_scene(scene.get(), animation, timeBegin, &evaluateOptions, &error), &ufbx_free_scene);
    if (!basePose) return false;

    std::vector<size_t> meshNodes;
    size_t totalPoints = 0;
    for (size_t nodeIndex = 0; nodeIndex < basePose->nodes.count; ++nodeIndex)
    {
        const ufbx_node *node = basePose->nodes.data[nodeIndex];
        if (!node->mesh || !node->visible || !node->mesh->num_vertices || !node->mesh->num_triangles) continue;
        if (meshNodes.size() >= 256 || node->mesh->num_vertices > UINT_MAX - totalPoints)
        {
            fbxLoadError = "FBX exceeds the supported number of meshes or total points.";
            return false;
        }
        meshNodes.push_back(nodeIndex);
        totalPoints += node->mesh->num_vertices;
    }
    if (meshNodes.empty() || !totalPoints || uint64_t(totalPoints) * frameCount > 4u * 1024u * 1024u)
        return false;

    FbxSkeletonData skeleton;
    skeleton.owner = model;
    skeleton.frameCount = frameCount;
    skeleton.frameRate = frameRate;
    for (unsigned int axis = 0; axis < 3; ++axis)
    {
        skeleton.importScale[axis] = scaleFactor[axis];
        skeleton.importTranslation[axis] = axis == X ? translationX : axis == Y ? translationY : translationZ;
        skeleton.importRotation[axis] = axis == X ? rotationX : axis == Y ? rotationY : rotationZ;
    }
    std::set<const ufbx_node *> boneNodes;
    std::set<const ufbx_node *> hierarchyNodes;
    for (size_t meshIndex = 0; meshIndex < meshNodes.size(); ++meshIndex)
    {
        const ufbx_mesh *mesh = scene->nodes.data[meshNodes[meshIndex]]->mesh;
        for (size_t skinIndex = 0; skinIndex < mesh->skin_deformers.count; ++skinIndex)
        {
            const ufbx_skin_deformer *skin = mesh->skin_deformers.data[skinIndex];
            for (size_t clusterIndex = 0; clusterIndex < skin->clusters.count; ++clusterIndex)
            {
                const ufbx_node *boneNode = skin->clusters.data[clusterIndex]->bone_node;
                if (boneNode) boneNodes.insert(boneNode);
            }
        }
    }
    for (const ufbx_node *boneNode : boneNodes)
        for (const ufbx_node *node = boneNode; node; node = node->parent)
            if (!hierarchyNodes.insert(node).second) break;
    skeleton.boneCount = static_cast<unsigned int>(boneNodes.size());
    std::map<const ufbx_node *, unsigned int> hierarchyIndices;
    std::vector<const ufbx_node *> hierarchyNodeOrder;
    for (size_t nodeIndex = 0; nodeIndex < scene->nodes.count; ++nodeIndex)
    {
        const ufbx_node *node = scene->nodes.data[nodeIndex];
        if (hierarchyNodes.find(node) == hierarchyNodes.end()) continue;
        FbxSkeletonNode exportedNode;
        exportedNode.name = ufbxString(node->name);
        exportedNode.isBone = boneNodes.find(node) != boneNodes.end();
        exportedNode.sourceNodeIndex = nodeIndex;
        for (unsigned int component = 0; component < 12; ++component)
            exportedNode.sourceLocalMatrix[component] = node->node_to_parent.v[component];
        hierarchyIndices[node] = static_cast<unsigned int>(skeleton.nodes.size());
        hierarchyNodeOrder.push_back(node);
        skeleton.nodes.push_back(exportedNode);
    }
    for (size_t index = 0; index < skeleton.nodes.size(); ++index)
    {
        const ufbx_node *node = hierarchyNodeOrder[index];
        const auto parent = hierarchyIndices.find(node->parent);
        if (parent != hierarchyIndices.end())
            skeleton.nodes[index].parentIndex = static_cast<int>(parent->second);
    }
    for (FbxSkeletonNode & node : skeleton.nodes)
        if (!exportMatrix(skeleton, node, node.sourceLocalMatrix.data(), node.restMatrix.data()))
            skeleton.transformsSupported = false;
    if (skeleton.boneCount)
        skeleton.animationMatrices.resize(size_t(frameCount) * skeleton.nodes.size());

    model->nbModels = static_cast<unsigned int>(meshNodes.size());
    model->nbTextures = 0;
    model->texture = new texture_t[2048]();
    BonePreviewData bonePreview;
    bonePreview.owner = model;
    bonePreview.frameCount = frameCount;
    bonePreview.points.resize(totalPoints);
    std::vector<size_t> meshClusterOffsets(meshNodes.size(), 0);
    size_t previewClusterCount = 0;
    size_t previewPointOffset = 0;
    for (size_t meshIndex = 0; meshIndex < meshNodes.size(); ++meshIndex)
    {
        const ufbx_mesh *mesh = basePose->nodes.data[meshNodes[meshIndex]]->mesh;
        const ufbx_skin_deformer *skin = mesh->skin_deformers.count ? mesh->skin_deformers.data[0] : NULL;
        meshClusterOffsets[meshIndex] = previewClusterCount;
        if (skin && previewClusterCount + skin->clusters.count < UINT16_MAX)
            previewClusterCount += skin->clusters.count;
        else skin = NULL;
        for (size_t point = 0; point < mesh->num_vertices; ++point, ++previewPointOffset)
        {
            BonePreviewPoint &previewPoint = bonePreview.points[previewPointOffset];
            previewPoint.source[X] = static_cast<float>(mesh->vertices.data[point].x);
            previewPoint.source[Y] = static_cast<float>(mesh->vertices.data[point].y);
            previewPoint.source[Z] = static_cast<float>(mesh->vertices.data[point].z);
            previewPoint.cluster = UINT16_MAX;
            if (!skin || point >= skin->vertices.count) continue;
            const ufbx_skin_vertex &skinVertex = skin->vertices.data[point];
            float strongestWeight = -1.0f;
            for (size_t influence = 0; influence < skinVertex.num_weights; ++influence)
            {
                const size_t weightIndex = size_t(skinVertex.weight_begin) + influence;
                if (weightIndex >= skin->weights.count) break;
                const ufbx_skin_weight &weight = skin->weights.data[weightIndex];
                if (weight.cluster_index >= skin->clusters.count || weight.weight <= strongestWeight) continue;
                strongestWeight = static_cast<float>(weight.weight);
                previewPoint.cluster = static_cast<uint16_t>(meshClusterOffsets[meshIndex] + weight.cluster_index);
            }
        }
    }
    bonePreview.clusterCount = static_cast<unsigned int>(previewClusterCount);
    if (bonePreview.clusterCount)
        bonePreview.matrices.resize(size_t(frameCount) * bonePreview.clusterCount);
    std::vector<string> texturePaths;
    std::map<string, unsigned int> textureIndices;
    const auto sourceTextureForMaterial = [&](const ufbx_material *material) -> int
    {
        if (!material) return -1;
        const ufbx_material_map *map = &material->fbx.diffuse_color;
        if (!map->texture) map = &material->pbr.base_color;
        if (!map->texture || map->texture->type != UFBX_TEXTURE_FILE) return -1;
        string path = ufbxString(map->texture->absolute_filename);
        if (path.empty()) path = ufbxString(map->texture->filename);
        if (path.empty()) return -1;
        const auto existing = textureIndices.find(path);
        if (existing != textureIndices.end()) return static_cast<int>(existing->second);
        if (texturePaths.size() >= 2048) return -1;
        const unsigned int index = static_cast<unsigned int>(texturePaths.size());
        texturePaths.push_back(path);
        textureIndices[path] = index;
        model->texture[index].name = ufbxString(map->texture->name);
        if (model->texture[index].name.empty()) model->texture[index].name = ufbxString(material->name);
        return static_cast<int>(index);
    };
    for (size_t meshIndex = 0; meshIndex < meshNodes.size(); ++meshIndex)
    {
        const ufbx_node *node = basePose->nodes.data[meshNodes[meshIndex]];
        const ufbx_mesh *mesh = node->mesh;
        model_t &outMesh = model->model[meshIndex];
        size_t outputPolygonCount = 0;
        for (size_t faceIndex = 0; faceIndex < mesh->num_faces; ++faceIndex)
        {
            const ufbx_face face = mesh->faces.data[faceIndex];
            if (face.num_indices < 3) continue;
            const size_t facePolygonCount = face.num_indices == 4 ? 1 : face.num_indices - 2;
            if (facePolygonCount > maxPolygons || outputPolygonCount > maxPolygons - facePolygonCount)
                return false;
            outputPolygonCount += facePolygonCount;
        }
        outMesh.nbPoint = static_cast<unsigned int>(mesh->num_vertices);
        outMesh.pntbl = new vertex_t[outMesh.nbPoint]();
        outMesh.pltbl = new polygon_t[outputPolygonCount]();
        outMesh.nbPolygon = static_cast<unsigned int>(outputPolygonCount);
        strncpy(outMesh.name, node->name.data, sizeof(outMesh.name) - 1);

        size_t polygonIndex = 0;
        for (size_t faceIndex = 0; faceIndex < mesh->num_faces; ++faceIndex)
        {
            const ufbx_face face = mesh->faces.data[faceIndex];
            if (face.num_indices < 3) continue;
            const bool quad = face.num_indices == 4;
            std::vector<uint32_t> triangles;
            size_t facePolygonCount = 1;
            if (!quad)
            {
                triangles.resize(static_cast<size_t>(face.num_indices - 2) * 3);
                facePolygonCount = ufbx_triangulate_face(
                    triangles.data(), triangles.size(), mesh, face);
                if (facePolygonCount != face.num_indices - 2) return false;
            }
            for (size_t facePolygon = 0; facePolygon < facePolygonCount; ++facePolygon)
            {
                polygon_t &polygon = outMesh.pltbl[polygonIndex++];
                polygon.texture = -1;
                polygon.color.r = polygon.color.g = polygon.color.b = polygon.color.a = 255;
                polygon.color.rgb = 0x7fff;
                polygon.SGL_ATTR.atrb = CL_Gouraud;
                int faceTexture = -1;
                const size_t sourceFaceIndex = faceIndex;
                if (sourceFaceIndex < mesh->face_material.count)
                {
                    const uint32_t materialIndex = mesh->face_material.data[sourceFaceIndex];
                    if (materialIndex < mesh->materials.count)
                    {
                        const ufbx_material *material = mesh->materials.data[materialIndex];
                        faceTexture = sourceTextureForMaterial(material);
                        const ufbx_material_map &diffuse = material->fbx.diffuse_color.has_value ?
                            material->fbx.diffuse_color : material->pbr.base_color;
                        const ufbx_vec3 color = diffuse.value_vec3;
                        if (diffuse.has_value && diffuse.value_components >= 3)
                        {
                            polygon.color.r = static_cast<uint8_t>(MAX(0.0, MIN(255.0, color.x * 255.0)));
                            polygon.color.g = static_cast<uint8_t>(MAX(0.0, MIN(255.0, color.y * 255.0)));
                            polygon.color.b = static_cast<uint8_t>(MAX(0.0, MIN(255.0, color.z * 255.0)));
                            polygon.color.rgb = static_cast<uint16_t>((polygon.color.r >> 3) |
                                ((polygon.color.g >> 3) << 5) | ((polygon.color.b >> 3) << 10));
                        }
                    }
                }
                polygon.texture = faceTexture;
                uint32_t meshCorners[4];
                if (quad)
                {
                    meshCorners[0] = face.index_begin;
                    meshCorners[1] = face.index_begin + 3;
                    meshCorners[2] = face.index_begin + 2;
                    meshCorners[3] = face.index_begin + 1;
                }
                else
                {
                    meshCorners[0] = triangles[facePolygon * 3];
                    meshCorners[1] = triangles[facePolygon * 3 + 2];
                    meshCorners[2] = triangles[facePolygon * 3 + 1];
                    meshCorners[3] = meshCorners[2];
                }
                for (unsigned int corner = 0; corner < 3; ++corner)
                {
                    const uint32_t meshCorner = meshCorners[corner];
                    if (meshCorner >= mesh->vertex_indices.count) return false;
                    const uint32_t pointIndex = mesh->vertex_indices.data[meshCorner];
                    if (pointIndex >= mesh->num_vertices) return false;
                    polygon.vertIdx[corner] = pointIndex;
                    if (mesh->vertex_uv.exists)
                    {
                        const ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, meshCorner);
                        polygon.vertUV[corner].U = static_cast<float>(uv.x);
                        polygon.vertUV[corner].V = static_cast<float>(uv.y);
                    }
                }
                if (quad)
                {
                    const uint32_t meshCorner = meshCorners[3];
                    if (meshCorner >= mesh->vertex_indices.count) return false;
                    const uint32_t pointIndex = mesh->vertex_indices.data[meshCorner];
                    if (pointIndex >= mesh->num_vertices) return false;
                    polygon.vertIdx[3] = pointIndex;
                    if (mesh->vertex_uv.exists)
                    {
                        const ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, meshCorner);
                        polygon.vertUV[3].U = static_cast<float>(uv.x);
                        polygon.vertUV[3].V = static_cast<float>(uv.y);
                    }
                }
                else
                {
                    polygon.vertIdx[3] = polygon.vertIdx[2];
                    polygon.vertUV[3] = polygon.vertUV[2];
                }
            }
        }
        if (polygonIndex != outMesh.nbPolygon) return false;

        for (size_t pointIndex = 0; pointIndex < mesh->num_vertices; ++pointIndex)
        {
            vertex_t &vertex = outMesh.pntbl[pointIndex];
            const uint32_t firstCorner = mesh->vertex_first_index.data[pointIndex];
            ufbx_vec3 position = mesh->vertices.data[pointIndex];
            if (firstCorner != UFBX_NO_INDEX && mesh->skinned_position.exists)
                position = ufbx_get_vertex_vec3(&mesh->skinned_position, firstCorner);
            if (mesh->skinned_is_local)
                position = ufbx_transform_position(&node->geometry_to_world, position);
            for (unsigned int axis = 0; axis < 3; ++axis)
                vertex.point[axis] = static_cast<float>(position.v[axis]) * scaleFactor[axis];
        }
    }

    model->nbTextures = static_cast<unsigned int>(texturePaths.size());
    size_t slash = filePath.find_last_of("\\/");
    const string textureFolder = slash == string::npos ? string(".\\") : filePath.substr(0, slash + 1);
    for (unsigned int index = 0; index < model->nbTextures; ++index)
        ReadTGAFile(textureFolder, &model->texture[index], texturePaths[index]);

    if (useUV)
    {
        std::vector<texture_t> sourceTextures(model->texture, model->texture + model->nbTextures);
        bool needsFallback = false;
        for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
            for (unsigned int polygonIndex = 0; polygonIndex < model->model[meshIndex].nbPolygon; ++polygonIndex)
            {
                const int textureIndex = model->model[meshIndex].pltbl[polygonIndex].texture;
                if (textureIndex < 0 || textureIndex >= static_cast<int>(sourceTextures.size()))
                    needsFallback = true;
            }
        const unsigned int fallbackIndex = static_cast<unsigned int>(sourceTextures.size());
        if (needsFallback)
        {
            sourceTextures.push_back(texture_t());
            sourceTextures.back().name = "None";
            createFakeTexture(&sourceTextures.back());
        }
        cacheAutomaticLodSourceTextures(model, sourceTextures.data(),
                                        static_cast<unsigned int>(sourceTextures.size()));
        model->nbTextures = 0;
        for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
            for (unsigned int polygonIndex = 0; polygonIndex < model->model[meshIndex].nbPolygon; ++polygonIndex)
            {
                polygon_t *polygon = &model->model[meshIndex].pltbl[polygonIndex];
                const int sourceIndex = polygon->texture;
                const texture_t &source = sourceTextures[
                    sourceIndex >= 0 && sourceIndex < static_cast<int>(fallbackIndex) ?
                    static_cast<unsigned int>(sourceIndex) : fallbackIndex];
                createTexture(polygon, model, source);
            }
        for (size_t index = 0; index < sourceTextures.size(); ++index)
            delete [] sourceTextures[index].pixel;
    }
    else if (model->nbTextures)
        specialConditions(0, static_cast<unsigned short>(model->nbTextures), model);

    for (unsigned int frame = 0; frame < frameCount; ++frame)
    {
        const double sampleTime = frameCount > 1 ?
            timeBegin + (timeEnd - timeBegin) * frame / (frameCount - 1) : timeBegin;
        std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> evaluatedPose(
            frame == 0 ? NULL : ufbx_evaluate_scene(
                scene.get(), animation, sampleTime, &evaluateOptions, &error), &ufbx_free_scene);
        const ufbx_scene *pose = frame == 0 ? basePose.get() : evaluatedPose.get();
        if (!pose) return false;

        if (skeleton.boneCount)
            for (size_t nodeIndex = 0; nodeIndex < skeleton.nodes.size(); ++nodeIndex)
            {
                const FbxSkeletonNode &node = skeleton.nodes[nodeIndex];
                if (node.sourceNodeIndex >= pose->nodes.count)
                {
                    fbxLoadError = "FBX animation changed the skeleton node hierarchy.";
                    return false;
                }
                std::array<double, 12> sourceMatrix;
                for (unsigned int component = 0; component < 12; ++component)
                    sourceMatrix[component] =
                        pose->nodes.data[node.sourceNodeIndex]->node_to_parent.v[component];
                std::array<int32_t, 12> &matrix =
                    skeleton.animationMatrices[size_t(frame) * skeleton.nodes.size() + nodeIndex];
                if (!exportMatrix(skeleton, node, sourceMatrix.data(), matrix.data()))
                    skeleton.transformsSupported = false;
            }

        model->keyFrames[frame].cVert = new vertex_t[totalPoints]();
        size_t frameOffset = 0;
        for (size_t meshIndex = 0; meshIndex < meshNodes.size(); ++meshIndex)
        {
            const size_t nodeIndex = meshNodes[meshIndex];
            const ufbx_node *node = pose->nodes.data[nodeIndex];
            const ufbx_mesh *mesh = node->mesh;
            const ufbx_mesh *baseMesh = basePose->nodes.data[nodeIndex]->mesh;
            if (!mesh || mesh->num_vertices != baseMesh->num_vertices ||
                mesh->num_faces != baseMesh->num_faces ||
                mesh->vertex_indices.count != baseMesh->vertex_indices.count)
                return false;
            for (size_t index = 0; index < mesh->vertex_indices.count; ++index)
                if (mesh->vertex_indices.data[index] != baseMesh->vertex_indices.data[index]) return false;
            for (size_t faceIndex = 0; faceIndex < mesh->num_faces; ++faceIndex)
                if (mesh->faces.data[faceIndex].index_begin != baseMesh->faces.data[faceIndex].index_begin ||
                    mesh->faces.data[faceIndex].num_indices != baseMesh->faces.data[faceIndex].num_indices)
                    return false;

            if (bonePreview.clusterCount)
            {
                const ufbx_skin_deformer *baseSkin = baseMesh->skin_deformers.count ?
                    baseMesh->skin_deformers.data[0] : NULL;
                const ufbx_skin_deformer *poseSkin = mesh->skin_deformers.count ?
                    mesh->skin_deformers.data[0] : NULL;
                if (!baseSkin || !poseSkin || baseSkin->clusters.count != poseSkin->clusters.count ||
                    meshClusterOffsets[meshIndex] + poseSkin->clusters.count > bonePreview.clusterCount)
                {
                    bonePreview.clusterCount = 0;
                    bonePreview.matrices.clear();
                }
                else
                {
                    for (size_t clusterIndex = 0; clusterIndex < poseSkin->clusters.count; ++clusterIndex)
                    {
                        const ufbx_matrix &matrix = poseSkin->clusters.data[clusterIndex]->geometry_to_world;
                        std::array<float, 12> &stored = bonePreview.matrices[
                            size_t(frame) * bonePreview.clusterCount + meshClusterOffsets[meshIndex] + clusterIndex];
                        stored = {{
                            static_cast<float>(matrix.m00), static_cast<float>(matrix.m01),
                            static_cast<float>(matrix.m02), static_cast<float>(matrix.m03),
                            static_cast<float>(matrix.m10), static_cast<float>(matrix.m11),
                            static_cast<float>(matrix.m12), static_cast<float>(matrix.m13),
                            static_cast<float>(matrix.m20), static_cast<float>(matrix.m21),
                            static_cast<float>(matrix.m22), static_cast<float>(matrix.m23)
                        }};
                    }
                }
            }

            const model_t &baseMeshData = model->model[meshIndex];
            for (size_t pointIndex = 0; pointIndex < mesh->num_vertices; ++pointIndex)
            {
                vertex_t &vertex = model->keyFrames[frame].cVert[frameOffset + pointIndex];
                vertex = baseMeshData.pntbl[pointIndex];
                const uint32_t firstCorner = mesh->vertex_first_index.data[pointIndex];
                ufbx_vec3 position = mesh->vertices.data[pointIndex];
                if (firstCorner != UFBX_NO_INDEX && mesh->skinned_position.exists)
                    position = ufbx_get_vertex_vec3(&mesh->skinned_position, firstCorner);
                if (mesh->skinned_is_local)
                    position = ufbx_transform_position(&node->geometry_to_world, position);
                for (unsigned int axis = 0; axis < 3; ++axis)
                    vertex.point[axis] = static_cast<float>(position.v[axis]) * scaleFactor[axis];
            }
            frameOffset += mesh->num_vertices;
        }
    }

    for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
    {
        recalculateNormals(&model->model[meshIndex]);
        pointNormals(&model->model[meshIndex]);
    }
    rotateModelData(model, rotationX, rotationY, rotationZ);
    translateModelData(model, translationX, translationY, translationZ);
    calcLight(model);
    findScale(model, false);
    if (bonePreview.clusterCount)
    {
        for (unsigned int axis = 0; axis < 3; ++axis)
        {
            bonePreview.importScale[axis] = scaleFactor[axis];
            bonePreview.importTranslation[axis] = axis == X ? translationX : axis == Y ? translationY : translationZ;
            bonePreview.inverseModelScale[axis] = model->scale[axis] != 0.0f ? 1.0f / model->scale[axis] : 1.0f;
            bonePreview.importRotation[axis] = axis == X ? rotationX : axis == Y ? rotationY : rotationZ;
        }
        discardBonePreviewData(model);
        bonePreviewData.push_back(std::move(bonePreview));
    }
    if (skeleton.boneCount && !skeleton.nodes.empty())
        fbxSkeletonData.push_back(std::move(skeleton));
    return true;
}