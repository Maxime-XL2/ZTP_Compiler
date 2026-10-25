
#include "SRC/COMMON.H"
#include <commdlg.h>
#include <commctrl.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cctype>
#include <new>
#include <vector>

LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
void EnableOpenGL(HWND hwnd, HDC*, HGLRC*);
void DisableOpenGL(HWND, HDC, HGLRC);
void deleteModels(animated_model_t * model);
extern int USE_SGL;
extern string fbxLoadError;
extern uint32_t maxGeneratedTextureWidth;
extern uint32_t maxGeneratedTextureHeight;
extern string fileOutName;
extern player_t PLAYER[4];
extern unsigned int specialTextureId[256];
extern unsigned int asciiTextureId;
extern unsigned int texId[1280];
extern void setTextures(animated_model_t *);
extern void setModelTextures(animated_model_t *, unsigned int *);
extern void refreshModelTextures(animated_model_t *, unsigned int);
extern void resetPlayer(player_t *);
extern int ReadTGAFile(string, texture_t *, string);
extern animated_model_t * loadingConfigured(unsigned int, int, float, uint32_t, uint32_t, string, string, bool = false);
extern animated_model_t * loadingConfigured(unsigned int, int, float, uint32_t, uint32_t, string, string,
                                             bool, int, int, int);
extern animated_model_t * loadingConfigured(unsigned int, int, float, uint32_t, uint32_t, string, string,
                                             bool, int, int, int, float, float, float, uint32_t, uint32_t);
extern animated_model_t * loadZTPFile(const string &);
extern int activeFace;
extern int activeModel;
extern int playAnimation;
extern int DisplayTEST;
extern bool enableSelfShadowing;
extern void resetAnimationPlaybackTiming();
extern void setLightOrientation(float azimuthDegrees, float elevationDegrees);
extern void calcLight(animated_model_t *);
extern int render(animated_model_t *, animated_model_t *, const unsigned int *, const unsigned int *);
extern float defLight[3];
extern HWND hwnd;
float animationPlaybackSpeed = 1.0f;
float cameraMovementSpeed = 1.0f;
extern bool animationLoopPlayback;

