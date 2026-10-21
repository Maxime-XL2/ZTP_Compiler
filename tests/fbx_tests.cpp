#include "../SRC/COMMON.H"
#include "../third_party/ufbx/ufbx.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>

bool loadFBX(const string &, float *, animated_model_t *, int, int, int, float, float, float);
extern string fbxLoadError;

void rotateModelData(animated_model_t *, int, int, int) {}
void translateModelData(animated_model_t *, float, float, float) {}
void findScale(animated_model_t *, bool) {}
void recalculateNormals(model_t *) {}
void pointNormals(model_t *) {}
void calcLight(animated_model_t *) {}
uint32_t useUV = 0;
int ReadTGAFile(string, texture_t *, string) { return 0; }
void createFakeTexture(texture_t *) {}
void createTexture(polygon_t *, animated_model_t *, const texture_t &) {}
void specialConditions(unsigned short, unsigned short, animated_model_t *) {}

int main(int argc, char **argv)
{
    assert(argc >= 2 && argc <= 4);
    if (argc == 3 && std::strcmp(argv[2], "--inspect-animation") == 0)
    {
        ufbx_load_opts options = {};
        ufbx_error error = {};
        std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> scene(
            ufbx_load_file(argv[1], &options, &error), &ufbx_free_scene);
        if (!scene)
        {
            char errorText[512] = {};
            ufbx_format_error(errorText, sizeof(errorText), &error);
            std::cerr << errorText << "\n";
            return 1;
        }
        size_t meshNodes = 0;
        size_t totalVertices = 0;
        size_t totalTriangles = 0;
        size_t emptyMeshes = 0;
        for (size_t nodeIndex = 0; nodeIndex < scene->nodes.count; ++nodeIndex)
        {
            const ufbx_node *node = scene->nodes.data[nodeIndex];
            if (!node->mesh || !node->visible) continue;
            const ufbx_mesh *mesh = node->mesh;
            ++meshNodes;
            totalVertices += mesh->num_vertices;
            totalTriangles += mesh->num_triangles;
            if (!mesh->num_triangles) ++emptyMeshes;
            if (mesh->num_triangles)
                std::cout << "Renderable mesh " << nodeIndex << ": "
                          << (node->name.data ? string(node->name.data, node->name.length) : string())
                          << ", points " << mesh->num_vertices
                          << ", triangles " << mesh->num_triangles << "\n";
        }
        std::cout << "Visible mesh totals: " << meshNodes << " meshes, " << totalVertices
                  << " points, " << totalTriangles << " triangles, " << emptyMeshes
                  << " without triangles\n";
        std::cout << "Scene materials: " << scene->materials.count << ", textures: "
                  << scene->textures.count << ", texture files: " << scene->texture_files.count << "\n";
        for (size_t materialIndex = 0; materialIndex < scene->materials.count; ++materialIndex)
        {
            const ufbx_material *material = scene->materials.data[materialIndex];
            const ufbx_texture *diffuse = material->fbx.diffuse_color.texture;
            const ufbx_texture *baseColor = material->pbr.base_color.texture;
            std::cout << "Material " << materialIndex << ": "
                      << (material->name.data ? string(material->name.data, material->name.length) : string())
                      << ", diffuse texture " << (diffuse && diffuse->name.data ?
                          string(diffuse->name.data, diffuse->name.length) : string("none"))
                      << ", base-color texture " << (baseColor && baseColor->name.data ?
                          string(baseColor->name.data, baseColor->name.length) : string("none")) << "\n";
        }
        for (size_t textureIndex = 0; textureIndex < scene->textures.count; ++textureIndex)
        {
            const ufbx_texture *texture = scene->textures.data[textureIndex];
            std::cout << "Texture " << textureIndex << ": "
                      << (texture->name.data ? string(texture->name.data, texture->name.length) : string())
                      << ", type " << texture->type
                      << ", path " << (texture->absolute_filename.data ?
                          string(texture->absolute_filename.data, texture->absolute_filename.length) : string())
                      << ", filename " << (texture->filename.data ?
                          string(texture->filename.data, texture->filename.length) : string())
                      << ", embedded bytes " << texture->content.size;
            if (texture->video && texture->video->content.size)
                std::cout << ", video bytes " << texture->video->content.size;
            std::cout << "\n";
        }
        std::cout << "Animation stacks: " << scene->anim_stacks.count << "\n";
        if (scene->anim)
            std::cout << "Global animation: time " << scene->anim->time_begin << ".."
                      << scene->anim->time_end << ", layers " << scene->anim->layers.count << "\n";
        else
            std::cout << "Global animation: none\n";
        std::cout << "Blend deformers: " << scene->blend_deformers.count
                  << ", blend shapes: " << scene->blend_shapes.count
                  << ", cache deformers: " << scene->cache_deformers.count
                  << ", cache files: " << scene->cache_files.count << "\n";
        size_t blendChannels = 0;
        size_t animatedBlendChannels = 0;
        size_t activeBlendChannels = 0;
        size_t blendShapeOffsets = 0;
        for (size_t shapeIndex = 0; shapeIndex < scene->blend_shapes.count; ++shapeIndex)
            blendShapeOffsets += scene->blend_shapes.data[shapeIndex]->num_offsets;
        for (size_t deformerIndex = 0; deformerIndex < scene->blend_deformers.count; ++deformerIndex)
            for (size_t channelIndex = 0; channelIndex < scene->blend_deformers.data[deformerIndex]->channels.count; ++channelIndex)
            {
                const ufbx_blend_channel *channel =
                    scene->blend_deformers.data[deformerIndex]->channels.data[channelIndex];
                ++blendChannels;
                if (channel->weight != 0.0) ++activeBlendChannels;
                for (size_t layerIndex = 0; scene->anim && layerIndex < scene->anim->layers.count; ++layerIndex)
                    for (size_t propIndex = 0; propIndex < scene->anim->layers.data[layerIndex]->anim_props.count; ++propIndex)
                    {
                        const ufbx_anim_prop *prop = &scene->anim->layers.data[layerIndex]->anim_props.data[propIndex];
                        if (prop->element == &channel->element) ++animatedBlendChannels;
                    }
            }
        std::cout << "Blend channels: " << blendChannels << ", animated: " << animatedBlendChannels
                  << ", nonzero weights: " << activeBlendChannels
                  << ", total point offsets: " << blendShapeOffsets << "\n";
        ufbx_evaluate_opts evaluateOptions = {};
        evaluateOptions.evaluate_skinning = true;
        evaluateOptions.evaluate_caches = true;
        for (size_t cacheIndex = 0; cacheIndex < scene->cache_deformers.count; ++cacheIndex)
        {
            const ufbx_cache_deformer *cache = scene->cache_deformers.data[cacheIndex];
            const string name = cache->name.data ? string(cache->name.data, cache->name.length) : string();
            const string channel = cache->channel.data ? string(cache->channel.data, cache->channel.length) : string();
            const string filename = cache->file && cache->file->filename.data ?
                string(cache->file->filename.data, cache->file->filename.length) : string();
            std::cout << "Cache " << cacheIndex << ": " << name << ", channel " << channel
                      << ", file " << filename << "\n";
        }
        for (size_t stackIndex = 0; stackIndex < scene->anim_stacks.count; ++stackIndex)
        {
            const ufbx_anim_stack *stack = scene->anim_stacks.data[stackIndex];
            size_t curves = 0;
            size_t keys = 0;
            for (size_t layerIndex = 0; layerIndex < stack->layers.count; ++layerIndex)
                for (size_t valueIndex = 0; valueIndex < stack->layers.data[layerIndex]->anim_values.count; ++valueIndex)
                    for (size_t axis = 0; axis < 3; ++axis)
                    {
                        const ufbx_anim_curve *curve =
                            stack->layers.data[layerIndex]->anim_values.data[valueIndex]->curves[axis];
                        if (curve)
                        {
                            ++curves;
                            keys += curve->keyframes.count;
                        }
                    }
            const string name = stack->name.data ? string(stack->name.data, stack->name.length) : string();
            std::cout << "Stack " << stackIndex << ": " << name << ", time "
                      << stack->time_begin << ".." << stack->time_end << ", layers "
                      << stack->layers.count << ", curves " << curves << ", keys " << keys << "\n";
            std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> startPose(
                ufbx_evaluate_scene(scene.get(), stack->anim, stack->time_begin,
                                    &evaluateOptions, &error), &ufbx_free_scene);
            std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> endPose(
                ufbx_evaluate_scene(scene.get(), stack->anim, stack->time_end,
                                    &evaluateOptions, &error), &ufbx_free_scene);
            bool movesPoints = false;
            if (startPose && endPose)
                for (size_t nodeIndex = 0; nodeIndex < startPose->nodes.count && !movesPoints; ++nodeIndex)
                {
                    const ufbx_node *startNode = startPose->nodes.data[nodeIndex];
                    const ufbx_node *endNode = endPose->nodes.data[nodeIndex];
                    if (!startNode->mesh || !endNode->mesh ||
                        startNode->mesh->num_vertices != endNode->mesh->num_vertices) continue;
                    for (size_t pointIndex = 0; pointIndex < startNode->mesh->num_vertices && !movesPoints; ++pointIndex)
                    {
                        ufbx_vec3 startPosition = startNode->mesh->vertices.data[pointIndex];
                        ufbx_vec3 endPosition = endNode->mesh->vertices.data[pointIndex];
                        const uint32_t startCorner = startNode->mesh->vertex_first_index.data[pointIndex];
                        const uint32_t endCorner = endNode->mesh->vertex_first_index.data[pointIndex];
                        if (startCorner != UFBX_NO_INDEX && startNode->mesh->skinned_position.exists)
                            startPosition = ufbx_get_vertex_vec3(&startNode->mesh->skinned_position, startCorner);
                        if (endCorner != UFBX_NO_INDEX && endNode->mesh->skinned_position.exists)
                            endPosition = ufbx_get_vertex_vec3(&endNode->mesh->skinned_position, endCorner);
                        if (startNode->mesh->skinned_is_local)
                            startPosition = ufbx_transform_position(&startNode->geometry_to_world, startPosition);
                        if (endNode->mesh->skinned_is_local)
                            endPosition = ufbx_transform_position(&endNode->geometry_to_world, endPosition);
                        for (size_t axis = 0; axis < 3; ++axis)
                            if (std::fabs(startPosition.v[axis] - endPosition.v[axis]) > 0.0001)
                                movesPoints = true;
                    }
                }
            std::cout << "  Evaluated point motion: " << (movesPoints ? "yes" : "no") << "\n";
        }
        return 0;
    }
    const bool expectMotion = argc == 3 && std::strcmp(argv[2], "--expect-motion") == 0;
    const bool expectQuads = argc == 3 && std::strcmp(argv[2], "--expect-quads") == 0;
    const bool previewBones = argc == 3 && std::strcmp(argv[2], "--preview-bones") == 0;
    const bool exportSkeleton = argc == 4 && std::strcmp(argv[2], "--export-skeleton") == 0;
    animated_model_t model = {};
    model.nbFrames = 0;
    model.model = new model_t[256]();
    model.keyFrames = NULL;
    float scale[3] = {1.0f, 1.0f, 1.0f};

    const bool loaded = loadFBX(argv[1], scale, &model, 0, 0, 0, 0.0f, 0.0f, 0.0f);
    if (!loaded)
    {
        std::cerr << fbxLoadError << "\n";
        return 1;
    }
    assert(model.nbModels > 0);
    assert(model.nbFrames > 0);
    unsigned int totalPoints = 0;
    unsigned int totalPolygons = 0;
    bool hasQuad = false;
    for (unsigned int mesh = 0; mesh < model.nbModels; ++mesh)
    {
        assert(model.model[mesh].nbPoint > 0);
        assert(model.model[mesh].nbPolygon > 0);
        totalPoints += model.model[mesh].nbPoint;
        totalPolygons += model.model[mesh].nbPolygon;
        for (unsigned int polygon = 0; polygon < model.model[mesh].nbPolygon; ++polygon)
        {
            const polygon_t &face = model.model[mesh].pltbl[polygon];
            assert(face.vertIdx[0] < model.model[mesh].nbPoint);
            assert(face.vertIdx[1] < model.model[mesh].nbPoint);
            assert(face.vertIdx[2] < model.model[mesh].nbPoint);
            assert(face.vertIdx[3] < model.model[mesh].nbPoint);
            assert(face.vertIdx[0] != face.vertIdx[1]);
            assert(face.vertIdx[1] != face.vertIdx[2]);
            assert(face.vertIdx[0] != face.vertIdx[2]);
            if (face.vertIdx[2] != face.vertIdx[3]) hasQuad = true;
        }
    }
    assert(totalPoints > 0);
    assert(totalPolygons > 0);
    if (expectQuads) assert(hasQuad);
    const polygon_t &firstFace = model.model[0].pltbl[0];
    const float *first = model.model[0].pntbl[firstFace.vertIdx[0]].point;
    const float *second = model.model[0].pntbl[firstFace.vertIdx[1]].point;
    const float *third = model.model[0].pntbl[firstFace.vertIdx[2]].point;
    const float normalZ = (second[X] - first[X]) * (third[Y] - first[Y]) -
                          (second[Y] - first[Y]) * (third[X] - first[X]);
    if (expectMotion) assert(normalZ < 0.0f);
    for (unsigned int frame = 0; frame < model.nbFrames; ++frame)
        assert(model.keyFrames[frame].cVert != NULL);

    bool animationChangesPosition = false;
    const unsigned int lastFrame = model.nbFrames - 1;
    for (unsigned int point = 0; point < totalPoints; ++point)
        for (unsigned int axis = 0; axis < 3; ++axis)
            if (std::fabs(model.keyFrames[0].cVert[point].point[axis] -
                          model.keyFrames[lastFrame].cVert[point].point[axis]) > 0.0001f)
                animationChangesPosition = true;
    std::cout << "FBX frames: " << model.nbFrames << " at " << model.framerate << " fps, meshes: "
              << model.nbModels << ", points: " << totalPoints
              << ", polygons: " << totalPolygons << ", sampled motion: "
              << (animationChangesPosition ? "yes" : "no") << ", quads: "
              << (hasQuad ? "yes" : "no") << "\n";
    if (expectMotion) assert(animationChangesPosition);
    if (exportSkeleton)
    {
        assert(hasFbxSkeleton(&model));
        unsigned int bones = 0;
        unsigned int nodes = 0;
        unsigned int frames = 0;
        unsigned long long dataBytes = 0;
        uint32_t translationScaleQ16_16 = 0;
        assert(exportFbxSkeletonC(&model, argv[3], &bones, &nodes, &frames, &dataBytes,
                                  &translationScaleQ16_16));
        assert(bones > 0 && nodes >= bones && frames == model.nbFrames && frames > 1 && dataBytes > 0);
        assert(translationScaleQ16_16 >= 65536u);
        std::ifstream generated(argv[3], std::ios::in | std::ios::binary);
        assert(generated);
        const string source((std::istreambuf_iterator<char>(generated)),
                            std::istreambuf_iterator<char>());
        assert(source.find("#define SATURN_SKELETON_TRANSLATION_SHIFT 0u") != string::npos);
        assert(source.find("#define SATURN_SKELETON_TRANSLATION_UNIT_SCALE_Q16_16 " +
                           std::to_string(translationScaleQ16_16) + "u") != string::npos);
        assert(source.find("sizeof(SaturnSkeletonPose) == 6") != string::npos);
        assert(source.find("int8_t rot[3]") != string::npos);
        assert(source.find("int8_t pos[3]") != string::npos);
        assert(source.find("rest_position[SATURN_SKELETON_NODE_COUNT][3]") != string::npos);
        assert(source.find("rest_rotation[SATURN_SKELETON_NODE_COUNT][3]") != string::npos);
        assert(source.find("rest_scale[SATURN_SKELETON_NODE_COUNT][3]") != string::npos);
        assert(source.find("SaturnSkeletonPose animation[SATURN_SKELETON_ANIMATION_FRAME_COUNT]") !=
               string::npos);
        assert(source.find("#define SATURN_SKELETON_ROTATION_STEPS_PER_TURN 256u") != string::npos);
        assert(source.find("int16_t translation_q8_8") == string::npos);
        assert(source.find("#define SATURN_SKELETON_NAME_DATA_SIZE_BYTES ") != string::npos);
        const string nameSizeMacro = "#define SATURN_SKELETON_NAME_DATA_SIZE_BYTES ";
        const size_t nameSizeStart = source.find(nameSizeMacro) + nameSizeMacro.size();
        const unsigned long long nameBytes =
            std::stoull(source.substr(nameSizeStart));
        const unsigned long long poseCount =
            static_cast<unsigned long long>(frames) * nodes;
        const unsigned long long channelBytes =
            static_cast<unsigned long long>(nodes) * 9ULL +
            poseCount * 6ULL +
            nameBytes;
        const unsigned long long expectedBytes =
            static_cast<unsigned long long>(nodes) * 8ULL +
            channelBytes + (4ULL - (channelBytes % 4ULL)) % 4ULL;
        assert(dataBytes == expectedBytes);
        assert(source.find("#define SATURN_SKELETON_ANIMATION_FRAME_COUNT " +
                           std::to_string(frames) + "u") != string::npos);
        assert(source.find("#define SATURN_SKELETON_DATA_SIZE_BYTES " +
                           std::to_string(dataBytes) + "ULL") != string::npos);
        std::cout << "Skeleton C export: " << bones << " bones, " << nodes
                  << " hierarchy nodes, " << frames << " frames, "
                  << dataBytes << " Saturn data bytes\n";
    }
    if (previewBones)
    {
        assert(hasFbxBonePreview(&model));
        ufbx_load_opts loadOptions = {};
        ufbx_error loadError = {};
        std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> sourceScene(
            ufbx_load_file(argv[1], &loadOptions, &loadError), &ufbx_free_scene);
        assert(sourceScene);
        const ufbx_mesh *sourceMesh = NULL;
        for (size_t node = 0; node < sourceScene->nodes.count && !sourceMesh; ++node)
            if (sourceScene->nodes.data[node]->mesh && sourceScene->nodes.data[node]->visible &&
                sourceScene->nodes.data[node]->mesh->num_triangles)
                sourceMesh = sourceScene->nodes.data[node]->mesh;
        assert(sourceMesh && sourceMesh->num_vertices == totalPoints);
        assert(sourceMesh->skin_deformers.count > 0);
        const ufbx_skin_deformer *skin = sourceMesh->skin_deformers.data[0];
        std::vector<vertex_t> originalFrame(model.keyFrames[0].cVert,
                                            model.keyFrames[0].cVert + totalPoints);
        assert(setFbxBonePreview(&model, true));
        assert(applyFbxBonePreviewPose(&model, 0, lastFrame, 1.0f, false));
        std::vector<vertex_t> firstPose(totalPoints);
        for (unsigned int point = 0; point < totalPoints; ++point)
            firstPose[point] = model.model[0].pntbl[point];
        assert(applyFbxBonePreviewPose(&model, 0, lastFrame, 1.0f, true));
        bool rigidPreviewMoves = false;
        bool interpolationMoves = false;
        for (unsigned int point = 0; point < totalPoints; ++point)
            for (unsigned int axis = 0; axis < 3; ++axis)
            {
                if (std::fabs(model.model[0].pntbl[point].point[axis] -
                              originalFrame[point].point[axis]) > 0.0001f)
                    rigidPreviewMoves = true;
                if (std::fabs(model.model[0].pntbl[point].point[axis] -
                              firstPose[point].point[axis]) > 0.0001f)
                    interpolationMoves = true;
                assert(model.keyFrames[0].cVert[point].point[axis] == originalFrame[point].point[axis]);
            }
        assert(rigidPreviewMoves);
        assert(interpolationMoves);
        assert(applyFbxBonePreviewPose(&model, lastFrame, lastFrame, 0.0f, false));
        unsigned int singleInfluencePoints = 0;
        for (unsigned int point = 0; point < totalPoints && point < skin->vertices.count; ++point)
        {
            if (skin->vertices.data[point].num_weights != 1) continue;
            ++singleInfluencePoints;
            for (unsigned int axis = 0; axis < 3; ++axis)
                assert(std::fabs(model.model[0].pntbl[point].point[axis] -
                                 model.keyFrames[lastFrame].cVert[point].point[axis]) < 0.001f);
        }
        assert(singleInfluencePoints > 0);
        assert(setFbxBonePreview(&model, true));
        assert(replaceAutomaticLodBase(&model, 40));
        const unsigned int reducedPointCount = model.model[0].nbPoint;
        assert(reducedPointCount > 0 && reducedPointCount < totalPoints);
        std::vector<vertex_t> reducedFrame(model.keyFrames[0].cVert,
                                           model.keyFrames[0].cVert + reducedPointCount);
        assert(applyFbxBonePreviewPose(&model, 0, lastFrame, 1.0f, true));
        bool reducedPreviewMoves = false;
        for (unsigned int point = 0; point < reducedPointCount; ++point)
            for (unsigned int axis = 0; axis < 3; ++axis)
            {
                if (std::fabs(model.model[0].pntbl[point].point[axis] -
                              reducedFrame[point].point[axis]) > 0.0001f)
                    reducedPreviewMoves = true;
                assert(model.keyFrames[0].cVert[point].point[axis] == reducedFrame[point].point[axis]);
            }
        assert(reducedPreviewMoves);
        assert(restoreAutomaticLodBase(&model));
        assert(model.model[0].nbPoint == totalPoints);
        assert(applyFbxBonePreviewPose(&model, lastFrame, lastFrame, 0.0f, false));
        for (unsigned int point = 0; point < totalPoints && point < skin->vertices.count; ++point)
        {
            if (skin->vertices.data[point].num_weights != 1) continue;
            for (unsigned int axis = 0; axis < 3; ++axis)
                assert(std::fabs(model.model[0].pntbl[point].point[axis] -
                                 model.keyFrames[lastFrame].cVert[point].point[axis]) < 0.001f);
        }
        assert(setFbxBonePreview(&model, false));
        std::cout << "Dominant-bone preview: full and reduced meshes animate, "
                     "single-influence points match, export keyframes unchanged\n";
    }

    discardFbxBonePreview(&model);
    discardFbxSkeleton(&model);
    for (unsigned int frame = 0; frame < model.nbFrames; ++frame)
        delete [] model.keyFrames[frame].cVert;
    for (unsigned int mesh = 0; mesh < model.nbModels; ++mesh)
    {
        delete [] model.model[mesh].pntbl;
        delete [] model.model[mesh].pltbl;
    }
    delete [] model.keyFrames;
    delete [] model.model;
    delete [] model.texture;
    return 0;
}