namespace
{
const int IDM_FILE_OPEN = 1001;
const int IDM_FILE_EXIT = 1002;
const int IDM_COMPARISON_OPEN = 1003;
const int IDM_COMPARISON_REMOVE = 1004;
const int IDC_COMPARISON_FILE = 1005;
const int IDM_FILE_SAVE = 1006;
const int IDM_FILE_SAVE_AS = 1007;
const int IDM_EXPORT_SKELETON = 1008;
const int IDM_OPTIONS = 1101;
const int IDM_SKIN_LOAD = 1201;
const int IDM_SKIN_PREVIEW_BASE = 1202;
const int IDM_SKIN_EXPORT = 1203;
const int IDM_SKIN_REMOVE = 1204;
const int IDM_SKIN_PREVIEW_FIRST = 30000;
const int SIDE_PANEL_WIDTH = ZTP_SIDE_PANEL_WIDTH;
const int IDC_FRAME_COUNT = 2001;
const int IDC_FRAME_COUNT_LABEL = 2009;
const int IDC_SGL_MODE = 2002;
const int IDC_SCALE = 2003;
const int IDC_USE_UV = 2004;
const int IDC_FORCE_4BPP = 2005;
const int IDC_POWER_OF_8 = 2006;
const int IDC_MAX_TEXTURE_WIDTH = 2007;
const int IDC_MAX_TEXTURE_HEIGHT = 2008;
const int IDC_OPEN_ANOTHER = 2101;
const int IDC_CURRENT_FILE = 2102;
const int IDC_SURFACE_STATUS = 2103;
const int IDC_SELECTION_HELP = 2104;
const int IDC_SURFACE_DOUBLE_SIDED = 2110;
const int IDC_SURFACE_ENVIRONMENTAL_MAPPING = 2111;
const int IDC_SURFACE_MESH = 2112;
const int IDC_SURFACE_GOURAUD = 2113;
const int IDC_SURFACE_LIGHT = 2114;
const int IDC_SURFACE_DEPTH = 2115;
const int IDC_SURFACE_PALETTE = 2116;
const int IDC_SURFACE_NEAR_CLIP = 2117;
const int IDC_SURFACE_REALTIME_GOURAUD = 2118;
const int IDC_SURFACE_FLIP = 2120;
const int IDC_SURFACE_ROTATE = 2121;
const int IDC_SURFACE_SORT = 2122;
const int IDC_SORT_STATUS = 2123;
const int IDC_SURFACE_TEXTURE_PREVIEW = 2124;
const int IDC_SURFACE_TEXTURE_INFO = 2125;
const int IDC_SURFACE_TEXTURE_SIZE_LABEL = 2126;
const int IDC_SURFACE_TEXTURE_SIZE = 2127;
const int IDC_SURFACE_TEXTURE_APPLY = 2128;
const int IDC_MODEL_GROUP = 2130;
const int IDC_SURFACE_GROUP = 2131;
const int IDC_ANIMATION_SPEED_LABEL = 2140;
const int IDC_ANIMATION_SPEED = 2141;
const int IDC_ANIMATION_START = 2146;
const int IDC_ANIMATION_END = 2147;
const int IDC_ANIMATION_RANGE_APPLY = 2148;
const int IDC_ANIMATION_START_LABEL = 2173;
const int IDC_ANIMATION_END_LABEL = 2174;
const int IDC_ANIMATION_LIST = 2175;
const int IDC_ANIMATION_ADD = 2176;
const int IDC_ANIMATION_DELETE = 2177;
const int IDC_CAMERA_SPEED_LABEL = 2142;
const int IDC_CAMERA_SPEED = 2143;
const int IDC_WIREFRAME = 2144;
const int IDC_SELF_SHADOWING = 2145;
const int IDC_LIGHT_AZIMUTH_LABEL = 2150;
const int IDC_LIGHT_AZIMUTH = 2151;
const int IDC_LIGHT_ELEVATION_LABEL = 2152;
const int IDC_LIGHT_ELEVATION = 2153;
const int IDC_LOD_REDUCTION_LABEL = 2160;
const int IDC_LOD_REDUCTION = 2161;
const int IDC_LOD_GENERATE = 2162;
const int IDC_LOD_SHOW = 2163;
const int IDC_LOD_STATUS = 2164;
const int IDC_LOD_LEVEL = 2165;
const int IDC_LOD_DELETE = 2166;
const int IDC_LOD_MERGE_QUADS = 2178;
const int IDC_ROTATION_X = 2167;
const int IDC_ROTATION_Y = 2168;
const int IDC_ROTATION_Z = 2169;
const int IDC_TRANSLATION_X = 2170;
const int IDC_TRANSLATION_Y = 2171;
const int IDC_TRANSLATION_Z = 2172;
const int IDC_ANIMATION_NAME = 2179;
const int IDC_ANIMATION_NAME_LABEL = 2180;
const int IDC_ANIMATION_LOOP = 2181;
const int IDC_BONE_PREVIEW = 2182;
const int IDC_ANIMATION_FRAME_DELTA_LABEL = 2183;
const int IDC_ANIMATION_FRAME_DELTA = 2184;
const int IDC_ANIMATION_RESAMPLE = 2185;
const float animationSpeedValues[] = {0.0f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f};
const float cameraSpeedValues[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
const unsigned int surfaceTextureSizeOptions[] = {8, 16, 32, 64, 128, 256};

struct LoadOptions
{
    int frameCount;
    int sglMode;
    float scale;
    bool useUv;
    uint32_t specialOptions;
    uint32_t maxTextureWidth;
    uint32_t maxTextureHeight;
    int rotationX;
    int rotationY;
    int rotationZ;
    float translationX;
    float translationY;
    float translationZ;
};

struct LoadOptionsDialogContext
{
    LoadOptions * options;
    bool showFrameCount;
};

struct PreviewAnimationRange
{
    unsigned int startFrame;
    unsigned int endFrame;
    string name;
    bool loop;
    unsigned int frameDelta;
    unsigned int storedDelta;
};

struct SkinVariant
{
    string name;
    texture_t atlas;
    animated_model_t * model;
    std::vector<unsigned int> textureIds;

    SkinVariant() : atlas(), model(NULL) {}
};

LoadOptions loadOptions = {100, 0, 1.0f, true, 1, 32, 32, 0, 0, 0, 0.0f, 0.0f, 0.0f};
animated_model_t * currentModel = NULL;
animated_model_t * comparisonModel = NULL;
std::vector<unsigned int> comparisonTextureIds;
std::vector<SkinVariant> skinVariants;
int previewSkinIndex = -1;
std::vector<PreviewAnimationRange> addedAnimations;
std::vector<string> protectedReferencePaths;
LoadOptions comparisonOptions = {static_cast<int>(ZTP_MAX_ANIMATION_FRAMES), 0, 1.0f, true, 1, 512, 256, 0, 0, 0, 0.0f, 0.0f, 0.0f};
string currentFolder;
string currentFile;
bool glReady = false;
bool currentModelIsZTP = false;
bool currentModelIsObj = false;
bool updatingAnimationName = false;
bool experimentalAnimationResampling = false;
bool ztpBaseReplaced = false;
string ztpSavePath;
string convertedModelSavePath;
bool ztpSaved = false;
uint32_t savedEditRevision = 0;
uint32_t savedLodRevision = 0;
std::vector<std::vector<vertex_t> > ztpBasePose;
uint32_t shownSelectionRevision = 0xFFFFFFFFu;
HBITMAP surfaceTexturePreviewBitmap = NULL;

string absoluteModelPath(const string & path);
bool confirmZTPSave(HWND owner);
void rebuildSkinVariants(HWND owner);
void syncSkinSurfaceAttributes();

void updateFileMenu(HWND owner)
{
    HMENU fileMenu = GetSubMenu(GetMenu(owner), 0);
    if (!fileMenu) return;
    DeleteMenu(fileMenu, IDM_FILE_SAVE, MF_BYCOMMAND);
    DeleteMenu(fileMenu, IDM_FILE_SAVE_AS, MF_BYCOMMAND);
    if (currentModel)
    {
        InsertMenuA(fileMenu, IDM_COMPARISON_OPEN, MF_BYCOMMAND | MF_STRING, IDM_FILE_SAVE, "Save");
        InsertMenuA(fileMenu, IDM_COMPARISON_OPEN, MF_BYCOMMAND | MF_STRING, IDM_FILE_SAVE_AS, "Save as...");
        const UINT saveState = MF_BYCOMMAND | (USE_SGL == 2 ? MF_GRAYED : MF_ENABLED);
        EnableMenuItem(fileMenu, IDM_FILE_SAVE, saveState);
        EnableMenuItem(fileMenu, IDM_FILE_SAVE_AS, saveState);
    }
    EnableMenuItem(fileMenu, IDM_EXPORT_SKELETON, MF_BYCOMMAND |
        (hasFbxSkeleton(currentModel) ? MF_ENABLED : MF_GRAYED));
    DrawMenuBar(owner);
}

void updateSkinMenu(HWND owner)
{
    HMENU skinMenu = GetSubMenu(GetMenu(owner), 1);
    if (!skinMenu) return;
    while (GetMenuItemCount(skinMenu) > 0) DeleteMenu(skinMenu, 0, MF_BYPOSITION);
    AppendMenuA(skinMenu, MF_STRING | (currentModel ? MF_ENABLED : MF_GRAYED),
                IDM_SKIN_LOAD, "Load skin atlas (OBJ/FBX)...");
    AppendMenuA(skinMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(skinMenu, MF_STRING | (currentModel ? MF_ENABLED : MF_GRAYED),
                IDM_SKIN_PREVIEW_BASE, "Preview base model");
    for (unsigned int index = 0; index < skinVariants.size(); ++index)
    {
        string label = "Skin " + std::to_string(index + 1) + ": " + skinVariants[index].name;
        AppendMenuA(skinMenu, MF_STRING |
            (skinVariants[index].model ? MF_ENABLED : MF_GRAYED),
            IDM_SKIN_PREVIEW_FIRST + index, label.c_str());
    }
    AppendMenuA(skinMenu, MF_SEPARATOR, 0, NULL);
    const bool hasSelectedSkin = previewSkinIndex >= 0 &&
        previewSkinIndex < static_cast<int>(skinVariants.size()) &&
        skinVariants[previewSkinIndex].model;
    AppendMenuA(skinMenu, MF_STRING | (hasSelectedSkin ? MF_ENABLED : MF_GRAYED),
                IDM_SKIN_EXPORT, "Export selected skin model...");
    AppendMenuA(skinMenu, MF_STRING | (hasSelectedSkin ? MF_ENABLED : MF_GRAYED),
                IDM_SKIN_REMOVE, "Remove selected skin");
    CheckMenuItem(skinMenu,
        previewSkinIndex < 0 ? IDM_SKIN_PREVIEW_BASE :
            IDM_SKIN_PREVIEW_FIRST + previewSkinIndex,
        MF_BYCOMMAND | MF_CHECKED);
    DrawMenuBar(owner);
}

void refreshLodStatus(HWND owner, unsigned int selected)
{
    unsigned int baseFaces = 0, basePoints = 0;
    if (currentModel)
        for (unsigned int mesh = 0; mesh < currentModel->nbModels; ++mesh)
        {
            baseFaces += currentModel->model[mesh].nbPolygon;
            basePoints += currentModel->model[mesh].nbPoint;
        }
    char status[128];
    const double textureSizeKiB = calculateTextureExportSize(currentModel, false) / 1024.0;
    if (selected)
        sprintf(status, "Faces: %u / %u\r\nPoints: %u / %u\r\nTextures: %.1f KiB",
            automaticLodFaceCount(), baseFaces, automaticLodPointCount(), basePoints, textureSizeKiB);
    else sprintf(status, "Faces: %u\r\nPoints: %u\r\nTextures: %.1f KiB",
        baseFaces, basePoints, textureSizeKiB);
    SetDlgItemTextA(owner, IDC_LOD_STATUS, status);
}

void refreshLodControls(HWND owner, unsigned int selected = 0)
{
    SendDlgItemMessageA(owner, IDC_LOD_LEVEL, CB_RESETCONTENT, 0, 0);
    SendDlgItemMessageA(owner, IDC_LOD_LEVEL, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Base (LOD 0)"));
    const unsigned int count = automaticLodLevelCount(currentModel);
    for (unsigned int level = 1; level <= count; ++level)
    {
        char label[48];
        sprintf(label, "LOD %u (%u%%)", level, currentModel->lodData->levels[level - 1].reduction);
        SendDlgItemMessageA(owner, IDC_LOD_LEVEL, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    }
    if (selected > count) selected = 0;
    SendDlgItemMessageA(owner, IDC_LOD_LEVEL, CB_SETCURSEL, selected, 0);
    EnableWindow(GetDlgItem(owner, IDC_LOD_LEVEL), count != 0);
    EnableWindow(GetDlgItem(owner, IDC_LOD_SHOW), count != 0);
    EnableWindow(GetDlgItem(owner, IDC_LOD_DELETE), selected != 0);
    if (selected) selectAutomaticLod(currentModel, selected);
    else resetAutomaticLod();
    CheckDlgButton(owner, IDC_LOD_SHOW, selected ? BST_CHECKED : BST_UNCHECKED);
    refreshLodStatus(owner, selected);
}

void refreshAnimationRangeControls(HWND owner, animated_model_t * model)
{
    const bool enabled = model && model->nbFrames > 0;
    const uint32_t frameUnit = MAX(1u, MIN(static_cast<uint32_t>(PLAYER[0].ANIMCTRL.sourceFrameDelta), 60u));
    const unsigned int maxFrame = enabled ? MIN(MIN(model->nbFrames - 1, 0xFFFEu), 0x10000u / frameUnit - 1) : 0;
    const unsigned int endFrame = enabled ? MIN(PLAYER[0].ANIMCTRL.endFrm, maxFrame + 1) : 0;
    char value[32];
    sprintf(value, "%u", enabled ? MIN(PLAYER[0].ANIMCTRL.startFrm, maxFrame) : 0);
    SetDlgItemTextA(owner, IDC_ANIMATION_START, value);
    sprintf(value, "%u", endFrame ? endFrame - 1 : maxFrame);
    SetDlgItemTextA(owner, IDC_ANIMATION_END, value);
    EnableWindow(GetDlgItem(owner, IDC_ANIMATION_START), enabled);
    EnableWindow(GetDlgItem(owner, IDC_ANIMATION_END), enabled);
    EnableWindow(GetDlgItem(owner, IDC_ANIMATION_RANGE_APPLY), enabled);
}

unsigned int maxPreviewFrame(animated_model_t * model)
{
    if (!model || !model->nbFrames) return 0;
    const uint32_t frameUnit = MAX(1u, MIN(static_cast<uint32_t>(PLAYER[0].ANIMCTRL.sourceFrameDelta), 60u));
    return MIN(MIN(model->nbFrames - 1, 0xFFFEu), 0x10000u / frameUnit - 1);
}

void setAnimationPreviewRange(unsigned int startFrame, unsigned int endFrame)
{
    const uint32_t frameUnit = MAX(1u, MIN(static_cast<uint32_t>(PLAYER[0].ANIMCTRL.sourceFrameDelta), 60u));
    PLAYER[0].ANIMCTRL.startFrm = static_cast<uint16_t>(startFrame);
    PLAYER[0].ANIMCTRL.endFrm = static_cast<uint16_t>(endFrame + 1);
    PLAYER[0].ANIMCTRL.currentFrm = static_cast<uint16_t>(startFrame * frameUnit);
    PLAYER[0].ANIMCTRL.currentKeyFrm = static_cast<uint16_t>(startFrame);
    resetAnimationPlaybackTiming();
    refreshAnimationRangeControls(hwnd, currentModel);
}

void refreshAddedAnimations(HWND owner, unsigned int selected);

string animationSidecarPath()
{
    if (currentModelIsZTP) return ztpSavePath + ".animations.txt";
    return currentFolder + currentFile + ".animations.txt";
}

unsigned int selectedAnimationIndex()
{
    const LRESULT selected = SendDlgItemMessageA(hwnd, IDC_ANIMATION_LIST, CB_GETCURSEL, 0, 0);
    return selected >= 0 && static_cast<unsigned int>(selected) < addedAnimations.size() ?
        static_cast<unsigned int>(selected) : 0xFFFFFFFFu;
}

unsigned int activeAnimationFrameDelta()
{
    const unsigned int selected = selectedAnimationIndex();
    return experimentalAnimationResampling && selected < addedAnimations.size() ?
        addedAnimations[selected].frameDelta : 4;
}

unsigned int activeAnimationStoredDelta()
{
    const unsigned int selected = selectedAnimationIndex();
    return selected < addedAnimations.size() ? addedAnimations[selected].storedDelta : 4;
}

bool hasAnimatedPointPositions(const animated_model_t * model)
{
    if (!model || model->nbFrames < 2 || !model->keyFrames || !model->keyFrames[0].cVert) return false;
    unsigned int totalPoints = 0;
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
        totalPoints += model->model[mesh].nbPoint;

    const vertex_t * firstFrame = model->keyFrames[0].cVert;
    for (unsigned int frame = 1; frame < model->nbFrames; ++frame)
    {
        const vertex_t * vertices = model->keyFrames[frame].cVert;
        if (!vertices) return false;
        for (unsigned int point = 0; point < totalPoints; ++point)
            for (unsigned int axis = 0; axis < XYZ; ++axis)
                if (std::fabs(vertices[point].point[axis] - firstFrame[point].point[axis]) > 1.0e-6f)
                    return true;
    }
    return false;
}

bool writeAnimationSidecar(const string & path, const std::vector<PreviewAnimationRange> & animations,
                           const std::vector<PreviewAnimationRange> & memoryAnimations)
{
    if (animations.size() != memoryAnimations.size()) return false;
    bool hasAnimations = false;
    for (unsigned int index = 0; index < animations.size(); ++index)
    {
        const bool hasOutputRange = animations[index].startFrame <= animations[index].endFrame;
        const bool hasMemoryRange = memoryAnimations[index].startFrame <= memoryAnimations[index].endFrame;
        if (hasOutputRange != hasMemoryRange) return false;
        hasAnimations = hasAnimations || hasOutputRange;
    }
    if (!hasAnimations)
    {
        if (DeleteFileA(path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND ||
            GetLastError() == ERROR_PATH_NOT_FOUND) return true;
        return false;
    }

    const string temporaryPath = path + ".tmp";
    std::ofstream output(temporaryPath.c_str(), std::ios::out | std::ios::trunc);
    if (!output) return false;
    output << "ZTP_ANIMATIONS 4\n";
    for (unsigned int index = 0; index < animations.size(); ++index)
    {
        const PreviewAnimationRange & animation = animations[index];
        if (animation.startFrame > animation.endFrame) continue;
        output << animation.startFrame << ' ' << animation.endFrame << ' '
               << (animation.loop ? 1 : 0) << ' ' << animation.frameDelta << ' '
               << animation.storedDelta << ' ' << animation.name << '\n';
    }
    output << "# In-memory frame indexes (0-based):\n";
    for (unsigned int index = 0; index < memoryAnimations.size(); ++index)
    {
        const PreviewAnimationRange & animation = memoryAnimations[index];
        if (animation.startFrame > animation.endFrame) continue;
        const string name = animation.name.empty() ?
            "Animation " + std::to_string(index + 1) : animation.name;
        output << "# " << name << ": " << animation.startFrame << '-'
               << animation.endFrame << '\n';
    }
    output.close();
    if (!output.good() || !MoveFileExA(temporaryPath.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileA(temporaryPath.c_str());
        return false;
    }
    return true;
}

bool saveAnimationSidecar()
{
    return writeAnimationSidecar(animationSidecarPath(), addedAnimations, addedAnimations);
}

void loadAnimationSidecar(animated_model_t * model)
{
    const string path = animationSidecarPath();
    std::ifstream input(path.c_str());
    if (!input.is_open())
    {
        const DWORD attributes = GetFileAttributesA(path.c_str());
        const DWORD pathError = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_SUCCESS;
        const bool missing = pathError == ERROR_FILE_NOT_FOUND || pathError == ERROR_PATH_NOT_FOUND;
        if (!missing)
        {
            MessageBoxA(hwnd, "Could not read the animation ranges file.",
                        "Animation load failed", MB_OK | MB_ICONERROR);
            return;
        }
        if (currentModelIsZTP || currentModelIsObj || !hasAnimatedPointPositions(model)) return;

        unsigned int endFrame = maxPreviewFrame(model);
        string name = currentFile;
        const size_t extension = name.find_last_of('.');
        if (extension != string::npos) name.resize(extension);
        addedAnimations.push_back({0, endFrame, name, true, 4, 4});
        if (!saveAnimationSidecar())
        {
            addedAnimations.pop_back();
            MessageBoxA(hwnd, "Could not create the animation ranges file.",
                        "Animation save failed", MB_OK | MB_ICONERROR);
            refreshAddedAnimations(hwnd, 0xFFFFFFFFu);
            return;
        }
        refreshAddedAnimations(hwnd, 0);
        setAnimationPreviewRange(0, endFrame);
        return;
    }

    string header;
    if (!std::getline(input, header)) return;
    if (!header.empty() && header[header.size() - 1] == '\r') header.resize(header.size() - 1);
    const bool versionOne = header == "ZTP_ANIMATIONS 1";
    const bool versionTwo = header == "ZTP_ANIMATIONS 2";
    const bool versionThree = header == "ZTP_ANIMATIONS 3";
    if (!versionOne && !versionTwo && !versionThree && header != "ZTP_ANIMATIONS 4") return;

    const unsigned int maxFrame = maxPreviewFrame(model);
    string line;
    while (addedAnimations.size() < 12 && std::getline(input, line))
    {
        std::istringstream entry(line);
        unsigned int startFrame = 0;
        unsigned int endFrame = 0;
        if (!(entry >> startFrame >> endFrame) || startFrame > endFrame || startFrame > maxFrame)
            continue;
        unsigned int loopValue = 1;
        unsigned int frameDelta = 4;
        unsigned int storedDelta = 4;
        if ((!versionOne && (!(entry >> loopValue) || loopValue > 1)) ||
            ((versionThree || header == "ZTP_ANIMATIONS 4") &&
             (!(entry >> frameDelta) || frameDelta < 1 || frameDelta > 16)) ||
            (header == "ZTP_ANIMATIONS 4" &&
             (!(entry >> storedDelta) || storedDelta < 1 || storedDelta > 16))) continue;
        if (versionThree) storedDelta = frameDelta;
        string name;
        std::getline(entry >> std::ws, name);
        if (!name.empty() && name[name.size() - 1] == '\r') name.resize(name.size() - 1);
        if (name.size() > 48) name.resize(48);
        addedAnimations.push_back({startFrame, MIN(endFrame, maxFrame), name, loopValue != 0,
                                   frameDelta, storedDelta});
    }
    experimentalAnimationResampling = std::any_of(addedAnimations.begin(), addedAnimations.end(),
        [](const PreviewAnimationRange & animation) {
            return animation.frameDelta != animation.storedDelta;
        });
    if (!addedAnimations.empty())
    {
        refreshAddedAnimations(hwnd, 0);
        animationLoopPlayback = addedAnimations[0].loop;
        PLAYER[0].ANIMCTRL.frameDelta = activeAnimationFrameDelta();
        PLAYER[0].ANIMCTRL.sourceFrameDelta = activeAnimationStoredDelta();
        setAnimationPreviewRange(addedAnimations[0].startFrame, addedAnimations[0].endFrame);
    }
}

void commitSelectedAnimationName(HWND owner)
{
    if (updatingAnimationName) return;
    const LRESULT selected = SendDlgItemMessageA(owner, IDC_ANIMATION_LIST, CB_GETCURSEL, 0, 0);
    if (selected < 0 || static_cast<unsigned int>(selected) >= addedAnimations.size()) return;
    char name[49] = {};
    GetDlgItemTextA(owner, IDC_ANIMATION_NAME, name, sizeof(name));
    addedAnimations[static_cast<unsigned int>(selected)].name = name;
    if (!saveAnimationSidecar())
        MessageBoxA(owner, "Could not save the animation ranges file.",
                    "Animation save failed", MB_OK | MB_ICONERROR);
}

void refreshAddedAnimations(HWND owner, unsigned int selected)
{
    SendDlgItemMessageA(owner, IDC_ANIMATION_LIST, CB_RESETCONTENT, 0, 0);
    for (unsigned int index = 0; index < addedAnimations.size(); ++index)
    {
        char label[128];
        if (addedAnimations[index].name.empty())
            sprintf(label, "Animation %u (%u-%u, d%u)", index + 1,
                addedAnimations[index].startFrame, addedAnimations[index].endFrame,
                addedAnimations[index].frameDelta);
        else
            sprintf(label, "%s (%u-%u, d%u)", addedAnimations[index].name.c_str(),
                addedAnimations[index].startFrame, addedAnimations[index].endFrame,
                addedAnimations[index].frameDelta);
        SendDlgItemMessageA(owner, IDC_ANIMATION_LIST, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    }
    if (selected < addedAnimations.size())
        SendDlgItemMessageA(owner, IDC_ANIMATION_LIST, CB_SETCURSEL, selected, 0);
    else
        SendDlgItemMessageA(owner, IDC_ANIMATION_LIST, CB_SETCURSEL, static_cast<WPARAM>(-1), 0);
    EnableWindow(GetDlgItem(owner, IDC_ANIMATION_LIST), !addedAnimations.empty());
    EnableWindow(GetDlgItem(owner, IDC_ANIMATION_ADD), currentModel && currentModel->nbFrames > 0 &&
        (!addedAnimations.empty() || addedAnimations.size() < 12));
    EnableWindow(GetDlgItem(owner, IDC_ANIMATION_DELETE), !addedAnimations.empty());
    const bool hasSelection = selected < addedAnimations.size();
    updatingAnimationName = true;
    SetDlgItemTextA(owner, IDC_ANIMATION_NAME,
        hasSelection ? addedAnimations[selected].name.c_str() : "");
    CheckDlgButton(owner, IDC_ANIMATION_LOOP,
        hasSelection && addedAnimations[selected].loop ? BST_CHECKED : BST_UNCHECKED);
    updatingAnimationName = false;
    EnableWindow(GetDlgItem(owner, IDC_ANIMATION_NAME_LABEL), hasSelection);
    EnableWindow(GetDlgItem(owner, IDC_ANIMATION_NAME), hasSelection);
    EnableWindow(GetDlgItem(owner, IDC_ANIMATION_LOOP), hasSelection);
    EnableWindow(GetDlgItem(owner, IDC_ANIMATION_FRAME_DELTA_LABEL), hasSelection);
    EnableWindow(GetDlgItem(owner, IDC_ANIMATION_FRAME_DELTA), hasSelection);
    if (hasSelection)
        SendDlgItemMessageA(owner, IDC_ANIMATION_FRAME_DELTA, CB_SETCURSEL,
            addedAnimations[selected].frameDelta - 1, 0);
    else
        SendDlgItemMessageA(owner, IDC_ANIMATION_FRAME_DELTA, CB_SETCURSEL, static_cast<WPARAM>(-1), 0);
    EnableWindow(GetDlgItem(owner, IDC_ANIMATION_RESAMPLE), currentModel && currentModel->nbFrames > 1);
    CheckDlgButton(owner, IDC_ANIMATION_RESAMPLE,
        experimentalAnimationResampling ? BST_CHECKED : BST_UNCHECKED);
}

bool readClampedAnimationFrame(HWND owner, int controlId, unsigned int maxFrame,
                               unsigned int * frame, bool * wasClamped)
{
    char text[32] = {};
    GetDlgItemTextA(owner, controlId, text, sizeof(text));
    char * end = NULL;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text) return false;
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end) return false;

    *wasClamped = parsed < 0 || static_cast<unsigned long>(parsed) > maxFrame;
    if (parsed < 0) *frame = 0;
    else if (static_cast<unsigned long>(parsed) > maxFrame) *frame = maxFrame;
    else *frame = static_cast<unsigned int>(parsed);
    return true;
}

bool applyAnimationRange(HWND owner)
{
    if (!currentModel || !currentModel->nbFrames) return false;
    const unsigned int maxFrame = maxPreviewFrame(currentModel);
    unsigned int startFrame = 0;
    unsigned int endFrame = 0;
    bool startClamped = false;
    bool endClamped = false;
    if (!readClampedAnimationFrame(owner, IDC_ANIMATION_START, maxFrame, &startFrame, &startClamped) ||
        !readClampedAnimationFrame(owner, IDC_ANIMATION_END, maxFrame, &endFrame, &endClamped))
    {
        MessageBoxA(owner, "Choose a valid frame range within the model.",
                    "Invalid animation range", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (startFrame > endFrame)
    {
        if (startClamped) endFrame = startFrame;
        else if (endClamped) startFrame = endFrame;
        else
        {
            MessageBoxA(owner, "The start frame must not be after the end frame.",
                        "Invalid animation range", MB_OK | MB_ICONWARNING);
            return false;
        }
    }

    char clampedText[16];
    sprintf(clampedText, "%u", startFrame);
    SetDlgItemTextA(owner, IDC_ANIMATION_START, clampedText);
    sprintf(clampedText, "%u", endFrame);
    SetDlgItemTextA(owner, IDC_ANIMATION_END, clampedText);
    setAnimationPreviewRange(startFrame, endFrame);
    return true;
}

bool chooseAnimationAction(HWND owner, bool * add, unsigned int * replaceIndex)
{
    HMENU menu = CreatePopupMenu();
    if (!menu) return false;
    AppendMenuA(menu, MF_STRING | (addedAnimations.size() >= 12 ? MF_GRAYED : 0), 1, "Add animation");
    if (!addedAnimations.empty()) AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    for (unsigned int index = 0; index < addedAnimations.size(); ++index)
    {
        char label[80];
        const string displayName = addedAnimations[index].name.empty() ?
            "Animation " + std::to_string(index + 1) : addedAnimations[index].name;
        sprintf(label, "Replace %s (%u-%u)", displayName.c_str(),
            addedAnimations[index].startFrame, addedAnimations[index].endFrame);
        AppendMenuA(menu, MF_STRING, 100 + index, label);
    }
    SetMenuDefaultItem(menu, addedAnimations.size() < 12 ? 1 : 100, FALSE);
    RECT button;
    GetWindowRect(GetDlgItem(owner, IDC_ANIMATION_ADD), &button);
    const UINT chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
        button.left, button.bottom, 0, owner, NULL);
    DestroyMenu(menu);
    if (!chosen) return false;
    *add = chosen == 1;
    *replaceIndex = chosen >= 100 ? chosen - 100 : 0;
    return true;
}

void addOrReplaceAnimation(HWND owner)
{
    if (!applyAnimationRange(owner)) return;
    bool add = false;
    unsigned int replaceIndex = 0;
    if (!chooseAnimationAction(owner, &add, &replaceIndex)) return;
    if (add)
        addedAnimations.push_back({PLAYER[0].ANIMCTRL.startFrm,
            static_cast<unsigned int>(PLAYER[0].ANIMCTRL.endFrm - 1), string(), true, 4, 4});
    else if (replaceIndex < addedAnimations.size())
    {
        addedAnimations[replaceIndex].startFrame = PLAYER[0].ANIMCTRL.startFrm;
        addedAnimations[replaceIndex].endFrame = static_cast<unsigned int>(PLAYER[0].ANIMCTRL.endFrm - 1);
    }
    else return;
    const unsigned int selected = add ? static_cast<unsigned int>(addedAnimations.size() - 1) : replaceIndex;
    animationLoopPlayback = addedAnimations[selected].loop;
    refreshAddedAnimations(owner, selected);
    PLAYER[0].ANIMCTRL.frameDelta = activeAnimationFrameDelta();
    PLAYER[0].ANIMCTRL.sourceFrameDelta = activeAnimationStoredDelta();
    setAnimationPreviewRange(addedAnimations[selected].startFrame, addedAnimations[selected].endFrame);
    if (!saveAnimationSidecar())
        MessageBoxA(owner, "Could not save the animation ranges file.",
                    "Animation save failed", MB_OK | MB_ICONERROR);
}

void selectAddedAnimation(HWND owner)
{
    const LRESULT selected = SendDlgItemMessageA(owner, IDC_ANIMATION_LIST, CB_GETCURSEL, 0, 0);
    if (selected < 0 || static_cast<unsigned int>(selected) >= addedAnimations.size()) return;
    const PreviewAnimationRange & range = addedAnimations[static_cast<unsigned int>(selected)];
    animationLoopPlayback = range.loop;
    PLAYER[0].ANIMCTRL.frameDelta = activeAnimationFrameDelta();
    CheckDlgButton(owner, IDC_ANIMATION_LOOP, range.loop ? BST_CHECKED : BST_UNCHECKED);
    updatingAnimationName = true;
    SetDlgItemTextA(owner, IDC_ANIMATION_NAME, range.name.c_str());
    updatingAnimationName = false;
    setAnimationPreviewRange(range.startFrame, range.endFrame);
}

void deleteSelectedAnimation(HWND owner)
{
    const LRESULT selected = SendDlgItemMessageA(owner, IDC_ANIMATION_LIST, CB_GETCURSEL, 0, 0);
    if (selected < 0 || static_cast<unsigned int>(selected) >= addedAnimations.size()) return;
    char question[96];
    sprintf(question, "Delete Animation %u (%u-%u)?", static_cast<unsigned int>(selected) + 1,
        addedAnimations[selected].startFrame, addedAnimations[selected].endFrame);
    if (MessageBoxA(owner, question, "Delete animation", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES)
        return;
    addedAnimations.erase(addedAnimations.begin() + selected);
    if (!saveAnimationSidecar())
        MessageBoxA(owner, "Could not save the animation ranges file.",
                    "Animation save failed", MB_OK | MB_ICONERROR);
    if (addedAnimations.empty())
    {
        PLAYER[0].ANIMCTRL.frameDelta = 4;
        PLAYER[0].ANIMCTRL.sourceFrameDelta = 4;
        setAnimationPreviewRange(0, maxPreviewFrame(currentModel));
        refreshAddedAnimations(owner, 0xFFFFFFFFu);
    }
    else
    {
        const unsigned int next = MIN(static_cast<unsigned int>(selected),
                                      static_cast<unsigned int>(addedAnimations.size() - 1));
        refreshAddedAnimations(owner, next);
        selectAddedAnimation(owner);
    }
}

void resetModelAnimation(animated_model_t * model)
{
    resetAutomaticLod();
    setFbxBonePreview(model, false);
    CheckDlgButton(hwnd, IDC_BONE_PREVIEW, BST_UNCHECKED);
    EnableWindow(GetDlgItem(hwnd, IDC_BONE_PREVIEW), hasFbxBonePreview(model));
    CheckDlgButton(hwnd, IDC_LOD_SHOW, BST_UNCHECKED);
    EnableWindow(GetDlgItem(hwnd, IDC_LOD_SHOW), FALSE);
    EnableWindow(GetDlgItem(hwnd, IDC_LOD_GENERATE), TRUE);
    EnableWindow(GetDlgItem(hwnd, IDC_LOD_MERGE_QUADS), model != NULL);
    ZeroMemory(&PLAYER[0].ANIMCTRL, sizeof(PLAYER[0].ANIMCTRL));
    PLAYER[0].ANIMCTRL.frameDelta = 4;
    PLAYER[0].ANIMCTRL.sourceFrameDelta = 4;
    const unsigned int maxFrame = maxPreviewFrame(model);
    PLAYER[0].ANIMCTRL.endFrm = model && model->nbFrames ? static_cast<uint16_t>(maxFrame + 1) : 0;
    addedAnimations.clear();
    animationLoopPlayback = true;
    experimentalAnimationResampling = false;
    refreshAnimationRangeControls(hwnd, model);
    refreshAddedAnimations(hwnd, 0xFFFFFFFFu);
    playAnimation = 1;
    resetAnimationPlaybackTiming();
    initializeLightSource(model);
    resetSelfShadowing();
    refreshLodControls(hwnd);
}

bool chooseLodGeneration(HWND owner, unsigned int reduction, unsigned int * replaceLevel,
                        bool * replaceBase, bool * restoreBase)
{
    const bool canRestoreBase = hasAutomaticLodBaseSnapshot(currentModel);
    if (!reduction && !canRestoreBase)
    {
        MessageBoxA(owner, "Choose a reduction between 1 and 99%.", "LOD reduction", MB_OK | MB_ICONWARNING);
        return false;
    }
    const unsigned int count = automaticLodLevelCount(currentModel);
    *replaceLevel = 0;
    *replaceBase = false;
    *restoreBase = false;
    unsigned int matching = 0;
    for (unsigned int level = 1; level <= count; ++level)
        if (currentModel->lodData->levels[level - 1].reduction == reduction) matching = level;
    HMENU menu = CreatePopupMenu();
    if (!menu) return false;
    AppendMenuA(menu, MF_STRING | ((!reduction || matching || count >= 63) ? MF_GRAYED : 0), 1, "Add LOD level");
    if (count) AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    for (unsigned int level = 1; level <= count; ++level)
    {
        char label[64];
        sprintf(label, "Replace LOD %u (%u%%)", level, currentModel->lodData->levels[level - 1].reduction);
        AppendMenuA(menu, MF_STRING | ((matching && matching != level) ? MF_GRAYED : 0), level + 1, label);
    }
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING | (!reduction ? MF_GRAYED : 0), 1000, "Replace base with reduced mesh");
    if (canRestoreBase)
        AppendMenuA(menu, MF_STRING, 1001, "Restore original base");
    SetMenuDefaultItem(menu, matching ? matching + 1 : (reduction ? 1 : 1001), FALSE);
    RECT button;
    GetWindowRect(GetDlgItem(owner, IDC_LOD_GENERATE), &button);
    const UINT chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
        button.left, button.bottom, 0, owner, NULL);
    DestroyMenu(menu);
    if (!chosen) return false;
    if (chosen == 1000) *replaceBase = true;
    else if (chosen == 1001) *restoreBase = true;
    else *replaceLevel = chosen - 1;
    return true;
}

void addControl(HWND dialog, const char * className, const char * text, DWORD style,
                int x, int y, int width, int height, int controlId)
{
    CreateWindowExA(0, className, text, WS_CHILD | WS_VISIBLE | style,
                    x, y, width, height, dialog,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                    reinterpret_cast<HINSTANCE>(GetWindowLongPtr(dialog, GWLP_HINSTANCE)), NULL);
}

void updateLightFromSliders(HWND parent)
{
    int azimuth = static_cast<int>(SendDlgItemMessageA(parent, IDC_LIGHT_AZIMUTH, TBM_GETPOS, 0, 0));
    int elevation = static_cast<int>(SendDlgItemMessageA(parent, IDC_LIGHT_ELEVATION, TBM_GETPOS, 0, 0));
    char label[48];
    sprintf(label, "Azimuth: %d deg", azimuth);
    SetDlgItemTextA(parent, IDC_LIGHT_AZIMUTH_LABEL, label);
    sprintf(label, "Elevation: %d deg", elevation);
    SetDlgItemTextA(parent, IDC_LIGHT_ELEVATION_LABEL, label);
    setLightOrientation(static_cast<float>(azimuth), static_cast<float>(elevation));
    if (currentModel) calcLight(currentModel);
    if (comparisonModel) calcLight(comparisonModel);
}

void createSidePanel(HWND parent)
{
    addControl(parent, "BUTTON", "Model", BS_GROUPBOX, 0, 0, 10, 10, IDC_MODEL_GROUP);
    addControl(parent, "BUTTON", "Surface options", BS_GROUPBOX, 0, 0, 10, 10, IDC_SURFACE_GROUP);
    addControl(parent, "BUTTON", "Open another OBJ...", BS_PUSHBUTTON, 0, 0, 10, 10, IDC_OPEN_ANOTHER);
    addControl(parent, "BUTTON", "Load reference...", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 10, 10, IDM_COMPARISON_OPEN);
    addControl(parent, "BUTTON", "Remove reference", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 10, 10, IDM_COMPARISON_REMOVE);
    addControl(parent, "STATIC", "Reference: none", SS_LEFT, 0, 0, 10, 10, IDC_COMPARISON_FILE);
    EnableWindow(GetDlgItem(parent, IDM_COMPARISON_REMOVE), FALSE);
    addControl(parent, "STATIC", "No model loaded", SS_LEFT, 0, 0, 10, 10, IDC_CURRENT_FILE);
    addControl(parent, "STATIC", "Select a polygon to edit", SS_LEFT, 0, 0, 10, 10, IDC_SURFACE_STATUS);
    addControl(parent, "STATIC", "Ctrl+click: add/remove | Ctrl+A: all", SS_LEFT, 0, 0, 10, 10, IDC_SELECTION_HELP);
    addControl(parent, "STATIC", "", SS_BITMAP, 0, 0, 10, 10, IDC_SURFACE_TEXTURE_PREVIEW);
    addControl(parent, "STATIC", "No texture selected", SS_LEFT, 0, 0, 10, 10, IDC_SURFACE_TEXTURE_INFO);
    addControl(parent, "STATIC", "Max size", SS_LEFT, 0, 0, 10, 10, IDC_SURFACE_TEXTURE_SIZE_LABEL);
    addControl(parent, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP, 0, 0, 10, 10,
        IDC_SURFACE_TEXTURE_SIZE);
    for (unsigned int index = 0; index < sizeof(surfaceTextureSizeOptions) / sizeof(surfaceTextureSizeOptions[0]); ++index)
    {
        char label[32];
        sprintf(label, "%u px", surfaceTextureSizeOptions[index]);
        SendDlgItemMessageA(parent, IDC_SURFACE_TEXTURE_SIZE, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(label));
    }
    SendDlgItemMessageA(parent, IDC_SURFACE_TEXTURE_SIZE, CB_SETCURSEL, 0, 0);
    addControl(parent, "BUTTON", "Apply to polygon", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 10, 10,
        IDC_SURFACE_TEXTURE_APPLY);
    EnableWindow(GetDlgItem(parent, IDC_SURFACE_TEXTURE_SIZE), FALSE);
    EnableWindow(GetDlgItem(parent, IDC_SURFACE_TEXTURE_APPLY), FALSE);
    addControl(parent, "BUTTON", "Double-sided", BS_3STATE, 0, 0, 10, 10, IDC_SURFACE_DOUBLE_SIDED);
    addControl(parent, "BUTTON", "Environmental mapping", BS_3STATE, 0, 0, 10, 10, IDC_SURFACE_ENVIRONMENTAL_MAPPING);
    addControl(parent, "BUTTON", "Mesh display", BS_3STATE, 0, 0, 10, 10, IDC_SURFACE_MESH);
    addControl(parent, "BUTTON", "Gouraud shading", BS_3STATE, 0, 0, 10, 10, IDC_SURFACE_GOURAUD);
    addControl(parent, "BUTTON", "Use light", BS_3STATE, 0, 0, 10, 10, IDC_SURFACE_LIGHT);
    addControl(parent, "BUTTON", "Depth shading", BS_3STATE, 0, 0, 10, 10, IDC_SURFACE_DEPTH);
    addControl(parent, "BUTTON", "Use palette", BS_3STATE, 0, 0, 10, 10, IDC_SURFACE_PALETTE);
    addControl(parent, "BUTTON", "Near clipping", BS_3STATE, 0, 0, 10, 10, IDC_SURFACE_NEAR_CLIP);
    addControl(parent, "BUTTON", "Real-time Gouraud", BS_3STATE, 0, 0, 10, 10, IDC_SURFACE_REALTIME_GOURAUD);
    addControl(parent, "BUTTON", "Flip texture", BS_PUSHBUTTON, 0, 0, 10, 10, IDC_SURFACE_FLIP);
    addControl(parent, "BUTTON", "Rotate polygon", BS_PUSHBUTTON, 0, 0, 10, 10, IDC_SURFACE_ROTATE);
    addControl(parent, "BUTTON", "Next sort mode", BS_PUSHBUTTON, 0, 0, 10, 10, IDC_SURFACE_SORT);
    addControl(parent, "STATIC", "Sort mode: SORT_CEN", SS_LEFT, 0, 0, 10, 10, IDC_SORT_STATUS);
    addControl(parent, "STATIC", "Anim speed", SS_LEFT, 0, 0, 10, 10, IDC_ANIMATION_SPEED_LABEL);
    addControl(parent, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP, 0, 0, 10, 10, IDC_ANIMATION_SPEED);
    addControl(parent, "STATIC", "Camera", SS_LEFT, 0, 0, 10, 10, IDC_CAMERA_SPEED_LABEL);
    addControl(parent, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP, 0, 0, 10, 10, IDC_CAMERA_SPEED);
    addControl(parent, "STATIC", "Start", SS_LEFT, 0, 0, 10, 10, IDC_ANIMATION_START_LABEL);
    addControl(parent, "EDIT", "0", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP,
        0, 0, 10, 10, IDC_ANIMATION_START);
    addControl(parent, "STATIC", "End", SS_LEFT, 0, 0, 10, 10, IDC_ANIMATION_END_LABEL);
    addControl(parent, "EDIT", "0", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP,
        0, 0, 10, 10, IDC_ANIMATION_END);
    addControl(parent, "BUTTON", "Apply", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 10, 10, IDC_ANIMATION_RANGE_APPLY);
    addControl(parent, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP, 0, 0, 10, 10, IDC_ANIMATION_LIST);
    addControl(parent, "BUTTON", "Add animation", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 10, 10, IDC_ANIMATION_ADD);
    addControl(parent, "BUTTON", "Delete", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 10, 10, IDC_ANIMATION_DELETE);
    EnableWindow(GetDlgItem(parent, IDC_ANIMATION_START), FALSE);
    EnableWindow(GetDlgItem(parent, IDC_ANIMATION_END), FALSE);
    EnableWindow(GetDlgItem(parent, IDC_ANIMATION_RANGE_APPLY), FALSE);
    EnableWindow(GetDlgItem(parent, IDC_ANIMATION_LIST), FALSE);
    EnableWindow(GetDlgItem(parent, IDC_ANIMATION_ADD), FALSE);
    EnableWindow(GetDlgItem(parent, IDC_ANIMATION_DELETE), FALSE);
    addControl(parent, "STATIC", "Name", SS_LEFT, 0, 0, 10, 10, IDC_ANIMATION_NAME_LABEL);
    addControl(parent, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
        0, 0, 10, 10, IDC_ANIMATION_NAME);
    SendDlgItemMessageA(parent, IDC_ANIMATION_NAME, EM_LIMITTEXT, 48, 0);
    addControl(parent, "BUTTON", "Loop", BS_AUTOCHECKBOX | WS_TABSTOP, 0, 0, 10, 10, IDC_ANIMATION_LOOP);
    EnableWindow(GetDlgItem(parent, IDC_ANIMATION_NAME_LABEL), FALSE);
    EnableWindow(GetDlgItem(parent, IDC_ANIMATION_NAME), FALSE);
    EnableWindow(GetDlgItem(parent, IDC_ANIMATION_LOOP), FALSE);
    addControl(parent, "STATIC", "Frame delta", SS_LEFT, 0, 0, 10, 10,
        IDC_ANIMATION_FRAME_DELTA_LABEL);
    addControl(parent, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 0, 0, 10, 10,
        IDC_ANIMATION_FRAME_DELTA);
    for (unsigned int delta = 1; delta <= 16; ++delta)
    {
        char label[48];
        sprintf(label, "%.4g fps (delta %u)", 60.0 / delta, delta);
        SendDlgItemMessageA(parent, IDC_ANIMATION_FRAME_DELTA, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(label));
    }
    SendDlgItemMessageA(parent, IDC_ANIMATION_FRAME_DELTA, CB_SETCURSEL, 3, 0);
    EnableWindow(GetDlgItem(parent, IDC_ANIMATION_FRAME_DELTA_LABEL), FALSE);
    EnableWindow(GetDlgItem(parent, IDC_ANIMATION_FRAME_DELTA), FALSE);
    addControl(parent, "BUTTON", "Experimental resampling on export",
        BS_AUTOCHECKBOX | WS_TABSTOP, 0, 0, 10, 10, IDC_ANIMATION_RESAMPLE);
    EnableWindow(GetDlgItem(parent, IDC_ANIMATION_RESAMPLE), FALSE);
    addControl(parent, "BUTTON", "Wireframe", BS_AUTOCHECKBOX | WS_TABSTOP, 0, 0, 10, 10, IDC_WIREFRAME);
    addControl(parent, "BUTTON", "Self-shadowing", BS_AUTOCHECKBOX | WS_TABSTOP, 0, 0, 10, 10, IDC_SELF_SHADOWING);
    addControl(parent, "BUTTON", "1-bone preview", BS_AUTOCHECKBOX | WS_TABSTOP, 0, 0, 10, 10, IDC_BONE_PREVIEW);
    EnableWindow(GetDlgItem(parent, IDC_BONE_PREVIEW), FALSE);
    CheckDlgButton(parent, IDC_SELF_SHADOWING, enableSelfShadowing ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(parent, IDC_WIREFRAME, (DisplayTEST & 1) ? BST_CHECKED : BST_UNCHECKED);
    addControl(parent, "STATIC", "Azimuth: 0 deg", SS_LEFT, 0, 0, 10, 10, IDC_LIGHT_AZIMUTH_LABEL);
    addControl(parent, TRACKBAR_CLASSA, "", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, 0, 0, 10, 10, IDC_LIGHT_AZIMUTH);
    addControl(parent, "STATIC", "Elevation: 0 deg", SS_LEFT, 0, 0, 10, 10, IDC_LIGHT_ELEVATION_LABEL);
    addControl(parent, TRACKBAR_CLASSA, "", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, 0, 0, 10, 10, IDC_LIGHT_ELEVATION);
    addControl(parent, "STATIC", "LOD reduction: 75%", SS_LEFT, 0, 0, 10, 10, IDC_LOD_REDUCTION_LABEL);
    addControl(parent, TRACKBAR_CLASSA, "", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, 0, 0, 10, 10, IDC_LOD_REDUCTION);
    SendDlgItemMessageA(parent, IDC_LOD_REDUCTION, TBM_SETRANGE, TRUE, MAKELPARAM(0, 99));
    SendDlgItemMessageA(parent, IDC_LOD_REDUCTION, TBM_SETPOS, TRUE, 75);
    addControl(parent, "BUTTON", "Generate", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 10, 10, IDC_LOD_GENERATE);
    addControl(parent, "BUTTON", "Show LOD", BS_AUTOCHECKBOX | WS_TABSTOP, 0, 0, 10, 10, IDC_LOD_SHOW);
    addControl(parent, "BUTTON", "Delete LOD", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 10, 10, IDC_LOD_DELETE);
    addControl(parent, "BUTTON", "Convert to quads", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 10, 10, IDC_LOD_MERGE_QUADS);
    EnableWindow(GetDlgItem(parent, IDC_LOD_DELETE), FALSE);
    addControl(parent, "STATIC", "Faces: 0\r\nPoints: 0\r\nTextures: 0.0 KiB", SS_LEFT, 0, 0, 10, 10, IDC_LOD_STATUS);
    addControl(parent, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP, 0, 0, 10, 10, IDC_LOD_LEVEL);
    EnableWindow(GetDlgItem(parent, IDC_LOD_LEVEL), FALSE);
    EnableWindow(GetDlgItem(parent, IDC_LOD_SHOW), FALSE);
    EnableWindow(GetDlgItem(parent, IDC_LOD_GENERATE), FALSE);
    EnableWindow(GetDlgItem(parent, IDC_LOD_MERGE_QUADS), FALSE);
    SendDlgItemMessageA(parent, IDC_LIGHT_AZIMUTH, TBM_SETRANGEMIN, FALSE, -360);
    SendDlgItemMessageA(parent, IDC_LIGHT_AZIMUTH, TBM_SETRANGEMAX, FALSE, 360);
    int initialAzimuth = static_cast<int>(std::lround(std::atan2(-defLight[X], -defLight[Z]) / conv));
    int initialElevation = static_cast<int>(std::lround(std::atan2(defLight[Y],
        std::sqrt(defLight[X]*defLight[X] + defLight[Z]*defLight[Z])) / conv));
    SendDlgItemMessageA(parent, IDC_LIGHT_AZIMUTH, TBM_SETPOS, TRUE, initialAzimuth);
    SendDlgItemMessageA(parent, IDC_LIGHT_AZIMUTH, TBM_SETPAGESIZE, 0, 15);
    SendDlgItemMessageA(parent, IDC_LIGHT_ELEVATION, TBM_SETRANGE, TRUE, MAKELPARAM(0, 90));
    SendDlgItemMessageA(parent, IDC_LIGHT_ELEVATION, TBM_SETPOS, TRUE, initialElevation);
    SendDlgItemMessageA(parent, IDC_LIGHT_ELEVATION, TBM_SETPAGESIZE, 0, 5);
    updateLightFromSliders(parent);
    const char * animationLabels[] = {"Paused", "0.25x", "0.5x", "1x", "2x", "4x", "8x"};
    const char * cameraLabels[] = {"0.25x", "0.5x", "1x", "2x", "4x", "8x", "16x"};
    for (unsigned int index = 0; index < 7; ++index)
    {
        SendDlgItemMessageA(parent, IDC_ANIMATION_SPEED, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(animationLabels[index]));
        SendDlgItemMessageA(parent, IDC_CAMERA_SPEED, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(cameraLabels[index]));
    }
    SendDlgItemMessageA(parent, IDC_ANIMATION_SPEED, CB_SETCURSEL, 3, 0);
    SendDlgItemMessageA(parent, IDC_CAMERA_SPEED, CB_SETCURSEL, 2, 0);

    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const int controlIds[] = {
        IDM_COMPARISON_OPEN, IDM_COMPARISON_REMOVE, IDC_COMPARISON_FILE,
        IDC_OPEN_ANOTHER, IDC_CURRENT_FILE, IDC_SURFACE_STATUS, IDC_SELECTION_HELP,
        IDC_SURFACE_TEXTURE_INFO, IDC_SURFACE_TEXTURE_SIZE_LABEL, IDC_SURFACE_TEXTURE_SIZE,
        IDC_SURFACE_TEXTURE_APPLY,
        IDC_SURFACE_DOUBLE_SIDED, IDC_SURFACE_ENVIRONMENTAL_MAPPING, IDC_SURFACE_MESH,
        IDC_SURFACE_GOURAUD, IDC_SURFACE_LIGHT, IDC_SURFACE_DEPTH,
        IDC_SURFACE_PALETTE, IDC_SURFACE_NEAR_CLIP, IDC_SURFACE_REALTIME_GOURAUD,
        IDC_SURFACE_FLIP, IDC_SURFACE_ROTATE, IDC_SURFACE_SORT, IDC_SORT_STATUS,
        IDC_ANIMATION_SPEED_LABEL, IDC_ANIMATION_SPEED, IDC_CAMERA_SPEED_LABEL, IDC_CAMERA_SPEED,
        IDC_ANIMATION_START_LABEL, IDC_ANIMATION_START, IDC_ANIMATION_END_LABEL,
        IDC_ANIMATION_END, IDC_ANIMATION_RANGE_APPLY, IDC_ANIMATION_LIST,
        IDC_ANIMATION_ADD, IDC_ANIMATION_DELETE, IDC_ANIMATION_NAME_LABEL, IDC_ANIMATION_NAME, IDC_ANIMATION_LOOP,
        IDC_ANIMATION_FRAME_DELTA_LABEL, IDC_ANIMATION_FRAME_DELTA, IDC_ANIMATION_RESAMPLE,
        IDC_WIREFRAME, IDC_SELF_SHADOWING, IDC_LIGHT_AZIMUTH_LABEL, IDC_LIGHT_AZIMUTH,
        IDC_LIGHT_ELEVATION_LABEL, IDC_LIGHT_ELEVATION,
        IDC_BONE_PREVIEW,
        IDC_LOD_REDUCTION_LABEL, IDC_LOD_REDUCTION, IDC_LOD_GENERATE, IDC_LOD_SHOW, IDC_LOD_STATUS, IDC_LOD_LEVEL,
        IDC_LOD_DELETE, IDC_LOD_MERGE_QUADS
    };
    for (unsigned int i = 0; i < sizeof(controlIds) / sizeof(controlIds[0]); ++i)
        SendDlgItemMessageA(parent, controlIds[i], WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

void layoutSidePanel(HWND parent)
{
    RECT client;
    GetClientRect(parent, &client);
    const int left = MAX(0, client.right - SIDE_PANEL_WIDTH);
    const int width = SIDE_PANEL_WIDTH - 16;
    struct Placement { int id; int x; int y; int width; int height; };
    const Placement placements[] = {
        {IDC_MODEL_GROUP, 8, 8, width, 116},
        {IDC_SURFACE_GROUP, 8, 130, width, 334},
        {IDC_OPEN_ANOTHER, 20, 29, width - 24, 28},
        {IDC_CURRENT_FILE, 20, 60, width - 24, 28},
        {IDC_SELECTION_HELP, 20, 91, width - 24, 20},
        {IDC_SURFACE_STATUS, 20, 154, width - 24, 26},
        {IDC_SURFACE_TEXTURE_PREVIEW, 20, 182, 96, 68},
        {IDC_SURFACE_TEXTURE_INFO, 124, 182, width - 144, 68},
        {IDC_SURFACE_TEXTURE_SIZE_LABEL, 20, 257, 72, 20},
        {IDC_SURFACE_TEXTURE_SIZE, 92, 254, 96, 150},
        {IDC_SURFACE_TEXTURE_APPLY, 194, 254, width - 218, 24},
        {IDC_SURFACE_DOUBLE_SIDED, 20, 282, (width - 48) / 2, 19},
        {IDC_SURFACE_ENVIRONMENTAL_MAPPING, 20, 301, (width - 48) / 2, 19},
        {IDC_SURFACE_MESH, 20, 320, (width - 48) / 2, 19},
        {IDC_SURFACE_GOURAUD, 20, 339, (width - 48) / 2, 19},
        {IDC_SURFACE_LIGHT, 20, 358, (width - 48) / 2, 19},
        {IDC_SURFACE_DEPTH, 174, 282, (width - 48) / 2, 19},
        {IDC_SURFACE_PALETTE, 174, 301, (width - 48) / 2, 19},
        {IDC_SURFACE_NEAR_CLIP, 174, 320, (width - 48) / 2, 19},
        {IDC_SURFACE_REALTIME_GOURAUD, 174, 339, (width - 48) / 2, 19},
        {IDC_SURFACE_FLIP, 20, 382, (width - 36) / 2, 24},
        {IDC_SURFACE_ROTATE, 24 + (width - 36) / 2, 382, (width - 36) / 2, 24},
        {IDC_SURFACE_SORT, 20, 412, width - 24, 24},
        {IDC_SORT_STATUS, 20, 440, width - 24, 20},
        {IDC_ANIMATION_SPEED_LABEL, 20, 475, 70, 22},
        {IDC_ANIMATION_SPEED, 90, 472, 78, 200},
        {IDC_CAMERA_SPEED_LABEL, 176, 475, 55, 22},
        {IDC_CAMERA_SPEED, 231, 472, 89, 200},
        {IDC_ANIMATION_START_LABEL, 20, 507, 38, 18},
        {IDC_ANIMATION_START, 60, 504, 65, 24},
        {IDC_ANIMATION_END_LABEL, 133, 507, 30, 18},
        {IDC_ANIMATION_END, 165, 504, 65, 24},
        {IDC_ANIMATION_RANGE_APPLY, 240, 504, 80, 24},
        {IDC_ANIMATION_LIST, 20, 535, 142, 160},
        {IDC_ANIMATION_ADD, 168, 535, 96, 24},
        {IDC_ANIMATION_DELETE, 270, 535, 50, 24},
        {IDC_ANIMATION_NAME_LABEL, 168, 565, width - 184, 20},
        {IDC_ANIMATION_NAME, 168, 584, width - 184, 24},
        {IDC_ANIMATION_LOOP, 168, 612, width - 184, 22},
        {IDC_ANIMATION_FRAME_DELTA_LABEL, 20, 640, 84, 20},
        {IDC_ANIMATION_FRAME_DELTA, 106, 637, 92, 160},
        {IDC_ANIMATION_RESAMPLE, 20, 664, width - 24, 22},
        {IDC_WIREFRAME, 20, 690, 146, 22},
        {IDC_BONE_PREVIEW, 174, 690, 146, 22},
        {IDC_SELF_SHADOWING, 20, 714, 300, 22},
        {IDC_LIGHT_AZIMUTH_LABEL, 20, 738, 146, 20},
        {IDC_LIGHT_AZIMUTH, 20, 758, 146, 26},
        {IDC_LIGHT_ELEVATION_LABEL, 174, 738, 146, 20},
        {IDC_LIGHT_ELEVATION, 174, 758, 146, 26},
        {IDC_LOD_REDUCTION_LABEL, 20, 788, 146, 22},
        {IDC_LOD_REDUCTION, 174, 785, 146, 26},
        {IDC_LOD_GENERATE, 20, 814, 66, 26},
        {IDC_LOD_SHOW, 91, 814, 48, 26},
        {IDC_LOD_DELETE, 144, 814, 48, 26},
        {IDC_LOD_MERGE_QUADS, 197, 814, 123, 26},
        {IDC_LOD_STATUS, 20, 843, 146, 48},
        {IDC_LOD_LEVEL, 174, 843, 146, 160},
        {IDM_COMPARISON_OPEN, 20, 893, 146, 26},
        {IDM_COMPARISON_REMOVE, 174, 893, 146, 26},
        {IDC_COMPARISON_FILE, 20, 923, width - 24, 26}
    };
    for (unsigned int i = 0; i < sizeof(placements) / sizeof(placements[0]); ++i)
    {
        HWND control = GetDlgItem(parent, placements[i].id);
        if (control)
            MoveWindow(control, left + placements[i].x, placements[i].y,
                       placements[i].width, placements[i].height, TRUE);
    }
}

void resizeOpenGLView(HWND parent)
{
    RECT client;
    GetClientRect(parent, &client);
    int viewWidth = MAX(1, client.right - SIDE_PANEL_WIDTH);
    int viewHeight = MAX(1, client.bottom);
    glViewport(0, 0, viewWidth, viewHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float persp = 60.0f * 224.0f / 352.0f;
    gluPerspective(persp, static_cast<float>(viewWidth) / viewHeight,
                   188.0f / static_cast<float>(1 << 6), 6553600.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

HBITMAP createSurfaceTexturePreview(const texture_t & texture)
{
    const int previewWidth = 96;
    const int previewHeight = 68;
    if (!texture.pixel || !texture.width || !texture.height ||
        (texture.colorDepth != COL_16 && texture.colorDepth != COL_32K)) return NULL;

    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = previewWidth;
    bitmapInfo.bmiHeader.biHeight = -previewHeight;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    HDC deviceContext = GetDC(hwnd);
    if (!deviceContext) return NULL;
    void * bitmapPixels = NULL;
    HBITMAP bitmap = CreateDIBSection(deviceContext, &bitmapInfo, DIB_RGB_COLORS,
                                      &bitmapPixels, NULL, 0);
    ReleaseDC(hwnd, deviceContext);
    if (!bitmap || !bitmapPixels)
    {
        if (bitmap) DeleteObject(bitmap);
        return NULL;
    }

    unsigned char * destination = static_cast<unsigned char *>(bitmapPixels);
    for (int y = 0; y < previewHeight; ++y)
        for (int x = 0; x < previewWidth; ++x)
        {
            const unsigned char background = ((x / 8 + y / 8) & 1) ? 72 : 104;
            const size_t offset = (static_cast<size_t>(y) * previewWidth + x) * 4;
            destination[offset] = background;
            destination[offset + 1] = background;
            destination[offset + 2] = background;
            destination[offset + 3] = 0;
        }

    const double scale = MIN(static_cast<double>(previewWidth) / texture.width,
                             static_cast<double>(previewHeight) / texture.height);
    const unsigned int drawWidth = MAX(1u, static_cast<unsigned int>(std::lround(texture.width * scale)));
    const unsigned int drawHeight = MAX(1u, static_cast<unsigned int>(std::lround(texture.height * scale)));
    const unsigned int offsetX = (previewWidth - drawWidth) / 2;
    const unsigned int offsetY = (previewHeight - drawHeight) / 2;
    for (unsigned int y = 0; y < drawHeight; ++y)
        for (unsigned int x = 0; x < drawWidth; ++x)
        {
            const unsigned int sourceX = MIN(texture.width - 1,
                static_cast<unsigned int>((uint64_t(x) * 2 + 1) * texture.width / (uint64_t(drawWidth) * 2)));
            const unsigned int sourceY = MIN(texture.height - 1,
                static_cast<unsigned int>((uint64_t(y) * 2 + 1) * texture.height / (uint64_t(drawHeight) * 2)));
            const pixel_t & pixel = texture.pixel[static_cast<size_t>(sourceY) * texture.width + sourceX];
            const uint16_t color = texture.colorDepth == COL_32K ? pixel.rgb :
                texture.clut[MIN(static_cast<unsigned int>(pixel.palette_idx), 15u)];
            const size_t destinationOffset =
                (static_cast<size_t>(offsetY + y) * previewWidth + offsetX + x) * 4;
            destination[destinationOffset] = static_cast<unsigned char>(((color >> 10) & 0x1F) * 255 / 31);
            destination[destinationOffset + 1] = static_cast<unsigned char>(((color >> 5) & 0x1F) * 255 / 31);
            destination[destinationOffset + 2] = static_cast<unsigned char>((color & 0x1F) * 255 / 31);
        }
    return bitmap;
}

bool updateSurfaceTexturePreview(HWND parent, const texture_t * texture)
{
    HBITMAP replacement = texture ? createSurfaceTexturePreview(*texture) : NULL;
    HBITMAP previous = reinterpret_cast<HBITMAP>(SendDlgItemMessageA(parent,
        IDC_SURFACE_TEXTURE_PREVIEW, STM_SETIMAGE, IMAGE_BITMAP,
        reinterpret_cast<LPARAM>(replacement)));
    surfaceTexturePreviewBitmap = replacement;
    if (previous) DeleteObject(previous);
    return !texture || replacement != NULL;
}

bool resizeActiveSurfaceTexture(unsigned int maxDimension, bool * changed, string * error)
{
    *changed = false;
    if (!currentModel || activeModel < 0 || activeModel >= static_cast<int>(currentModel->nbModels) ||
        activeFace < 0 || activeFace >= static_cast<int>(currentModel->model[activeModel].nbPolygon))
    {
        *error = "Select a polygon before changing its texture size.";
        return false;
    }
    if (!currentModel->texture || maxDimension < 8 || maxDimension > 256 || (maxDimension & 7))
    {
        *error = "The selected polygon has no resizable texture.";
        return false;
    }

    polygon_t & polygon = currentModel->model[activeModel].pltbl[activeFace];
    if (polygon.texture < 0 || polygon.texture >= static_cast<int>(currentModel->nbTextures))
    {
        *error = "The selected polygon has no texture.";
        return false;
    }
    texture_t & source = currentModel->texture[polygon.texture];
    if (!source.pixel || !source.width || !source.height ||
        (source.colorDepth != COL_16 && source.colorDepth != COL_32K))
    {
        *error = "The selected texture has invalid pixel data or an unsupported color depth.";
        return false;
    }

    const int sourceTextureIndex = polygon.texture;
    const ATTR originalAttributes = polygon.SGL_ATTR;
    unsigned int textureReferences = 0;
    for (unsigned int mesh = 0; mesh < currentModel->nbModels; ++mesh)
        for (unsigned int face = 0; face < currentModel->model[mesh].nbPolygon; ++face)
            if (currentModel->model[mesh].pltbl[face].texture == sourceTextureIndex)
                ++textureReferences;
    if (currentModel->lodData)
        for (const lod_level_t & level : currentModel->lodData->levels)
            for (const lod_mesh_t & mesh : level.meshes)
                for (const polygon_t & face : mesh.faces)
                    if (face.texture == sourceTextureIndex) ++textureReferences;

    texture_t cachedSource = {};
    if (useUV && getCachedSourceTextureForSurface(currentModel,
            static_cast<unsigned int>(activeModel), static_cast<unsigned int>(activeFace), &cachedSource))
    {
        if ((textureReferences > 1 && currentModel->nbTextures >= 1280) ||
            currentModel->nbTextures >= 2048)
        {
            *error = "The model has reached its texture limit; this surface cannot get a separate texture.";
            return false;
        }
        const unsigned int appendedTextureIndex = currentModel->nbTextures;
        if (!createTextureAtMaxDimension(&polygon, currentModel, cachedSource, maxDimension))
        {
            *error = "Could not regenerate this surface texture from its source image.";
            return false;
        }

        const unsigned int generatedTextureIndex = currentModel->nbTextures - 1;
        texture_t & generated = currentModel->texture[generatedTextureIndex];
        bool identical = generated.width == source.width && generated.height == source.height &&
            generated.colorDepth == source.colorDepth;
        const size_t sourcePixelCount = static_cast<size_t>(source.width) * source.height;
        if (identical)
            identical = std::memcmp(generated.clut, source.clut, sizeof(source.clut)) == 0 &&
                std::memcmp(generated.pixel, source.pixel, sourcePixelCount * sizeof(pixel_t)) == 0;
        if (identical)
        {
            delete [] generated.pixel;
            generated.pixel = NULL;
            currentModel->nbTextures = appendedTextureIndex;
            polygon.texture = sourceTextureIndex;
            polygon.SGL_ATTR = originalAttributes;
            return true;
        }

        const uint16_t generatedColor = generated.SGL_ATTR.colno;
        if (textureReferences == 1)
        {
            texture_t replacement = generated;
            delete [] source.pixel;
            source = replacement;
            source.textureId = static_cast<unsigned int>(sourceTextureIndex);
            source.SGL_ATTR.texno = static_cast<uint16_t>(sourceTextureIndex);
            generated.pixel = NULL;
            currentModel->nbTextures = appendedTextureIndex;
            polygon.texture = sourceTextureIndex;
            polygon.SGL_ATTR = originalAttributes;
            polygon.SGL_ATTR.texno = static_cast<uint16_t>(sourceTextureIndex);
        }
        else
        {
            polygon.SGL_ATTR = originalAttributes;
            polygon.SGL_ATTR.texno = static_cast<uint16_t>(generatedTextureIndex);
            currentModel->texture[generatedTextureIndex].SGL_ATTR.texno =
                static_cast<uint16_t>(generatedTextureIndex);
        }
        polygon.SGL_ATTR.colno = generatedColor;
        *changed = true;
        return true;
    }

    const unsigned int sourceMaxDimension = MAX(source.width, source.height);
    const unsigned int proportionalWidth = static_cast<unsigned int>(
        (uint64_t(source.width) * maxDimension + sourceMaxDimension / 2) / sourceMaxDimension);
    const unsigned int alignedWidth = ((proportionalWidth + 4u) / 8u) * 8u;
    const unsigned int targetWidth = MAX(8u, MIN(maxDimension, alignedWidth));
    const unsigned int targetHeight = MAX(1u, static_cast<unsigned int>(
        (uint64_t(source.height) * maxDimension + sourceMaxDimension / 2) / sourceMaxDimension));
    if (targetWidth == source.width && targetHeight == source.height) return true;

    const size_t targetPixelCount = static_cast<size_t>(targetWidth) * targetHeight;
    pixel_t * resizedPixels = new (std::nothrow) pixel_t[targetPixelCount];
    if (!resizedPixels)
    {
        *error = "Not enough memory to resize this texture.";
        return false;
    }
    for (unsigned int y = 0; y < targetHeight; ++y)
        for (unsigned int x = 0; x < targetWidth; ++x)
        {
            const unsigned int sourceX = MIN(source.width - 1,
                static_cast<unsigned int>((uint64_t(x) * 2 + 1) * source.width / (uint64_t(targetWidth) * 2)));
            const unsigned int sourceY = MIN(source.height - 1,
                static_cast<unsigned int>((uint64_t(y) * 2 + 1) * source.height / (uint64_t(targetHeight) * 2)));
            resizedPixels[static_cast<size_t>(y) * targetWidth + x] =
                source.pixel[static_cast<size_t>(sourceY) * source.width + sourceX];
        }

    if (textureReferences > 1)
    {
        if (currentModel->nbTextures >= 1280)
        {
            delete [] resizedPixels;
            *error = "The model has reached the 1280-texture export limit; this surface cannot get a separate texture.";
            return false;
        }
        const unsigned int newTextureIndex = currentModel->nbTextures;
        texture_t * replacement = new (std::nothrow) texture_t[2048]();
        if (!replacement)
        {
            delete [] resizedPixels;
            *error = "Not enough memory to add a separate surface texture.";
            return false;
        }
        for (unsigned int index = 0; index < currentModel->nbTextures; ++index)
            replacement[index] = currentModel->texture[index];
        replacement[newTextureIndex] = source;
        replacement[newTextureIndex].width = targetWidth;
        replacement[newTextureIndex].height = targetHeight;
        replacement[newTextureIndex].textureId = newTextureIndex;
        replacement[newTextureIndex].pixel = resizedPixels;
        replacement[newTextureIndex].SGL_ATTR.texno = static_cast<uint16_t>(newTextureIndex);

        delete [] currentModel->texture;
        currentModel->texture = replacement;
        ++currentModel->nbTextures;
        polygon.texture = static_cast<int>(newTextureIndex);
        polygon.SGL_ATTR.texno = static_cast<uint16_t>(newTextureIndex);
    }
    else
    {
        delete [] source.pixel;
        source.pixel = resizedPixels;
        source.width = targetWidth;
        source.height = targetHeight;
    }
    *changed = true;
    return true;
}

void updateSurfacePanel(HWND parent, animated_model_t * model)
{
    SetDlgItemTextA(parent, IDC_CURRENT_FILE,
        model && !currentFile.empty() ? currentFile.c_str() : "No model loaded");
    const int editControls[] = {
        IDC_SURFACE_DOUBLE_SIDED, IDC_SURFACE_ENVIRONMENTAL_MAPPING, IDC_SURFACE_MESH,
        IDC_SURFACE_GOURAUD, IDC_SURFACE_LIGHT, IDC_SURFACE_DEPTH,
        IDC_SURFACE_PALETTE, IDC_SURFACE_NEAR_CLIP, IDC_SURFACE_REALTIME_GOURAUD,
        IDC_SURFACE_FLIP, IDC_SURFACE_ROTATE, IDC_SURFACE_SORT
    };
    bool selected = model && selectedSurfaceCount() != 0;
    for (unsigned int i = 0; i < sizeof(editControls) / sizeof(editControls[0]); ++i)
        EnableWindow(GetDlgItem(parent, editControls[i]), selected);

    if (!selected)
    {
        SetDlgItemTextA(parent, IDC_SURFACE_STATUS, "Select a polygon to edit");
        SetDlgItemTextA(parent, IDC_SORT_STATUS, "Sort mode: --");
        SetDlgItemTextA(parent, IDC_SURFACE_TEXTURE_INFO, "Select a textured polygon");
        updateSurfaceTexturePreview(parent, NULL);
        EnableWindow(GetDlgItem(parent, IDC_SURFACE_TEXTURE_SIZE), FALSE);
        EnableWindow(GetDlgItem(parent, IDC_SURFACE_TEXTURE_APPLY), FALSE);
        shownSelectionRevision = surfaceSelectionRevision();
        return;
    }

    char status[96];
    sprintf(status, "Model %d  |  Polygon %d  |  %u selected", activeModel + 1, activeFace + 1, selectedSurfaceCount());
    SetDlgItemTextA(parent, IDC_SURFACE_STATUS, status);
    char sortingStatus[48];
    int sortMode = selectedSortMode(model);
    sprintf(sortingStatus, "Sort mode: %s", sortMode < 0 ? "MIXED" : sortingModeName(sortMode));
    SetDlgItemTextA(parent, IDC_SORT_STATUS, sortingStatus);
    const texture_t * surfaceTexture = NULL;
    if (activeModel >= 0 && activeModel < static_cast<int>(model->nbModels) &&
        activeFace >= 0 && activeFace < static_cast<int>(model->model[activeModel].nbPolygon))
    {
        const polygon_t & polygon = model->model[activeModel].pltbl[activeFace];
        if (model->texture && polygon.texture >= 0 &&
            polygon.texture < static_cast<int>(model->nbTextures))
            surfaceTexture = &model->texture[polygon.texture];
    }
    if (surfaceTexture && surfaceTexture->pixel && surfaceTexture->width && surfaceTexture->height)
    {
        char textureInfo[160];
        const char * colorDepth = surfaceTexture->colorDepth == COL_16 ? "4 bpp palette" :
            surfaceTexture->colorDepth == COL_32K ? "15 bpp RGB" : "Unsupported depth";
        sprintf(textureInfo, "Texture %d\n%u x %u px\n%s",
            model->model[activeModel].pltbl[activeFace].texture,
            surfaceTexture->width, surfaceTexture->height, colorDepth);
        if (!updateSurfaceTexturePreview(parent, surfaceTexture))
            strcat(textureInfo, "\nPreview unavailable");
        SetDlgItemTextA(parent, IDC_SURFACE_TEXTURE_INFO, textureInfo);
        unsigned int currentMaxDimension = MAX(surfaceTexture->width, surfaceTexture->height);
        unsigned int bestIndex = 0;
        unsigned int bestDistance = UINT_MAX;
        for (unsigned int index = 0;
             index < sizeof(surfaceTextureSizeOptions) / sizeof(surfaceTextureSizeOptions[0]); ++index)
        {
            const unsigned int distance = currentMaxDimension > surfaceTextureSizeOptions[index] ?
                currentMaxDimension - surfaceTextureSizeOptions[index] :
                surfaceTextureSizeOptions[index] - currentMaxDimension;
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = index;
            }
        }
        SendDlgItemMessageA(parent, IDC_SURFACE_TEXTURE_SIZE, CB_SETCURSEL, bestIndex, 0);
        const bool resizable = surfaceTexture->colorDepth == COL_16 ||
            surfaceTexture->colorDepth == COL_32K;
        EnableWindow(GetDlgItem(parent, IDC_SURFACE_TEXTURE_SIZE), resizable);
        EnableWindow(GetDlgItem(parent, IDC_SURFACE_TEXTURE_APPLY), resizable);
    }
    else
    {
        SetDlgItemTextA(parent, IDC_SURFACE_TEXTURE_INFO,
            surfaceTexture ? "Texture pixel data is unavailable" : "Selected polygon has no texture");
        updateSurfaceTexturePreview(parent, NULL);
        EnableWindow(GetDlgItem(parent, IDC_SURFACE_TEXTURE_SIZE), FALSE);
        EnableWindow(GetDlgItem(parent, IDC_SURFACE_TEXTURE_APPLY), FALSE);
    }
    const int optionControls[] = {
        IDC_SURFACE_DOUBLE_SIDED, IDC_SURFACE_ENVIRONMENTAL_MAPPING, IDC_SURFACE_MESH,
        IDC_SURFACE_GOURAUD, IDC_SURFACE_LIGHT, IDC_SURFACE_DEPTH,
        IDC_SURFACE_PALETTE, IDC_SURFACE_NEAR_CLIP, IDC_SURFACE_REALTIME_GOURAUD
    };
    const surface_edit_operation operations[] = {
        SURFACE_TOGGLE_DOUBLE_SIDED, SURFACE_TOGGLE_ENVIRONMENTAL_MAPPING, SURFACE_TOGGLE_MESH,
        SURFACE_TOGGLE_GOURAUD, SURFACE_TOGGLE_LIGHT, SURFACE_TOGGLE_DEPTH,
        SURFACE_TOGGLE_PALETTE, SURFACE_TOGGLE_NEAR_CLIP, SURFACE_TOGGLE_REALTIME_GOURAUD
    };
    for (unsigned int i = 0; i < sizeof(optionControls) / sizeof(optionControls[0]); ++i)
    {
        int state = selectedSurfaceOptionState(model, operations[i]);
        CheckDlgButton(parent, optionControls[i], state == 1 ? BST_CHECKED :
                       state == -1 ? BST_INDETERMINATE : BST_UNCHECKED);
    }
    shownSelectionRevision = surfaceSelectionRevision();
}

bool handleSurfaceCommand(HWND parent, int controlId)
{
    surface_edit_operation operation;
    switch (controlId)
    {
        case IDC_SURFACE_DOUBLE_SIDED: operation = SURFACE_TOGGLE_DOUBLE_SIDED; break;
        case IDC_SURFACE_ENVIRONMENTAL_MAPPING: operation = SURFACE_TOGGLE_ENVIRONMENTAL_MAPPING; break;
        case IDC_SURFACE_MESH: operation = SURFACE_TOGGLE_MESH; break;
        case IDC_SURFACE_GOURAUD: operation = SURFACE_TOGGLE_GOURAUD; break;
        case IDC_SURFACE_LIGHT: operation = SURFACE_TOGGLE_LIGHT; break;
        case IDC_SURFACE_DEPTH: operation = SURFACE_TOGGLE_DEPTH; break;
        case IDC_SURFACE_PALETTE: operation = SURFACE_TOGGLE_PALETTE; break;
        case IDC_SURFACE_NEAR_CLIP: operation = SURFACE_TOGGLE_NEAR_CLIP; break;
        case IDC_SURFACE_REALTIME_GOURAUD: operation = SURFACE_TOGGLE_REALTIME_GOURAUD; break;
        case IDC_SURFACE_FLIP: operation = SURFACE_CYCLE_TEXTURE_FLIP; break;
        case IDC_SURFACE_ROTATE: operation = SURFACE_ROTATE; break;
        case IDC_SURFACE_SORT: operation = SURFACE_CYCLE_SORTING; break;
        default: return false;
    }
    const uint32_t previousRevision = surfaceEditRevision();
    if (controlId >= IDC_SURFACE_DOUBLE_SIDED && controlId <= IDC_SURFACE_REALTIME_GOURAUD)
        setSelectedSurfaceOption(currentModel, operation, selectedSurfaceOptionState(currentModel, operation) != 1);
    else
        editSelectedSurface(currentModel, operation);
    if (surfaceEditRevision() != previousRevision && !skinVariants.empty())
    {
        if (operation == SURFACE_CYCLE_TEXTURE_FLIP || operation == SURFACE_ROTATE)
            rebuildSkinVariants(parent);
        else
            syncSkinSurfaceAttributes();
    }
    updateSurfacePanel(parent, currentModel);
    return true;
}

INT_PTR CALLBACK OptionsDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    LoadOptionsDialogContext * context = reinterpret_cast<LoadOptionsDialogContext *>(
        GetWindowLongPtr(dialog, GWLP_USERDATA));
    LoadOptions * options = context ? context->options : NULL;
    if (message == WM_INITDIALOG)
    {
        context = reinterpret_cast<LoadOptionsDialogContext *>(lParam);
        options = context->options;
        SetWindowLongPtr(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));
        const int frameOffset = context->showFrameCount ? 0 : 28;
        addControl(dialog, "STATIC", "Animation frames", 0, 10, 12, 120, 18, IDC_FRAME_COUNT_LABEL);
        addControl(dialog, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL, 138, 10, 100, 22, IDC_FRAME_COUNT);
        if (!context->showFrameCount)
        {
            ShowWindow(GetDlgItem(dialog, IDC_FRAME_COUNT_LABEL), SW_HIDE);
            ShowWindow(GetDlgItem(dialog, IDC_FRAME_COUNT), SW_HIDE);
        }
        addControl(dialog, "STATIC", "SGL mode", 0, 10, 40 - frameOffset, 120, 18, -1);
        HWND sglCombo = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST,
                        138, 38 - frameOffset, 150, 80, dialog,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SGL_MODE)),
                                        reinterpret_cast<HINSTANCE>(GetWindowLongPtr(dialog, GWLP_HINSTANCE)), NULL);
        SendMessageA(sglCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Disabled"));
        SendMessageA(sglCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Enabled"));
        SendMessageA(sglCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Palette export"));
        addControl(dialog, "STATIC", "Scale", 0, 10, 68 - frameOffset, 120, 18, -1);
        addControl(dialog, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL, 138, 66 - frameOffset, 100, 22, IDC_SCALE);
        const int rotationIds[] = {IDC_ROTATION_X, IDC_ROTATION_Y, IDC_ROTATION_Z};
        const char *rotationLabels[] = {"Rotate X", "Rotate Y", "Rotate Z"};
        const int rotationValues[] = {options->rotationX, options->rotationY, options->rotationZ};
        char value[64];
        for (int axis = 0; axis < 3; ++axis)
        {
            const int y = 96 + axis * 24 - frameOffset;
            addControl(dialog, "STATIC", rotationLabels[axis], 0, 10, y + 2, 120, 18, -1);
            HWND rotationCombo = CreateWindowExA(0, "COMBOBOX", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST,
                138, y, 110, 100, dialog,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(rotationIds[axis])),
                reinterpret_cast<HINSTANCE>(GetWindowLongPtr(dialog, GWLP_HINSTANCE)), NULL);
            SendMessageA(rotationCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("0 degrees"));
            SendMessageA(rotationCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("90 degrees"));
            SendMessageA(rotationCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("180 degrees"));
            SendMessageA(rotationCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("270 degrees"));
            SendMessageA(rotationCombo, CB_SETCURSEL, rotationValues[axis], 0);
        }
        const int translationIds[] = {IDC_TRANSLATION_X, IDC_TRANSLATION_Y, IDC_TRANSLATION_Z};
        const char *translationLabels[] = {"Translate X", "Translate Y", "Translate Z"};
        const float translationValues[] = {options->translationX, options->translationY, options->translationZ};
        for (int axis = 0; axis < 3; ++axis)
        {
            const int y = 180 + axis * 24 - frameOffset;
            addControl(dialog, "STATIC", translationLabels[axis], 0, 10, y + 2, 120, 18, -1);
            addControl(dialog, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL, 138, y, 100, 22, translationIds[axis]);
            sprintf(value, "%.6g", translationValues[axis]);
            SetDlgItemTextA(dialog, translationIds[axis], value);
        }
        addControl(dialog, "BUTTON", "Generate textures from UV", BS_AUTOCHECKBOX, 10, 258 - frameOffset, 240, 20, IDC_USE_UV);
        addControl(dialog, "BUTTON", "Force 4 BPP", BS_AUTOCHECKBOX, 10, 282 - frameOffset, 160, 20, IDC_FORCE_4BPP);
        addControl(dialog, "BUTTON", "Power-of-8 UV texture height", BS_AUTOCHECKBOX, 10, 306 - frameOffset, 250, 20, IDC_POWER_OF_8);
        addControl(dialog, "STATIC", "Max texture width", 0, 10, 332 - frameOffset, 120, 18, -1);
        addControl(dialog, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL, 138, 330 - frameOffset, 100, 22, IDC_MAX_TEXTURE_WIDTH);
        addControl(dialog, "STATIC", "Max texture height", 0, 10, 356 - frameOffset, 120, 18, -1);
        addControl(dialog, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL, 138, 354 - frameOffset, 100, 22, IDC_MAX_TEXTURE_HEIGHT);
        addControl(dialog, "BUTTON", "OK", BS_DEFPUSHBUTTON, 108, 390 - frameOffset, 70, 24, IDOK);
        addControl(dialog, "BUTTON", "Cancel", 0, 184, 390 - frameOffset, 70, 24, IDCANCEL);

        if (context->showFrameCount)
        {
            sprintf(value, "%d", options->frameCount);
            SetDlgItemTextA(dialog, IDC_FRAME_COUNT, value);
        }
        sprintf(value, "%.6g", options->scale);
        SetDlgItemTextA(dialog, IDC_SCALE, value);
        SendDlgItemMessageA(dialog, IDC_SGL_MODE, CB_SETCURSEL, options->sglMode, 0);
        CheckDlgButton(dialog, IDC_USE_UV, options->useUv ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(dialog, IDC_FORCE_4BPP, (options->specialOptions & 1) ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(dialog, IDC_POWER_OF_8, (options->specialOptions & 2) ? BST_CHECKED : BST_UNCHECKED);
        sprintf(value, "%u", options->maxTextureWidth);
        SetDlgItemTextA(dialog, IDC_MAX_TEXTURE_WIDTH, value);
        sprintf(value, "%u", options->maxTextureHeight);
        SetDlgItemTextA(dialog, IDC_MAX_TEXTURE_HEIGHT, value);
        return TRUE;
    }
    if (message == WM_COMMAND && options)
    {
        if (LOWORD(wParam) == IDOK)
        {
            char frameText[32] = {};
            char scaleText[64] = {};
            char translationXText[64] = {};
            char translationYText[64] = {};
            char translationZText[64] = {};
            char maxTextureWidthText[32] = {};
            char maxTextureHeightText[32] = {};
            if (context->showFrameCount)
                GetDlgItemTextA(dialog, IDC_FRAME_COUNT, frameText, sizeof(frameText));
            GetDlgItemTextA(dialog, IDC_SCALE, scaleText, sizeof(scaleText));
            GetDlgItemTextA(dialog, IDC_TRANSLATION_X, translationXText, sizeof(translationXText));
            GetDlgItemTextA(dialog, IDC_TRANSLATION_Y, translationYText, sizeof(translationYText));
            GetDlgItemTextA(dialog, IDC_TRANSLATION_Z, translationZText, sizeof(translationZText));
            GetDlgItemTextA(dialog, IDC_MAX_TEXTURE_WIDTH, maxTextureWidthText, sizeof(maxTextureWidthText));
            GetDlgItemTextA(dialog, IDC_MAX_TEXTURE_HEIGHT, maxTextureHeightText, sizeof(maxTextureHeightText));
            const int rotationX = static_cast<int>(SendDlgItemMessageA(dialog, IDC_ROTATION_X, CB_GETCURSEL, 0, 0));
            const int rotationY = static_cast<int>(SendDlgItemMessageA(dialog, IDC_ROTATION_Y, CB_GETCURSEL, 0, 0));
            const int rotationZ = static_cast<int>(SendDlgItemMessageA(dialog, IDC_ROTATION_Z, CB_GETCURSEL, 0, 0));
            char * frameEnd = frameText;
            char * scaleEnd = NULL;
            char * translationXEnd = NULL;
            char * translationYEnd = NULL;
            char * translationZEnd = NULL;
            char * maxTextureWidthEnd = NULL;
            char * maxTextureHeightEnd = NULL;
            long frameCount = options->frameCount;
            if (context->showFrameCount) frameCount = std::strtol(frameText, &frameEnd, 10);
            float scale = std::strtof(scaleText, &scaleEnd);
            float translationX = std::strtof(translationXText, &translationXEnd);
            float translationY = std::strtof(translationYText, &translationYEnd);
            float translationZ = std::strtof(translationZText, &translationZEnd);
            unsigned long maxTextureWidth = std::strtoul(maxTextureWidthText, &maxTextureWidthEnd, 10);
            unsigned long maxTextureHeight = std::strtoul(maxTextureHeightText, &maxTextureHeightEnd, 10);
            int sglMode = static_cast<int>(SendDlgItemMessageA(dialog, IDC_SGL_MODE, CB_GETCURSEL, 0, 0));
            if ((context->showFrameCount && (frameEnd == frameText || *frameEnd != '\0' || frameCount < 0 ||
                frameCount > static_cast<long>(ZTP_MAX_ANIMATION_FRAMES))) ||
                scaleEnd == scaleText || *scaleEnd != '\0' || !std::isfinite(scale) || scale <= 0.0f ||
                translationXEnd == translationXText || *translationXEnd != '\0' || !std::isfinite(translationX) ||
                translationYEnd == translationYText || *translationYEnd != '\0' || !std::isfinite(translationY) ||
                translationZEnd == translationZText || *translationZEnd != '\0' || !std::isfinite(translationZ) ||
                maxTextureWidthEnd == maxTextureWidthText || *maxTextureWidthEnd != '\0' ||
                maxTextureWidth < 8 || maxTextureWidth > 65535 ||
                maxTextureHeightEnd == maxTextureHeightText || *maxTextureHeightEnd != '\0' ||
                maxTextureHeight < 1 || maxTextureHeight > 256 ||
                ((IsDlgButtonChecked(dialog, IDC_POWER_OF_8) == BST_CHECKED) && maxTextureHeight < 8) ||
                sglMode < 0 || sglMode > 2 || rotationX < 0 || rotationX > 3 ||
                rotationY < 0 || rotationY > 3 || rotationZ < 0 || rotationZ > 3)
            {
                MessageBoxA(dialog, "Enter valid frame, scale, translation and texture dimensions. Width must be at least 8; height must be 1-256 (at least 8 with power-of-8 enabled).",
                    "Invalid options", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            if (context->showFrameCount) options->frameCount = static_cast<int>(frameCount);
            options->sglMode = sglMode;
            options->scale = scale;
            options->useUv = IsDlgButtonChecked(dialog, IDC_USE_UV) == BST_CHECKED;
            options->specialOptions = (IsDlgButtonChecked(dialog, IDC_FORCE_4BPP) == BST_CHECKED ? 1u : 0u) |
                                      (IsDlgButtonChecked(dialog, IDC_POWER_OF_8) == BST_CHECKED ? 2u : 0u);
            options->maxTextureWidth = static_cast<uint32_t>(maxTextureWidth);
            options->maxTextureHeight = static_cast<uint32_t>(maxTextureHeight);
            options->rotationX = rotationX;
            options->rotationY = rotationY;
            options->rotationZ = rotationZ;
            options->translationX = translationX;
            options->translationY = translationY;
            options->translationZ = translationZ;
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}

bool showOptionsDialog(HWND owner, LoadOptions * options, bool showFrameCount = true)
{
    struct DialogTemplate
    {
        DLGTEMPLATE dialog;
        WORD menu;
        WORD windowClass;
        char title[24];
    };
    DialogTemplate templateData = {};
    templateData.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER;
    templateData.dialog.cdit = 0;
    templateData.dialog.x = 10;
    templateData.dialog.y = 10;
    templateData.dialog.cx = 280;
    templateData.dialog.cy = showFrameCount ? 432 : 404;
    lstrcpyA(templateData.title, "Converter options");
    LoadOptionsDialogContext context = {options, showFrameCount};
    return DialogBoxIndirectParamA(GetModuleHandle(NULL), &templateData.dialog, owner,
                                   OptionsDialogProc, reinterpret_cast<LPARAM>(&context)) == IDOK;
}

void releaseModelTextures(animated_model_t * model)
{
    if (!model) return;
    if (model->nbTextures) glDeleteTextures(model->nbTextures, texId);
    glDeleteTextures(256, specialTextureId);
    glDeleteTextures(1, &asciiTextureId);
}

void destroySkinVariant(SkinVariant * skin, bool destroyAtlas)
{
    if (!skin) return;
    if (glReady && !skin->textureIds.empty())
        glDeleteTextures(static_cast<GLsizei>(skin->textureIds.size()), skin->textureIds.data());
    skin->textureIds.clear();
    destroySkinVariantModel(skin->model);
    skin->model = NULL;
    if (destroyAtlas)
    {
        delete [] skin->atlas.pixel;
        skin->atlas.pixel = NULL;
    }
}

void clearSkinVariants()
{
    for (unsigned int index = 0; index < skinVariants.size(); ++index)
        destroySkinVariant(&skinVariants[index], true);
    skinVariants.clear();
    previewSkinIndex = -1;
}

bool bakeSkinVariant(SkinVariant * skin, string * error)
{
    if (!currentModel || currentModelIsZTP || !useUV)
    {
        if (error) *error = "Skin atlases require an OBJ or FBX model loaded with UVs.";
        return false;
    }
    destroySkinVariant(skin, false);
    skin->model = createSkinVariantModel(currentModel, skin->atlas, error);
    if (!skin->model) return false;
    try
    {
        skin->textureIds.assign(skin->model->nbTextures, 0);
        if (glReady) setModelTextures(skin->model, skin->textureIds.data());
    }
    catch (const std::bad_alloc &)
    {
        if (error) *error = "There is not enough memory to upload the skin textures.";
        destroySkinVariant(skin, false);
        return false;
    }
    return true;
}

void rebuildSkinVariants(HWND owner)
{
    const int selectedSkin = previewSkinIndex;
    for (unsigned int index = 0; index < skinVariants.size(); ++index)
    {
        string error;
        if (!bakeSkinVariant(&skinVariants[index], &error))
        {
            previewSkinIndex = -1;
            updateSkinMenu(owner);
            MessageBoxA(owner, error.c_str(), "Skin regeneration failed", MB_OK | MB_ICONERROR);
            InvalidateRect(owner, NULL, FALSE);
            return;
        }
    }
    previewSkinIndex = selectedSkin >= 0 && selectedSkin < static_cast<int>(skinVariants.size()) ?
        selectedSkin : -1;
    updateSkinMenu(owner);
    InvalidateRect(owner, NULL, FALSE);
}

void syncSkinSurfaceAttributes()
{
    if (!currentModel) return;
    for (unsigned int skinIndex = 0; skinIndex < skinVariants.size(); ++skinIndex)
    {
        animated_model_t * variant = skinVariants[skinIndex].model;
        if (!variant || variant->nbModels != currentModel->nbModels) continue;
        for (unsigned int mesh = 0; mesh < currentModel->nbModels; ++mesh)
        {
            const unsigned int faceCount = MIN(currentModel->model[mesh].nbPolygon,
                                                variant->model[mesh].nbPolygon);
            for (unsigned int face = 0; face < faceCount; ++face)
            {
                polygon_t & skinPolygon = variant->model[mesh].pltbl[face];
                const ATTR textureAttributes = skinPolygon.SGL_ATTR;
                skinPolygon.SGL_ATTR = currentModel->model[mesh].pltbl[face].SGL_ATTR;
                skinPolygon.SGL_ATTR.texno = textureAttributes.texno;
                skinPolygon.SGL_ATTR.colno = textureAttributes.colno;
                skinPolygon.SGL_ATTR.dir = textureAttributes.dir;
            }
        }
    }
}

void loadSkinAtlas(HWND owner)
{
    if (!currentModel) return;
    if (currentModelIsZTP || !useUV)
    {
        MessageBoxA(owner, "Skin atlas generation is available only for OBJ/FBX models loaded with UVs.",
                    "Skin atlas unavailable", MB_OK | MB_ICONINFORMATION);
        return;
    }
    char filePath[MAX_PATH] = {};
    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrTitle = "Load complete skin atlas";
    dialog.lpstrFilter = "Texture images (*.tga;*.png)\0*.tga;*.png\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = filePath;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&dialog)) return;

    SkinVariant skinVariant;
    if (ReadTGAFile("", &skinVariant.atlas, filePath) <= 0 || !skinVariant.atlas.pixel)
    {
        delete [] skinVariant.atlas.pixel;
        skinVariant.atlas.pixel = NULL;
        MessageBoxA(owner, "Could not load this skin image. Choose a valid TGA or PNG file.",
                    "Skin load failed", MB_OK | MB_ICONERROR);
        return;
    }
    const uint64_t pixelCount = uint64_t(skinVariant.atlas.width) * skinVariant.atlas.height;
    if (!skinVariant.atlas.width || !skinVariant.atlas.height || skinVariant.atlas.width > 65535 ||
        skinVariant.atlas.height > 65535 || pixelCount > 16u * 1024u * 1024u ||
        (skinVariant.atlas.colorDepth == COL_16 && (pixelCount & 1u)))
    {
        delete [] skinVariant.atlas.pixel;
        skinVariant.atlas.pixel = NULL;
        MessageBoxA(owner, "The skin atlas dimensions exceed the supported limits.",
                    "Skin load failed", MB_OK | MB_ICONWARNING);
        return;
    }
    skinVariant.atlas.name = filePath;
    skinVariant.atlas.textureId = 0;
    skinVariant.name = filePath;
    const size_t separator = skinVariant.name.find_last_of("\\/");
    if (separator != string::npos) skinVariant.name = skinVariant.name.substr(separator + 1);
    const size_t extension = skinVariant.name.find_last_of('.');
    if (extension != string::npos) skinVariant.name.resize(extension);

    string error;
    if (!bakeSkinVariant(&skinVariant, &error))
    {
        destroySkinVariant(&skinVariant, true);
        MessageBoxA(owner, error.c_str(), "Skin generation failed", MB_OK | MB_ICONERROR);
        return;
    }
    try
    {
        skinVariants.push_back(skinVariant);
    }
    catch (const std::bad_alloc &)
    {
        destroySkinVariant(&skinVariant, true);
        MessageBoxA(owner, "There is not enough memory to add this skin.",
                    "Skin load failed", MB_OK | MB_ICONERROR);
        return;
    }
    previewSkinIndex = static_cast<int>(skinVariants.size()) - 1;
    updateSkinMenu(owner);
    InvalidateRect(owner, NULL, FALSE);
}
bool loadModel(const string & folder, const string & fileName, HWND owner, bool isObj = false)
{
    LoadOptions selectedOptions = loadOptions;
    if (!showOptionsDialog(owner, &selectedOptions, isObj)) return false;
    if (!confirmZTPSave(owner)) return false;

    animated_model_t * nextModel = loadingConfigured(selectedOptions.frameCount, selectedOptions.sglMode,
        selectedOptions.scale, selectedOptions.useUv ? 1u : 0u, selectedOptions.specialOptions,
        folder, fileName, false, selectedOptions.rotationX, selectedOptions.rotationY, selectedOptions.rotationZ,
        selectedOptions.translationX, selectedOptions.translationY, selectedOptions.translationZ,
        selectedOptions.maxTextureWidth, selectedOptions.maxTextureHeight);
    if (!nextModel || !nextModel->nbModels)
    {
        string error = fbxLoadError.empty() ? "Could not read this model." : fbxLoadError;
        MessageBoxA(owner, error.c_str(), "Model load failed", MB_OK | MB_ICONERROR);
        return false;
    }
    clearSkinVariants();
    delete [] skin.pixel;
    skin.pixel = NULL;
    releaseModelTextures(currentModel);
    deleteModels(currentModel);
    currentModel = nextModel;
    loadOptions = selectedOptions;
    currentFolder = folder;
    currentFile = fileName;
    currentModelIsZTP = false;
    currentModelIsObj = isObj;
    ztpBaseReplaced = false;
    ztpBasePose.clear();
    ztpSavePath.clear();
    convertedModelSavePath.clear();
    updateFileMenu(owner);
    updateSkinMenu(owner);
    clearSelectedSurfaces();
    setTextures(currentModel);
    for (int player = 0; player < 4; ++player) resetPlayer(&PLAYER[player]);
    resetModelAnimation(currentModel);
    loadAnimationSidecar(currentModel);
    SetWindowTextA(owner, ("ZTP Converter - " + fileName).c_str());
    updateSurfacePanel(owner, currentModel);
    return true;
}

bool loadZTPModel(const string &path, const string &fileName, HWND owner)
{
    if (!confirmZTPSave(owner)) return false;
    animated_model_t *nextModel = loadZTPFile(path);
    if (!nextModel)
    {
        MessageBoxA(owner, "Could not read this ZTP model (legacy or ZTP_Z01/ZTP_S01).", "ZTP load failed",
                    MB_OK | MB_ICONERROR);
        return false;
    }
    clearSkinVariants();
    releaseModelTextures(currentModel);
    deleteModels(currentModel);
    currentModel = nextModel;
    currentFolder = path;
    currentFile = fileName;
    currentModelIsZTP = true;
    currentModelIsObj = false;
    ztpBaseReplaced = false;
    convertedModelSavePath.clear();
    ztpSavePath = absoluteModelPath(path);
    currentFolder = ztpSavePath;
    ztpSaved = false;
    savedEditRevision = surfaceEditRevision();
    savedLodRevision = currentModel->lodData ? currentModel->lodData->revision : 0;
    ztpBasePose.resize(currentModel->nbModels);
    for (unsigned int mesh = 0; mesh < currentModel->nbModels; ++mesh)
        ztpBasePose[mesh].assign(currentModel->model[mesh].pntbl,
            currentModel->model[mesh].pntbl + currentModel->model[mesh].nbPoint);
    updateFileMenu(owner);
    updateSkinMenu(owner);
    USE_SGL = currentModel->sourceSgl;
    fileOutName = fileName.substr(0, fileName.find_last_of('.'));
    if (fileOutName.empty()) fileOutName = "model";
    clearSelectedSurfaces();
    setTextures(currentModel);
    for (int player = 0; player < 4; ++player) resetPlayer(&PLAYER[player]);
    resetModelAnimation(currentModel);
    loadAnimationSidecar(currentModel);
    SetWindowTextA(owner, ("ZTP Converter - " + fileName).c_str());
    updateSurfacePanel(owner, currentModel);
    return true;
}

bool hasBasePoseAsFirstAnimationFrame()
{
    if (currentModelIsZTP) return false;
    if (!currentModelIsObj) return true;

    string fileName = currentFile;
    const size_t extension = fileName.find_last_of('.');
    if (extension != string::npos && _stricmp(fileName.substr(extension).c_str(), ".obj") == 0)
        fileName.resize(extension);
    const auto endsWith = [&](const char * suffix) {
        const size_t suffixLength = std::strlen(suffix);
        return fileName.size() >= suffixLength &&
            fileName.compare(fileName.size() - suffixLength, suffixLength, suffix) == 0;
    };
    return endsWith("_000001") || endsWith("000001") || endsWith("0000");
}

bool buildAnimationExport(animated_model_t * source, animated_model_t * output,
                          std::vector<std::vector<vertex_t> > * sampledFrames,
                          std::vector<key_frame_t> * frameDescriptors,
                          std::vector<PreviewAnimationRange> * outputAnimations,
                          std::vector<PreviewAnimationRange> * outputMemoryAnimations,
                          bool filterToAnimations, bool resampleAnimations,
                          string * error)
{
    *output = *source;
    *outputAnimations = addedAnimations;
    *outputMemoryAnimations = addedAnimations;
    const bool omitBasePoseFrame = filterToAnimations && hasBasePoseAsFirstAnimationFrame();
    const unsigned int firstAnimationFrame = omitBasePoseFrame ? 1u : 0u;
    if (omitBasePoseFrame)
        for (unsigned int index = 0; index < addedAnimations.size(); ++index)
        {
            PreviewAnimationRange & memoryAnimation = (*outputMemoryAnimations)[index];
            if (memoryAnimation.startFrame < firstAnimationFrame)
                memoryAnimation.startFrame = firstAnimationFrame;
            if (memoryAnimation.startFrame > memoryAnimation.endFrame)
            {
                memoryAnimation.startFrame = 1;
                memoryAnimation.endFrame = 0;
                (*outputAnimations)[index].startFrame = 1;
                (*outputAnimations)[index].endFrame = 0;
            }
        }
    if (!filterToAnimations) return true;
    if (addedAnimations.empty())
    {
        if (omitBasePoseFrame)
        {
            output->nbFrames = 0;
            output->keyFrames = NULL;
        }
        return true;
    }

    std::vector<unsigned int> ordered(addedAnimations.size());
    for (unsigned int index = 0; index < ordered.size(); ++index) ordered[index] = index;
    std::sort(ordered.begin(), ordered.end(), [](unsigned int first, unsigned int second) {
        return addedAnimations[first].startFrame < addedAnimations[second].startFrame;
    });
    for (unsigned int index = 0; index < ordered.size(); ++index)
    {
        const PreviewAnimationRange & animation = addedAnimations[ordered[index]];
        if (animation.startFrame > animation.endFrame || animation.endFrame >= source->nbFrames ||
            animation.frameDelta < 1 || animation.frameDelta > 16 ||
            animation.storedDelta < 1 || animation.storedDelta > 16)
        {
            *error = "An animation range or frame delta is invalid; no ZTP file was changed.";
            return false;
        }
        if (!source->keyFrames)
        {
            *error = "The source animation has no keyframe data.";
            return false;
        }
        for (unsigned int frame = animation.startFrame; frame <= animation.endFrame; ++frame)
            if (!source->keyFrames[frame].cVert)
            {
                *error = "An animation range references a missing keyframe.";
                return false;
            }
        if (resampleAnimations && index &&
            animation.startFrame <= addedAnimations[ordered[index - 1]].endFrame)
        {
            *error = "Experimental export requires non-overlapping animation ranges.";
            return false;
        }
    }

    std::vector<animationFrameSample_t> samples;
    std::vector<unsigned int> mappedStarts(addedAnimations.size());
    std::vector<unsigned int> mappedEnds(addedAnimations.size());
    if (!resampleAnimations)
    {
        std::vector<bool> included(source->nbFrames, false);
        for (const PreviewAnimationRange & animation : addedAnimations)
            for (unsigned int frame = MAX(animation.startFrame, firstAnimationFrame);
                 frame <= animation.endFrame; ++frame)
                included[frame] = true;
        std::vector<unsigned int> remap(source->nbFrames, 0xFFFFFFFFu);
        for (unsigned int frame = 0; frame < included.size(); ++frame)
            if (included[frame])
            {
                remap[frame] = static_cast<unsigned int>(samples.size());
                samples.push_back({frame, frame, 0.0f});
            }
        for (unsigned int index = 0; index < outputAnimations->size(); ++index)
        {
            PreviewAnimationRange & animation = (*outputAnimations)[index];
            const unsigned int firstFrame = MAX(addedAnimations[index].startFrame, firstAnimationFrame);
            if (firstFrame > addedAnimations[index].endFrame)
            {
                animation.startFrame = 1;
                animation.endFrame = 0;
                continue;
            }
            animation.startFrame = remap[firstFrame];
            animation.endFrame = remap[animation.endFrame];
            animation.frameDelta = animation.storedDelta;
        }
    }
    else
    {
        for (unsigned int orderedIndex = 0; orderedIndex < ordered.size(); ++orderedIndex)
        {
            const unsigned int animationIndex = ordered[orderedIndex];
            const PreviewAnimationRange & animation = addedAnimations[animationIndex];
            const unsigned int firstFrame = MAX(animation.startFrame, firstAnimationFrame);
            if (firstFrame > animation.endFrame)
            {
                mappedStarts[animationIndex] = 1;
                mappedEnds[animationIndex] = 0;
                continue;
            }
            mappedStarts[animationIndex] = static_cast<unsigned int>(samples.size());
            if (!appendAnimationFrameSamples(firstFrame, animation.endFrame,
                                             animation.storedDelta, animation.frameDelta,
                                             &samples))
            {
                *error = "Resampling exceeds the ZTP animation frame limit.";
                return false;
            }
            mappedEnds[animationIndex] = static_cast<unsigned int>(samples.size() - 1);
            (*outputAnimations)[animationIndex].frameDelta = animation.frameDelta;
            (*outputAnimations)[animationIndex].storedDelta = animation.frameDelta;
            if (samples.size() > ZTP_MAX_ANIMATION_FRAMES)
            {
                *error = "Resampling exceeds the ZTP animation frame limit.";
                return false;
            }
        }
        for (unsigned int index = 0; index < outputAnimations->size(); ++index)
        {
            (*outputAnimations)[index].startFrame = mappedStarts[index];
            (*outputAnimations)[index].endFrame = mappedEnds[index];
        }
    }
    if (samples.size() > ZTP_MAX_ANIMATION_FRAMES ||
        samples.size() == 0)
    {
        if (samples.empty() && omitBasePoseFrame)
        {
            output->nbFrames = 0;
            output->keyFrames = NULL;
            return true;
        }
        *error = "Animation export contains no frames or exceeds the ZTP frame limit.";
        return false;
    }

    bool changed = samples.size() != source->nbFrames;
    for (unsigned int frame = 0; !changed && frame < samples.size(); ++frame)
        changed = samples[frame].firstFrame != frame || samples[frame].secondFrame != frame ||
            samples[frame].fraction != 0.0f;
    if (!changed)
    {
        output->nbFrames = static_cast<unsigned int>(samples.size());
        if (samples.size() != source->nbFrames)
        {
            frameDescriptors->resize(samples.size());
            for (unsigned int frame = 0; frame < samples.size(); ++frame)
                (*frameDescriptors)[frame] = source->keyFrames[samples[frame].firstFrame];
            output->keyFrames = frameDescriptors->data();
        }
        return true;
    }

    uint64_t totalPointCount = 0;
    for (unsigned int mesh = 0; mesh < source->nbModels; ++mesh)
        totalPointCount += source->model[mesh].nbPoint;
    if (totalPointCount * samples.size() > 4u * 1024u * 1024u)
    {
        *error = "Resampled animation exceeds the ZTP point-frame limit.";
        return false;
    }

    sampledFrames->resize(samples.size());
    frameDescriptors->resize(samples.size());
    for (unsigned int frame = 0; frame < samples.size(); ++frame)
    {
        std::vector<vertex_t> & vertices = (*sampledFrames)[frame];
        vertices.resize(static_cast<size_t>(totalPointCount));
        unsigned int pointOffset = 0;
        for (unsigned int mesh = 0; mesh < source->nbModels; ++mesh)
        {
            for (unsigned int point = 0; point < source->model[mesh].nbPoint; ++point)
            {
                const vertex_t & first = source->keyFrames[samples[frame].firstFrame].cVert[pointOffset + point];
                const vertex_t & second = source->keyFrames[samples[frame].secondFrame].cVert[pointOffset + point];
                vertex_t & destination = vertices[pointOffset + point];
                destination = first;
                for (unsigned int axis = 0; axis < XYZ; ++axis)
                    destination.point[axis] = first.point[axis] +
                        (second.point[axis] - first.point[axis]) * samples[frame].fraction;
            }
            pointOffset += source->model[mesh].nbPoint;
        }
        (*frameDescriptors)[frame].cVert = vertices.data();
    }
    output->nbFrames = static_cast<unsigned int>(samples.size());
    output->keyFrames = frameDescriptors->data();
    return true;
}

string absoluteModelPath(const string & path)
{
    DWORD required = GetFullPathNameA(path.c_str(), 0, NULL, NULL);
    if (!required) return path;
    std::vector<char> buffer(required);
    DWORD length = GetFullPathNameA(path.c_str(), required, buffer.data(), NULL);
    return length && length < required ? string(buffer.data(), length) : path;
}

bool isProtectedReferencePath(const string & path)
{
    const string absolute = absoluteModelPath(path);
    for (const string & reference : protectedReferencePaths)
        if (_stricmp(absolute.c_str(), reference.c_str()) == 0) return true;
    return false;
}

void exportSkinVariant(HWND owner)
{
    if (!currentModel || previewSkinIndex < 0 ||
        previewSkinIndex >= static_cast<int>(skinVariants.size())) return;
    SkinVariant & skinVariant = skinVariants[previewSkinIndex];
    if (!skinVariant.model) return;

    string basePath = convertedModelSavePath;
    if (basePath.empty())
    {
        CreateDirectoryA("OUT", NULL);
        const string baseName = fileOutName.empty() ? string("model") : fileOutName;
        basePath = "OUT\\" + baseName + ".ZTP";
    }
    const size_t separator = basePath.find_last_of("\\/");
    const string directory = separator == string::npos ? string() : basePath.substr(0, separator + 1);
    string baseName = separator == string::npos ? basePath : basePath.substr(separator + 1);
    const size_t extension = baseName.find_last_of('.');
    if (extension != string::npos) baseName.resize(extension);
    const string suggestedPath = directory + baseName + "_skin" +
        std::to_string(previewSkinIndex + 1) + ".ZTP";

    char filePath[MAX_PATH] = {};
    if (suggestedPath.size() >= sizeof(filePath))
    {
        MessageBoxA(owner, "The suggested skin output path is too long. Use a shorter model name or output folder.",
                    "Skin export failed", MB_OK | MB_ICONWARNING);
        return;
    }
    std::memcpy(filePath, suggestedPath.c_str(), suggestedPath.size() + 1);
    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrTitle = "Export skin model without animations";
    dialog.lpstrFilter = "ZTP files (*.ztp)\0*.ztp\0\0";
    dialog.lpstrFile = filePath;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = "ZTP";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameA(&dialog)) return;

    const string destination = absoluteModelPath(filePath);
    if (isProtectedReferencePath(destination))
    {
        MessageBoxA(owner, "This file is loaded as a visual reference and cannot be overwritten.",
                    "Protected reference", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!basePath.empty() && _stricmp(destination.c_str(), absoluteModelPath(basePath).c_str()) == 0)
    {
        MessageBoxA(owner, "The skin export must use a different filename from the base model.",
                    "Choose another filename", MB_OK | MB_ICONWARNING);
        return;
    }

    if (!saveZTPFileWithoutAnimations(destination, skinVariant.model))
    {
        MessageBoxA(owner, "Could not export the skin model. Check the destination and write permissions.",
                    "Skin export failed", MB_OK | MB_ICONERROR);
        return;
    }
    const std::vector<PreviewAnimationRange> noAnimations;
    if (!writeAnimationSidecar(destination + ".animations.txt", noAnimations, noAnimations))
    {
        MessageBoxA(owner, "The skin model was saved without animations, but an old animation sidecar could not be removed.",
                    "Animation metadata cleanup failed", MB_OK | MB_ICONWARNING);
        return;
    }
    MessageBoxA(owner, "The skin model was exported without animation frames. Its mesh and point indexes match the base model.",
                "Skin export complete", MB_OK | MB_ICONINFORMATION);
}
bool saveCurrentZTP(HWND owner, bool saveAs)
{
    if (!currentModel || !currentModelIsZTP) return false;
    string destination = ztpSavePath;
    if (saveAs)
    {
        char filePath[MAX_PATH] = {};
        std::strncpy(filePath, destination.c_str(), sizeof(filePath) - 1);
        OPENFILENAMEA dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = owner;
        dialog.lpstrTitle = "Save ZTP as";
        dialog.lpstrFilter = "ZTP files (*.ztp)\0*.ztp\0\0";
        dialog.lpstrFile = filePath;
        dialog.nMaxFile = MAX_PATH;
        dialog.lpstrDefExt = "ZTP";
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameA(&dialog)) return false;
        destination = absoluteModelPath(filePath);
    }
    if (isProtectedReferencePath(destination))
    {
        MessageBoxA(owner, "This file was loaded as a visual reference and cannot be overwritten. Choose another name with Save as.",
                    "Protected reference", MB_OK | MB_ICONWARNING);
        return false;
    }
    const bool exportReducedCopy = experimentalAnimationResampling && !addedAnimations.empty();
    if (exportReducedCopy && _stricmp(absoluteModelPath(destination).c_str(),
                                      absoluteModelPath(ztpSavePath).c_str()) == 0)
    {
        MessageBoxA(owner, "Experimental keyframe resampling writes a separate copy so the original remains reversible. Choose Save as and a different filename.",
                    "Choose a new ZTP filename", MB_OK | MB_ICONWARNING);
        return false;
    }
    animated_model_t modelToSave = {};
    std::vector<std::vector<vertex_t> > sampledFrames;
    std::vector<key_frame_t> frameDescriptors;
    std::vector<PreviewAnimationRange> outputAnimations;
    std::vector<PreviewAnimationRange> outputMemoryAnimations;
    string exportError;
    if (!buildAnimationExport(currentModel, &modelToSave, &sampledFrames,
                              &frameDescriptors, &outputAnimations, &outputMemoryAnimations,
                              exportReducedCopy,
                              exportReducedCopy, &exportError))
    {
        MessageBoxA(owner, exportError.c_str(), "Animation export failed", MB_OK | MB_ICONWARNING);
        return false;
    }
    std::vector<model_t> meshes(currentModel->model, currentModel->model + currentModel->nbModels);
    for (unsigned int mesh = 0; mesh < meshes.size(); ++mesh)
        meshes[mesh].pntbl = ztpBasePose[mesh].data();
    modelToSave.model = meshes.data();
    if (!saveZTPFile(destination, &modelToSave))
    {
        MessageBoxA(owner, "Could not save the ZTP file. Check the destination and write permissions. The previous file has not been replaced.",
                    "Save failed", MB_OK | MB_ICONERROR);
        return false;
    }
    if (!writeAnimationSidecar(destination + ".animations.txt", outputAnimations,
                               outputMemoryAnimations))
        MessageBoxA(owner, "The ZTP file was saved, but its animation companion sidecar could not be written. Reduced animations require this metadata.",
                    "Animation metadata save failed", MB_OK | MB_ICONERROR);
    if (exportReducedCopy)
    {
        MessageBoxA(owner, "A resampled ZTP copy was saved with remapped animation metadata. The original file and the current in-memory animation remain unchanged.",
                    "Reduced ZTP copy saved", MB_OK | MB_ICONINFORMATION);
        return true;
    }
    ztpSavePath = destination;
    currentFolder = destination;
    currentFile = destination.substr(destination.find_last_of("\\/") + 1);
    fileOutName = currentFile.substr(0, currentFile.find_last_of('.'));
    ztpSaved = true;
    ztpBaseReplaced = false;
    savedEditRevision = surfaceEditRevision();
    savedLodRevision = currentModel->lodData ? currentModel->lodData->revision : 0;
    SetWindowTextA(owner, ("ZTP Converter - " + currentFile).c_str());
    updateSurfacePanel(owner, currentModel);
    return true;
}

bool saveConvertedModel(HWND owner, bool saveAs)
{
    if (!currentModel || currentModelIsZTP) return false;
    string destination = convertedModelSavePath;
    if (destination.empty())
    {
        CreateDirectoryA("OUT", NULL);
        destination = "OUT\\" + (fileOutName.empty() ? string("model") : fileOutName) + ".ZTP";
    }
    if (saveAs)
    {
        char filePath[MAX_PATH] = {};
        std::strncpy(filePath, destination.c_str(), sizeof(filePath) - 1);
        OPENFILENAMEA dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = owner;
        dialog.lpstrTitle = "Save ZTP as";
        dialog.lpstrFilter = "ZTP files (*.ztp)\0*.ztp\0\0";
        dialog.lpstrFile = filePath;
        dialog.nMaxFile = MAX_PATH;
        dialog.lpstrDefExt = "ZTP";
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameA(&dialog)) return false;
        destination = filePath;
    }
    const size_t extension = destination.find_last_of('.');
    const size_t separator = destination.find_last_of("\\/");
    const bool hasFileExtension = extension != string::npos &&
        (separator == string::npos || extension > separator);
    if (!hasFileExtension)
        destination += ".ZTP";
    else if (_stricmp(destination.substr(extension).c_str(), ".ztp") != 0)
        destination = destination.substr(0, extension) + ".ZTP";
    destination = absoluteModelPath(destination);
    if (isProtectedReferencePath(destination) ||
        _stricmp(destination.c_str(), absoluteModelPath(currentFolder + currentFile).c_str()) == 0)
    {
        MessageBoxA(owner, "The ZTP output cannot overwrite the source model or a protected reference. Choose another filename.",
                    "Protected output path", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (USE_SGL == 2)
    {
        MessageBoxA(owner, "Palette export mode does not produce a ZTP file. Choose Disabled or Enabled in converter options.",
                    "ZTP export unavailable", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (USE_SGL == 0)
        for (unsigned int mesh = 0; mesh < currentModel->nbModels; ++mesh)
            if (currentModel->model[mesh].nbPoint > 256)
            {
                char error[256];
                sprintf(error, "Mesh %u has %u points; non-SGL Z01 supports at most 256. Enable SGL or reduce/replace the base mesh before export.",
                    mesh + 1, currentModel->model[mesh].nbPoint);
                MessageBoxA(owner, error, "ZTP export failed", MB_OK | MB_ICONWARNING);
                return false;
            }

    animated_model_t modelToSave = {};
    std::vector<std::vector<vertex_t> > sampledFrames;
    std::vector<key_frame_t> frameDescriptors;
    std::vector<PreviewAnimationRange> outputAnimations;
    std::vector<PreviewAnimationRange> outputMemoryAnimations;
    string exportError;
    if (!buildAnimationExport(currentModel, &modelToSave, &sampledFrames,
                              &frameDescriptors, &outputAnimations, &outputMemoryAnimations, true,
                              experimentalAnimationResampling, &exportError))
    {
        MessageBoxA(owner, exportError.c_str(), "Animation export failed", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (!saveZTPFile(destination, &modelToSave))
    {
        MessageBoxA(owner, "Could not save the ZTP file. Check the destination and write permissions. The previous file has not been replaced.",
                    "Save failed", MB_OK | MB_ICONERROR);
        return false;
    }
    if (!writeAnimationSidecar(destination + ".animations.txt", outputAnimations,
                               outputMemoryAnimations))
        MessageBoxA(owner, "The ZTP file was saved, but its animation companion sidecar could not be written. Reduced animations require this metadata.",
                    "Animation metadata save failed", MB_OK | MB_ICONERROR);
    convertedModelSavePath = destination;
    return true;
}

bool confirmZTPSave(HWND owner)
{
    if (!currentModelIsZTP || !currentModel ||
        (ztpSaved && !ztpBaseReplaced && savedEditRevision == surfaceEditRevision() &&
         savedLodRevision == (currentModel->lodData ? currentModel->lodData->revision : 0))) return true;
    int answer = MessageBoxA(owner, "Save the current ZTP file before continuing?",
        "Unsaved ZTP", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (answer == IDCANCEL) return false;
    return answer == IDNO || (answer == IDYES && saveCurrentZTP(owner, false));
}

bool exportCurrentSkeleton(HWND owner)
{
    if (!currentModel || !hasFbxSkeleton(currentModel)) return false;
    fbxLoadError.clear();
    string baseName = currentFile;
    const size_t extension = baseName.find_last_of('.');
    if (extension != string::npos) baseName.resize(extension);
    baseName += "_skeleton.c";
    char filePath[MAX_PATH] = {};
    std::strncpy(filePath, baseName.c_str(), sizeof(filePath) - 1);
    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrTitle = "Export Saturn skeleton as C";
    dialog.lpstrFilter = "C source files (*.c)\0*.c\0\0";
    dialog.lpstrFile = filePath;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = "c";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameA(&dialog)) return false;

    unsigned int bones = 0;
    unsigned int nodes = 0;
    unsigned int frames = 0;
    unsigned long long dataSize = 0;
    uint32_t translationScaleQ16_16 = 0;
    if (!exportFbxSkeletonC(currentModel, filePath, &bones, &nodes, &frames, &dataSize,
                            &translationScaleQ16_16))
    {
        const string error = fbxLoadError.empty() ?
            "Could not export the skeleton. Check the destination and try again." : fbxLoadError;
        MessageBoxA(owner, error.c_str(), "Skeleton export failed", MB_OK | MB_ICONERROR);
        return false;
    }
    std::ostringstream summary;
    summary << "Skeleton exported successfully.\n\nBones: " << bones
            << "\nHierarchy nodes: " << nodes
            << "\nAnimation frames: " << frames
            << "\nTranslation unit scale: "
            << static_cast<double>(translationScaleQ16_16) / 65536.0 << "x"
            << "\nSaturn data size: " << dataSize << " bytes\n\n"
            << absoluteModelPath(filePath);
    MessageBoxA(owner, summary.str().c_str(), "Skeleton export complete", MB_OK | MB_ICONINFORMATION);
    return true;
}

void removeComparison(HWND owner)
{
    if (!comparisonTextureIds.empty())
        glDeleteTextures(static_cast<GLsizei>(comparisonTextureIds.size()), comparisonTextureIds.data());
    comparisonTextureIds.clear();
    deleteModels(comparisonModel);
    comparisonModel = NULL;
    resetSelfShadowing();
    resetPreviewColors();
    SetDlgItemTextA(owner, IDC_COMPARISON_FILE, "Reference: none");
    EnableWindow(GetDlgItem(owner, IDM_COMPARISON_REMOVE), FALSE);
}

bool loadComparison(const string & fullPath, const string & folder, const string & fileName,
                    bool isZTP, HWND owner)
{
    if (!currentModel)
    {
        MessageBoxA(owner, "Open a working model first.", "Visual reference", MB_OK | MB_ICONINFORMATION);
        return false;
    }
    animated_model_t * nextModel = NULL;
    LoadOptions options = comparisonOptions;
    if (isZTP) nextModel = loadZTPFile(fullPath);
    else
    {
        if (!showOptionsDialog(owner, &options)) return false;
        const int savedSgl = USE_SGL;
        const uint32_t savedUv = useUV;
        const uint32_t savedSpecial = specialOption;
        const uint32_t savedMaxTextureWidth = maxGeneratedTextureWidth;
        const uint32_t savedMaxTextureHeight = maxGeneratedTextureHeight;
        const uint32_t savedCoordinatesCount = tot_tex_coord;
        const std::vector<uv_t> savedCoordinates(texture_coord, texture_coord + tot_tex_coord);
        nextModel = loadingConfigured(options.frameCount, options.sglMode, options.scale,
            options.useUv ? 1u : 0u, options.specialOptions, folder, fileName, true,
            options.rotationX, options.rotationY, options.rotationZ,
            options.translationX, options.translationY, options.translationZ,
            options.maxTextureWidth, options.maxTextureHeight);
        USE_SGL = savedSgl;
        useUV = savedUv;
        specialOption = savedSpecial;
        maxGeneratedTextureWidth = savedMaxTextureWidth;
        maxGeneratedTextureHeight = savedMaxTextureHeight;
        tot_tex_coord = savedCoordinatesCount;
        std::copy(savedCoordinates.begin(), savedCoordinates.end(), texture_coord);
    }
    if (!nextModel || !nextModel->nbModels || !nextModel->nbTextures)
    {
        deleteModels(nextModel);
        MessageBoxA(owner, "Could not read this reference model.",
                    "Reference load failed", MB_OK | MB_ICONERROR);
        return false;
    }
    removeComparison(owner);
    comparisonModel = nextModel;
    protectedReferencePaths.push_back(absoluteModelPath(fullPath));
    comparisonOptions = options;
    comparisonTextureIds.resize(comparisonModel->nbTextures);
    setModelTextures(comparisonModel, comparisonTextureIds.data());
    calcLight(comparisonModel);
    SetDlgItemTextA(owner, IDC_COMPARISON_FILE, ("Reference: " + fileName).c_str());
    EnableWindow(GetDlgItem(owner, IDM_COMPARISON_REMOVE), TRUE);
    return true;
}

void openModelDialog(HWND owner, bool comparison = false)
{
    char filePath[MAX_PATH] = {};
    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrTitle = comparison ? "Load visual reference" : "Open working model";
    dialog.lpstrFilter = "Model files (*.obj;*.fbx;*.ztp)\0*.obj;*.fbx;*.ztp\0Wavefront OBJ files (*.obj)\0*.obj\0Autodesk FBX files (*.fbx)\0*.fbx\0ZTP files (*.ztp)\0*.ztp\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = filePath;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = "obj";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&dialog)) return;

    const string fullPath = filePath;
    char * separator = strrchr(filePath, '\\');
    if (!separator) separator = strrchr(filePath, '/');
    string folder;
    string fileName;
    if (separator)
    {
        *separator = '\0';
        folder = string(filePath) + "\\";
        fileName = separator + 1;
    }
    else
    {
        folder = ".\\";
        fileName = filePath;
    }
    size_t extension = fileName.find_last_of('.');
    string extensionName = extension == string::npos ? "" : fileName.substr(extension);
    for (size_t index = 0; index < extensionName.size(); ++index)
        extensionName[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(extensionName[index])));
    if (extensionName == ".ztp")
    {
        if (comparison) loadComparison(fullPath, folder, fileName, true, owner);
        else loadZTPModel(fullPath, fileName, owner);
        return;
    }
    if (extensionName != ".obj" && extensionName != ".fbx")
    {
        MessageBoxA(owner, "Choose an OBJ, FBX or ZTP file.", "Unsupported file", MB_OK | MB_ICONWARNING);
        return;
    }
    fileName.resize(extension);
    if (comparison && extensionName == ".fbx")
    {
        MessageBoxA(owner, "FBX visual references are not supported yet.",
                    "Unsupported reference", MB_OK | MB_ICONWARNING);
        return;
    }
    if (comparison) loadComparison(fullPath, folder, fileName, false, owner);
    else loadModel(folder, extensionName == ".fbx" ? fileName + ".fbx" : fileName,
                   owner, extensionName == ".obj");
}

HMENU createMainMenu()
{
    HMENU menu = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_OPEN, "Open model...");
    AppendMenuA(fileMenu, MF_STRING, IDM_COMPARISON_OPEN, "Load visual reference...");
    AppendMenuA(fileMenu, MF_STRING, IDM_COMPARISON_REMOVE, "Remove visual reference");
    AppendMenuA(fileMenu, MF_STRING | MF_GRAYED, IDM_EXPORT_SKELETON, "Export skeleton (C)...");
    AppendMenuA(fileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_EXIT, "Exit");
    AppendMenuA(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), "File");
    HMENU skinMenu = CreatePopupMenu();
    AppendMenuA(skinMenu, MF_STRING | MF_GRAYED, IDM_SKIN_LOAD, "Load skin atlas (OBJ/FBX)...");
    AppendMenuA(skinMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(skinMenu, MF_STRING | MF_GRAYED, IDM_SKIN_PREVIEW_BASE, "Preview base model");
    AppendMenuA(skinMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(skinMenu, MF_STRING | MF_GRAYED, IDM_SKIN_EXPORT, "Export selected skin model...");
    AppendMenuA(skinMenu, MF_STRING | MF_GRAYED, IDM_SKIN_REMOVE, "Remove selected skin");
    AppendMenuA(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(skinMenu), "Skins");
    HMENU optionsMenu = CreatePopupMenu();
    AppendMenuA(optionsMenu, MF_STRING, IDM_OPTIONS, "Configure and reload...");
    AppendMenuA(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(optionsMenu), "Options");
    return menu;
}
}

void deleteModels(animated_model_t * model)
{
    if (!model) return;
    discardFbxBonePreview(model);
    discardFbxSkeleton(model);
    discardAutomaticLodBaseSnapshot(model);
    discardAutomaticLodSourceTextures(model);
    delete model->lodData;
    for (unsigned int i=0; i<model->nbModels; i++) {
        delete [] model->model[i].pltbl;
        delete [] model->model[i].pntbl;
    }

    for (unsigned int i=0; i<model->nbFrames; i++) {
        if (model->keyFrames)
        {
            //if (model->keyFrames[i].cNorm) free(model->keyFrames[i].cNorm);
            delete [] model->keyFrames[i].cVert;
        }
    }


    for (unsigned int i=0; i<model->nbTextures; i++) {
        delete [] model->texture[i].pixel;
    }


    delete [] model->texture;
    delete [] model->model;
    delete [] model->keyFrames;
    delete model;
}

/***************************************
* UGLY UGLY GLOBALS... TO BE REMOVED SOMEDAY...
***********************************/
unsigned int texId[1280];
string PdataName[256];
int DisplayTEST=0;
int ZT_FRAMERATE=1;
player_t PLAYER[4];
int enableInterpolation=1;

uv_t   texture_coord[maxPolygons*4];
uint32_t tot_tex_coord = 0;
uint32_t useUV = 0;
uint32_t specialOption = 0;
uint8_t clearColor[4] ={128, 128, 128, 255};
texture_t skin;
BOOL bQuit = FALSE;

extern void                     initOpenGL();
extern animated_model_t *       loading(string line);
extern void                     setTextures(animated_model_t * model);
extern void                     calcLight(animated_model_t*);

WNDCLASSEX wcex;
HWND hwnd;
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    INITCOMMONCONTROLSEX commonControls = {sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES};
    if (!InitCommonControlsEx(&commonControls)) return 0;
    extern void writeNormLUT(void);
    writeNormLUT();



    HDC hDC;
    HGLRC hRC;
    MSG msg;


    /* register window class */
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_OWNDC;
    wcex.lpfnWndProc = WindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = "ZTP_CONVERTER";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);;


    if (!RegisterClassEx(&wcex))
        return 0;


    hwnd = CreateWindowExA(0, "ZTP_CONVERTER", "ZTP Converter",
                           WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
                           64, 64, 1360, 1024, NULL, createMainMenu(), hInstance, NULL);
    if (!hwnd) return 0;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    EnableOpenGL(hwnd, &hDC, &hRC);
    initOpenGL();
    glReady = true;
    resizeOpenGLView(hwnd);

    if (lpCmdLine && lpCmdLine[0])
    {
        std::istringstream arguments(lpCmdLine);
        int frameCount = 0;
        int sglMode = 1;
        float scale = 1.0f;
        int useUv = 0;
        unsigned int specialOptions = 0;
        uint32_t maxTextureWidth = 512;
        uint32_t maxTextureHeight = 256;
        arguments >> frameCount >> sglMode >> scale >> useUv >> specialOptions;
        string ignoredArgument;
        arguments >> ignoredArgument >> ignoredArgument >> ignoredArgument >> maxTextureWidth >> maxTextureHeight;
        loadOptions.frameCount = frameCount;
        loadOptions.sglMode = sglMode;
        loadOptions.scale = scale;
        loadOptions.useUv = useUv != 0;
        loadOptions.specialOptions = specialOptions;
        loadOptions.maxTextureWidth = maxTextureWidth;
        loadOptions.maxTextureHeight = maxTextureHeight;
        currentModel = loading(string(lpCmdLine));
        std::istringstream fileArguments(lpCmdLine);
        string ignored;
        fileArguments >> ignored >> ignored >> ignored >> ignored >> ignored >> currentFolder >> currentFile;
        setTextures(currentModel);
        for (int player = 0; player < 4; ++player) resetPlayer(&PLAYER[player]);
        resetModelAnimation(currentModel);
        if (!currentFile.empty()) SetWindowTextA(hwnd, ("ZTP Converter - " + currentFile).c_str());
    }
    else
    {
        openModelDialog(hwnd);
    }
    updateFileMenu(hwnd);

/**
    for (uint32_t i=0; i<tot_tex_coord; i++) {
        cout << (int)(texture_coord[i].U*65536) << ", " << (int)(texture_coord[i].V*65536) << "\n";
    }*/
    /* program main loop */
    while (!bQuit)
    {
        /* check for messages */
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            /* handle or dispatch messages */
            if (msg.message == WM_QUIT)
            {
                bQuit = TRUE;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        if (bQuit) break;
        if (currentModel)
        {
            glClearColor(0.4f, 0.4f, 0.5f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glMatrixMode(GL_MODELVIEW);
            glDisable(GL_CULL_FACE);
            glColor4f(1.0f,1.0f,1.0f,1.0f);
            glPushMatrix();
            glMatrixMode(GL_MODELVIEW);
            gluLookAt(0.0, 0.0, -0.1, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0);
            extern int myControls(player_t*, animated_model_t*);
            extern void suspendModelControls(player_t *);
            const HWND focusedControl = GetFocus();
            const bool textInputActive = focusedControl == GetDlgItem(hwnd, IDC_ANIMATION_NAME) ||
                focusedControl == GetDlgItem(hwnd, IDC_ANIMATION_START) ||
                focusedControl == GetDlgItem(hwnd, IDC_ANIMATION_END) ||
                focusedControl == GetDlgItem(hwnd, IDC_ANIMATION_LOOP) ||
                focusedControl == GetDlgItem(hwnd, IDC_SURFACE_TEXTURE_SIZE);
            if (GetForegroundWindow() == hwnd && !textInputActive &&
                myControls(&PLAYER[0], currentModel) == -1)
            {
                SendMessageA(hwnd, WM_CLOSE, 0, 0);
            }
            else if (GetForegroundWindow() != hwnd || textInputActive)
                suspendModelControls(&PLAYER[0]);
            if (shownSelectionRevision != surfaceSelectionRevision())
                updateSurfacePanel(hwnd, currentModel);
            UINT wireframeState = (DisplayTEST & 1) ? BST_CHECKED : BST_UNCHECKED;
            if (IsDlgButtonChecked(hwnd, IDC_WIREFRAME) != wireframeState)
                CheckDlgButton(hwnd, IDC_WIREFRAME, wireframeState);
            PLAYER[0].POSITION[X] = PLAYER[0].NEXT_POSITION[X];
            PLAYER[0].POSITION[Y] = PLAYER[0].NEXT_POSITION[Y];
            PLAYER[0].POSITION[Z] = PLAYER[0].NEXT_POSITION[Z];
            if (DisplayTEST==0) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            else glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            animated_model_t * previewModel = currentModel;
            const unsigned int * previewTextureIds = texId;
            if (previewSkinIndex >= 0 && previewSkinIndex < static_cast<int>(skinVariants.size()) &&
                skinVariants[previewSkinIndex].model)
            {
                previewModel = skinVariants[previewSkinIndex].model;
                previewTextureIds = skinVariants[previewSkinIndex].textureIds.data();
            }
            render(previewModel, comparisonModel, comparisonTextureIds.data(), previewTextureIds);
            glPopMatrix();
            renderText(currentModel);
        }
        else
        {
            glClearColor(0.12f, 0.14f, 0.17f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        SwapBuffers(hDC);
        Sleep (ZT_FRAMERATE);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }

#if 1
    if (currentModel && !currentModelIsZTP)
    {
    extern string fileOutName;// = "OUT/" + fileIn;
    CreateDirectory("OUT/", NULL);
    cout << "NOW CREATING BINARY FILE " << fileOutName << ".ZTP\n";
    string binaryFileName = convertedModelSavePath.empty() ?
        "OUT\\" + (fileOutName.empty() ? string("model") : fileOutName) + ".ZTP" :
        convertedModelSavePath;
    while (isProtectedReferencePath(binaryFileName))
        binaryFileName.insert(binaryFileName.size() - 4, "_working");
    cout << "Binary output: " << binaryFileName << "\n";

    char exportError[256] = {};
    if (USE_SGL == 2)
        sprintf(exportError, "Palette export mode does not produce a ZTP file. Choose Disabled or Enabled in converter options.");
    else if (USE_SGL == 0)
        for (unsigned int mesh = 0; mesh < currentModel->nbModels; ++mesh)
            if (currentModel->model[mesh].nbPoint > 256)
            {
                sprintf(exportError, "Mesh %u has %u points; non-SGL Z01 supports at most 256. Enable SGL or reduce/replace the base mesh before export.",
                    mesh + 1, currentModel->model[mesh].nbPoint);
                break;
            }

    if (exportError[0])
    {
        cout << "\nERROR WHILE WRITING BINARY FILE: " << exportError << "\n";
        MessageBoxA(hwnd, exportError, "ZTP export failed", MB_OK | MB_ICONERROR);
    }
    else
    {
        extern void write_model_binary(ofstream*file, animated_model_t*aModel, bool preserveLoadedZTP);
        extern bool write_MOD_HEADER(string filename, animated_model_t * aModel);
        extern int writeShadows(string, animated_model_t*);
        char temporaryPath[MAX_PATH] = {};
        if (!GetTempFileNameA("OUT", "ztp", 0, temporaryPath))
        {
            MessageBoxA(hwnd, "Could not create a temporary export file in OUT.",
                        "ZTP export failed", MB_OK | MB_ICONERROR);
        }
        else
        {
            ofstream binFile(temporaryPath, ios::out | ios::binary | ios::trunc);
            bool written = binFile.is_open();
            std::vector<std::vector<vertex_t> > sampledFrames;
            std::vector<key_frame_t> exportFrames;
            std::vector<PreviewAnimationRange> outputAnimations;
            std::vector<PreviewAnimationRange> outputMemoryAnimations;
            string animationError;
            if (!written)
                cout << "\nERROR WHILE OPENING TEMPORARY BINARY FILE!\n";
            else
            {
                animated_model_t exportModel = {};
                if (!buildAnimationExport(currentModel, &exportModel, &sampledFrames,
                                          &exportFrames, &outputAnimations, &outputMemoryAnimations, true,
                                          experimentalAnimationResampling, &animationError))
                {
                    binFile.close();
                    DeleteFileA(temporaryPath);
                    MessageBoxA(hwnd, animationError.c_str(), "Animation export failed",
                                MB_OK | MB_ICONWARNING);
                    written = false;
                }
                else
                {
                    write_model_binary(&binFile, &exportModel, currentModelIsZTP);
                    binFile.close();
                    written = binFile.good();
                }
            }
            if (written && MoveFileExA(temporaryPath, binaryFileName.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                if (!writeAnimationSidecar(binaryFileName + ".animations.txt", outputAnimations,
                                           outputMemoryAnimations))
                    MessageBoxA(hwnd, "The ZTP file was exported, but its animation companion sidecar could not be written. Reduced animations require this metadata.",
                                "Animation metadata export failed", MB_OK | MB_ICONERROR);
                cout << "Writing debug text file...\n";
                write_MOD_HEADER("OUT.TXT", currentModel);
                writeShadows("SHADOWS.PNG", currentModel);
            }
            else
            {
                DeleteFileA(temporaryPath);
                if (animationError.empty())
                    MessageBoxA(hwnd, "The ZTP writer rejected the model or could not finish writing. The previous export was preserved.",
                                "ZTP export failed", MB_OK | MB_ICONERROR);
            }
        }
    }
    }
#endif // 1

    /* shutdown OpenGL */
    removeComparison(hwnd);
    resetAutomaticLod();
    clearSkinVariants();
    releaseModelTextures(currentModel);
    deleteModels(currentModel);
    DisableOpenGL(hwnd, hDC, hRC);

    /* destroy the window explicitly */
    DestroyWindow(hwnd);

return 0;
    //return msg.wParam;
}



LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
            createSidePanel(hwnd);
            layoutSidePanel(hwnd);
            updateSurfacePanel(hwnd, NULL);
            refreshAnimationRangeControls(hwnd, NULL);
            refreshAddedAnimations(hwnd, 0xFFFFFFFFu);
            return 0;

        case WM_SIZE:
            layoutSidePanel(hwnd);
            if (glReady) resizeOpenGLView(hwnd);
            return 0;

        case WM_GETMINMAXINFO:
        {
            RECT minimum = {0, 0, SIDE_PANEL_WIDTH + 320, 900};
            AdjustWindowRectEx(&minimum, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW, TRUE, 0);
            MINMAXINFO * limits = reinterpret_cast<MINMAXINFO *>(lParam);
            limits->ptMinTrackSize.x = minimum.right - minimum.left;
            limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
            return 0;
        }

        case WM_RBUTTONDOWN:
        {
            RECT client;
            GetClientRect(hwnd, &client);
            int cursorX = static_cast<short>(LOWORD(lParam));
            int cursorY = static_cast<short>(HIWORD(lParam));
            if (currentModel && cursorX >= 0 && cursorX < client.right - SIDE_PANEL_WIDTH &&
                cursorY >= 0 && cursorY < client.bottom)
                SetCapture(hwnd);
            return 0;
        }

        case WM_RBUTTONUP:
        case WM_CANCELMODE:
            if (GetCapture() == hwnd) ReleaseCapture();
            return 0;

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE)
            {
                if (GetCapture() == hwnd) ReleaseCapture();
                extern void suspendModelControls(player_t *);
                suspendModelControls(&PLAYER[0]);
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);

        case WM_CLOSE:
            if (GetCapture() == hwnd) ReleaseCapture();
            if (confirmZTPSave(hwnd)) PostQuitMessage(0);
        break;

        case WM_DESTROY:
            if (surfaceTexturePreviewBitmap)
            {
                DeleteObject(surfaceTexturePreviewBitmap);
                surfaceTexturePreviewBitmap = NULL;
            }
            return 0;

        case WM_KEYDOWN:
        {
            switch (wParam)
            {
                case VK_ESCAPE:
                    if (!(lParam & (1u << 30))) SendMessageA(hwnd, WM_CLOSE, 0, 0);
                break;
            }
        }
        break;

        case WM_HSCROLL:
        {
            HWND slider = reinterpret_cast<HWND>(lParam);
            if (slider && slider == GetDlgItem(hwnd, IDC_LOD_REDUCTION))
            {
                char label[48];
                sprintf(label, "LOD reduction: %d%%", static_cast<int>(SendMessageA(slider, TBM_GETPOS, 0, 0)));
                SetDlgItemTextA(hwnd, IDC_LOD_REDUCTION_LABEL, label);
                return 0;
            }
            if (slider && (slider == GetDlgItem(hwnd, IDC_LIGHT_AZIMUTH) ||
                           slider == GetDlgItem(hwnd, IDC_LIGHT_ELEVATION)))
            {
                updateLightFromSliders(hwnd);
                return 0;
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        case WM_COMMAND:
        {
            if (LOWORD(wParam) == IDC_SURFACE_TEXTURE_APPLY && HIWORD(wParam) == BN_CLICKED)
            {
                const LRESULT selectedSize = SendDlgItemMessageA(hwnd, IDC_SURFACE_TEXTURE_SIZE,
                                                                   CB_GETCURSEL, 0, 0);
                if (selectedSize < 0 ||
                    static_cast<size_t>(selectedSize) >=
                        sizeof(surfaceTextureSizeOptions) / sizeof(surfaceTextureSizeOptions[0]))
                {
                    MessageBoxA(hwnd, "Choose a maximum texture dimension first.",
                                "Texture resize", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                const unsigned int previousTextureCount = currentModel ? currentModel->nbTextures : 0;
                bool changed = false;
                string error;
                if (!resizeActiveSurfaceTexture(surfaceTextureSizeOptions[selectedSize], &changed, &error))
                    MessageBoxA(hwnd, error.c_str(), "Texture resize failed", MB_OK | MB_ICONWARNING);
                else if (changed)
                {
                    refreshModelTextures(currentModel, previousTextureCount);
                    if (currentModelIsZTP) ztpSaved = false;
                    const LRESULT selectedLod = SendDlgItemMessageA(hwnd, IDC_LOD_LEVEL,
                                                                    CB_GETCURSEL, 0, 0);
                    refreshLodStatus(hwnd, selectedLod > 0 ?
                        static_cast<unsigned int>(selectedLod) : 0);
                }
                updateSurfacePanel(hwnd, currentModel);
                SetFocus(hwnd);
                return 0;
            }
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                int controlId = LOWORD(wParam);
                if (controlId == IDC_ANIMATION_LIST)
                {
                    LRESULT selected = SendDlgItemMessageA(hwnd, IDC_ANIMATION_LIST, CB_GETCURSEL, 0, 0);
                    if (selected >= 0)
                    {
                        refreshAddedAnimations(hwnd, static_cast<unsigned int>(selected));
                        selectAddedAnimation(hwnd);
                    }
                    SetFocus(hwnd);
                    return 0;
                }
                if (controlId == IDC_ANIMATION_FRAME_DELTA)
                {
                    const LRESULT selected = SendDlgItemMessageA(hwnd, IDC_ANIMATION_LIST, CB_GETCURSEL, 0, 0);
                    const LRESULT deltaIndex = SendDlgItemMessageA(hwnd, IDC_ANIMATION_FRAME_DELTA, CB_GETCURSEL, 0, 0);
                    if (selected >= 0 && static_cast<unsigned int>(selected) < addedAnimations.size() &&
                        deltaIndex >= 0 && deltaIndex < 16)
                    {
                        PreviewAnimationRange & range = addedAnimations[static_cast<unsigned int>(selected)];
                        range.frameDelta = static_cast<unsigned int>(deltaIndex) + 1;
                        if (range.frameDelta != range.storedDelta)
                            experimentalAnimationResampling = true;
                        PLAYER[0].ANIMCTRL.frameDelta = activeAnimationFrameDelta();
                        PLAYER[0].ANIMCTRL.sourceFrameDelta = activeAnimationStoredDelta();
                        refreshAddedAnimations(hwnd, static_cast<unsigned int>(selected));
                        setAnimationPreviewRange(range.startFrame, range.endFrame);
                        if (!saveAnimationSidecar())
                            MessageBoxA(hwnd, "Could not save the animation ranges file.",
                                        "Animation save failed", MB_OK | MB_ICONERROR);
                    }
                    SetFocus(hwnd);
                    return 0;
                }
                if (controlId == IDC_LOD_LEVEL)
                {
                    LRESULT level = SendDlgItemMessageA(hwnd, IDC_LOD_LEVEL, CB_GETCURSEL, 0, 0);
                    if (level >= 0) refreshLodControls(hwnd, static_cast<unsigned int>(level));
                    resetSelfShadowing();
                    clearSelectedSurfaces();
                    SetFocus(hwnd);
                    return 0;
                }
                if (controlId == IDC_ANIMATION_SPEED || controlId == IDC_CAMERA_SPEED)
                {
                    LRESULT index = SendDlgItemMessageA(hwnd, controlId, CB_GETCURSEL, 0, 0);
                    if (index >= 0 && index < 7)
                    {
                        if (controlId == IDC_ANIMATION_SPEED)
                            animationPlaybackSpeed = animationSpeedValues[index];
                        else
                            cameraMovementSpeed = cameraSpeedValues[index];
                    }
                    SetFocus(hwnd);
                    return 0;
                }
            }
            if (LOWORD(wParam) == IDC_ANIMATION_NAME && HIWORD(wParam) == EN_CHANGE)
            {
                commitSelectedAnimationName(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == IDC_ANIMATION_LOOP && HIWORD(wParam) == BN_CLICKED)
            {
                const LRESULT selected = SendDlgItemMessageA(hwnd, IDC_ANIMATION_LIST, CB_GETCURSEL, 0, 0);
                if (selected >= 0 && static_cast<unsigned int>(selected) < addedAnimations.size())
                {
                    const bool loop = IsDlgButtonChecked(hwnd, IDC_ANIMATION_LOOP) == BST_CHECKED;
                    addedAnimations[static_cast<unsigned int>(selected)].loop = loop;
                    animationLoopPlayback = loop;
                    if (!saveAnimationSidecar())
                        MessageBoxA(hwnd, "Could not save the animation ranges file.",
                                    "Animation save failed", MB_OK | MB_ICONERROR);
                }
                return 0;
            }
            if (LOWORD(wParam) == IDC_ANIMATION_RESAMPLE && HIWORD(wParam) == BN_CLICKED)
            {
                experimentalAnimationResampling =
                    IsDlgButtonChecked(hwnd, IDC_ANIMATION_RESAMPLE) == BST_CHECKED;
                PLAYER[0].ANIMCTRL.frameDelta = activeAnimationFrameDelta();
                PLAYER[0].ANIMCTRL.sourceFrameDelta = activeAnimationStoredDelta();
                const unsigned int selected = selectedAnimationIndex();
                if (selected < addedAnimations.size())
                    setAnimationPreviewRange(addedAnimations[selected].startFrame,
                                             addedAnimations[selected].endFrame);
                else if (currentModel && currentModel->nbFrames)
                    setAnimationPreviewRange(0, maxPreviewFrame(currentModel));
                return 0;
            }
            if (LOWORD(wParam) == IDC_ANIMATION_RANGE_APPLY && HIWORD(wParam) == BN_CLICKED)
            {
                applyAnimationRange(hwnd);
                SetFocus(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == IDC_ANIMATION_ADD && HIWORD(wParam) == BN_CLICKED)
            {
                addOrReplaceAnimation(hwnd);
                SetFocus(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == IDC_ANIMATION_DELETE && HIWORD(wParam) == BN_CLICKED)
            {
                deleteSelectedAnimation(hwnd);
                SetFocus(hwnd);
                return 0;
            }
            const unsigned int commandId = LOWORD(wParam);
            if (commandId >= IDM_SKIN_PREVIEW_FIRST &&
                commandId < IDM_SKIN_PREVIEW_FIRST + skinVariants.size())
            {
                const unsigned int skinIndex = commandId - IDM_SKIN_PREVIEW_FIRST;
                if (skinIndex < skinVariants.size() && skinVariants[skinIndex].model)
                {
                    previewSkinIndex = static_cast<int>(skinIndex);
                    updateSkinMenu(hwnd);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }
            switch (LOWORD(wParam))
            {
                case IDM_SKIN_LOAD:
                    loadSkinAtlas(hwnd);
                    SetFocus(hwnd);
                    return 0;
                case IDM_SKIN_PREVIEW_BASE:
                    previewSkinIndex = -1;
                    updateSkinMenu(hwnd);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                case IDM_SKIN_EXPORT:
                    exportSkinVariant(hwnd);
                    SetFocus(hwnd);
                    return 0;
                case IDM_SKIN_REMOVE:
                    if (previewSkinIndex >= 0 &&
                        previewSkinIndex < static_cast<int>(skinVariants.size()))
                    {
                        destroySkinVariant(&skinVariants[previewSkinIndex], true);
                        skinVariants.erase(skinVariants.begin() + previewSkinIndex);
                        previewSkinIndex = -1;
                    }
                    updateSkinMenu(hwnd);
                    InvalidateRect(hwnd, NULL, FALSE);
                    SetFocus(hwnd);
                    return 0;
                case IDM_FILE_OPEN:
                case IDC_OPEN_ANOTHER:
                    openModelDialog(hwnd);
                    return 0;
                case IDM_COMPARISON_OPEN:
                    openModelDialog(hwnd, true);
                    SetFocus(hwnd);
                    return 0;
                case IDM_COMPARISON_REMOVE:
                    removeComparison(hwnd);
                    SetFocus(hwnd);
                    return 0;
                case IDM_OPTIONS:
                    if (currentModelIsZTP) loadZTPModel(currentFolder, currentFile, hwnd);
                    else if (!currentFile.empty()) loadModel(currentFolder, currentFile, hwnd, currentModelIsObj);
                    else showOptionsDialog(hwnd, &loadOptions);
                    return 0;
                case IDM_FILE_EXIT:
                    SendMessageA(hwnd, WM_CLOSE, 0, 0);
                    return 0;
                case IDM_FILE_SAVE:
                case IDM_FILE_SAVE_AS:
                    if (currentModelIsZTP)
                        saveCurrentZTP(hwnd, LOWORD(wParam) == IDM_FILE_SAVE_AS);
                    else
                        saveConvertedModel(hwnd, LOWORD(wParam) == IDM_FILE_SAVE_AS);
                    SetFocus(hwnd);
                    return 0;
                case IDM_EXPORT_SKELETON:
                    exportCurrentSkeleton(hwnd);
                    SetFocus(hwnd);
                    return 0;
                case IDC_WIREFRAME:
                    if (HIWORD(wParam) == BN_CLICKED)
                    {
                        if (IsDlgButtonChecked(hwnd, IDC_WIREFRAME) == BST_CHECKED) DisplayTEST |= 1;
                        else DisplayTEST &= ~1;
                        SetFocus(hwnd);
                    }
                    return 0;
                case IDC_SELF_SHADOWING:
                    if (HIWORD(wParam) == BN_CLICKED)
                    {
                        enableSelfShadowing = IsDlgButtonChecked(hwnd, IDC_SELF_SHADOWING) == BST_CHECKED;
                        resetSelfShadowing();
                        SetFocus(hwnd);
                    }
                    return 0;
                case IDC_BONE_PREVIEW:
                    if (HIWORD(wParam) == BN_CLICKED && currentModel)
                    {
                        const bool enabled = IsDlgButtonChecked(hwnd, IDC_BONE_PREVIEW) == BST_CHECKED;
                        if (!setFbxBonePreview(currentModel, enabled))
                            CheckDlgButton(hwnd, IDC_BONE_PREVIEW, BST_UNCHECKED);
                        extern int enableInterpolation;
                        applyAnimationPose(currentModel, PLAYER[0].ANIMCTRL, true, enableInterpolation != 0);
                        calcLight(currentModel);
                        resetSelfShadowing();
                        SetFocus(hwnd);
                    }
                    return 0;
                case IDC_LOD_GENERATE:
                    if (HIWORD(wParam) == BN_CLICKED && currentModel)
                    {
                        unsigned int reduction = SendDlgItemMessageA(hwnd, IDC_LOD_REDUCTION, TBM_GETPOS, 0, 0);
                        unsigned int replaceLevel = 0;
                        bool replaceBase = false, restoreBase = false;
                        if (!chooseLodGeneration(hwnd, reduction, &replaceLevel, &replaceBase, &restoreBase))
                        {
                            SetFocus(hwnd);
                            return 0;
                        }
                        HCURSOR previousCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
                        const unsigned int previousTextureCount = currentModel->nbTextures;
                        bool generated = restoreBase ? restoreAutomaticLodBase(currentModel) :
                            replaceBase ? replaceAutomaticLodBase(currentModel, reduction) :
                                          generateAutomaticLod(currentModel, reduction, replaceLevel);
                        SetCursor(previousCursor);
                        if (generated && (replaceBase || restoreBase))
                            refreshModelTextures(currentModel, previousTextureCount);
                        if (replaceBase || restoreBase)
                        {
                            ztpBaseReplaced = replaceBase && generated;
                            if (currentModelIsZTP)
                            {
                                ztpBasePose.resize(currentModel->nbModels);
                                for (unsigned int mesh = 0; mesh < currentModel->nbModels; ++mesh)
                                {
                                    const model_t & part = currentModel->model[mesh];
                                    if (part.nbPoint)
                                        ztpBasePose[mesh].assign(part.pntbl, part.pntbl + part.nbPoint);
                                    else ztpBasePose[mesh].clear();
                                }
                            }
                            if (generated)
                            {
                                calcLight(currentModel);
                                if (!skinVariants.empty()) rebuildSkinVariants(hwnd);
                            }
                        }
                        resetSelfShadowing();
                        unsigned int selected = 0;
                        if (generated && !restoreBase && currentModel->lodData)
                            for (unsigned int level = 0; level < currentModel->lodData->levels.size(); ++level)
                                if (currentModel->lodData->levels[level].reduction == reduction) selected = level + 1;
                        refreshLodControls(hwnd, selected);
                        clearSelectedSurfaces();
                        SetFocus(hwnd);
                    }
                    return 0;
                case IDC_LOD_DELETE:
                    if (HIWORD(wParam) == BN_CLICKED && currentModel)
                    {
                        LRESULT selected = SendDlgItemMessageA(hwnd, IDC_LOD_LEVEL, CB_GETCURSEL, 0, 0);
                        if (selected <= 0 || static_cast<unsigned int>(selected) > automaticLodLevelCount(currentModel))
                            return 0;
                        char question[96];
                        sprintf(question, "Delete LOD %u (%u%%)?", static_cast<unsigned int>(selected),
                            currentModel->lodData->levels[selected - 1].reduction);
                        if (MessageBoxA(hwnd, question, "Delete LOD", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES &&
                            deleteAutomaticLod(currentModel, static_cast<unsigned int>(selected)))
                        {
                            refreshLodControls(hwnd, MIN(static_cast<unsigned int>(selected), automaticLodLevelCount(currentModel)));
                            resetSelfShadowing();
                            clearSelectedSurfaces();
                        }
                        SetFocus(hwnd);
                    }
                    return 0;
                case IDC_LOD_MERGE_QUADS:
                    if (HIWORD(wParam) == BN_CLICKED && currentModel)
                    {
                        HCURSOR previousCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
                        const unsigned int previousTextureCount = currentModel->nbTextures;
                        const bool merged = mergeBaseTrianglesIntoQuads(currentModel);
                        SetCursor(previousCursor);
                        if (merged)
                        {
                            refreshModelTextures(currentModel, previousTextureCount);
                            ztpBaseReplaced = currentModelIsZTP;
                            resetAutomaticLod();
                            CheckDlgButton(hwnd, IDC_LOD_SHOW, BST_UNCHECKED);
                            calcLight(currentModel);
                            resetSelfShadowing();
                            refreshLodControls(hwnd);
                            clearSelectedSurfaces();
                            if (!skinVariants.empty()) rebuildSkinVariants(hwnd);
                        }
                        else MessageBoxA(hwnd, "No compatible triangle pairs were found.", "Convert tris to quads",
                                         MB_OK | MB_ICONINFORMATION);
                        SetFocus(hwnd);
                    }
                    return 0;
                case IDC_LOD_SHOW:
                    if (HIWORD(wParam) == BN_CLICKED)
                    {
                        const bool show = IsDlgButtonChecked(hwnd, IDC_LOD_SHOW) == BST_CHECKED;
                        LRESULT selected = SendDlgItemMessageA(hwnd, IDC_LOD_LEVEL, CB_GETCURSEL, 0, 0);
                        refreshLodControls(hwnd, show ? static_cast<unsigned int>(MAX(1, selected)) : 0);
                        resetSelfShadowing();
                        clearSelectedSurfaces();
                        SetFocus(hwnd);
                    }
                    return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED && handleSurfaceCommand(hwnd, LOWORD(wParam)))
                return 0;
            break;
        }

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

void EnableOpenGL(HWND hwnd, HDC* hDC, HGLRC* hRC)
{
    PIXELFORMATDESCRIPTOR pfd;

    int iFormat;

    /* get the device context (DC) */
    *hDC = GetDC(hwnd);

    /* set the pixel format for the DC */
    ZeroMemory(&pfd, sizeof(pfd));

    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW |
                  PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    iFormat = ChoosePixelFormat(*hDC, &pfd);

    SetPixelFormat(*hDC, iFormat, &pfd);

    /* create and enable the render context (RC) */
    *hRC = wglCreateContext(*hDC);

    wglMakeCurrent(*hDC, *hRC);
}
void DisableOpenGL (HWND hwnd, HDC hDC, HGLRC hRC)
{
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC);
    ReleaseDC(hwnd, hDC);
}
