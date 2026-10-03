
#include "SRC/COMMON.H"
#include <commdlg.h>
#include <commctrl.h>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <cmath>

LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
void EnableOpenGL(HWND hwnd, HDC*, HGLRC*);
void DisableOpenGL(HWND, HDC, HGLRC);
void deleteModels(animated_model_t * model);
extern int USE_SGL;
extern player_t PLAYER[4];
extern unsigned int specialTextureId[256];
extern unsigned int asciiTextureId;
extern unsigned int texId[1280];
extern void setTextures(animated_model_t *);
extern void resetPlayer(player_t *);
extern animated_model_t * loadingConfigured(unsigned int, int, float, uint32_t, uint32_t, string, string);
extern int activeFace;
extern int activeModel;
extern int playAnimation;
extern int DisplayTEST;
extern bool enableSelfShadowing;
extern void resetAnimationPlaybackTiming();
extern void setLightOrientation(float azimuthDegrees, float elevationDegrees);
extern void calcLight(animated_model_t *);
extern float defLight[3];
extern HWND hwnd;
float animationPlaybackSpeed = 1.0f;
float cameraMovementSpeed = 1.0f;

namespace
{
const int IDM_FILE_OPEN = 1001;
const int IDM_FILE_EXIT = 1002;
const int IDM_OPTIONS = 1101;
const int SIDE_PANEL_WIDTH = ZTP_SIDE_PANEL_WIDTH;
const int IDC_FRAME_COUNT = 2001;
const int IDC_SGL_MODE = 2002;
const int IDC_SCALE = 2003;
const int IDC_USE_UV = 2004;
const int IDC_FORCE_4BPP = 2005;
const int IDC_POWER_OF_8 = 2006;
const int IDC_OPEN_ANOTHER = 2101;
const int IDC_CURRENT_FILE = 2102;
const int IDC_SURFACE_STATUS = 2103;
const int IDC_SELECTION_HELP = 2104;
const int IDC_SURFACE_DOUBLE_SIDED = 2110;
const int IDC_SURFACE_UV_DIRECTION = 2111;
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
const int IDC_MODEL_GROUP = 2130;
const int IDC_SURFACE_GROUP = 2131;
const int IDC_ANIMATION_SPEED_LABEL = 2140;
const int IDC_ANIMATION_SPEED = 2141;
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
const float animationSpeedValues[] = {0.0f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f};
const float cameraSpeedValues[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};

struct LoadOptions
{
    int frameCount;
    int sglMode;
    float scale;
    bool useUv;
    uint32_t specialOptions;
};

LoadOptions loadOptions = {100, 0, 1.0f, true, 1};
animated_model_t * currentModel = NULL;
string currentFolder;
string currentFile;
bool glReady = false;
uint32_t shownSelectionRevision = 0xFFFFFFFFu;

void resetModelAnimation(animated_model_t * model)
{
    resetAutomaticLod();
    CheckDlgButton(hwnd, IDC_LOD_SHOW, BST_UNCHECKED);
    EnableWindow(GetDlgItem(hwnd, IDC_LOD_SHOW), FALSE);
    EnableWindow(GetDlgItem(hwnd, IDC_LOD_GENERATE), TRUE);
    SetDlgItemTextA(hwnd, IDC_LOD_STATUS, "LOD: not generated");
    ZeroMemory(&PLAYER[0].ANIMCTRL, sizeof(PLAYER[0].ANIMCTRL));
    PLAYER[0].ANIMCTRL.fps = 2;
    PLAYER[0].ANIMCTRL.endFrm = model->nbFrames > 0xFFFFu ? 0xFFFFu : static_cast<uint16_t>(model->nbFrames);
    playAnimation = 1;
    resetAnimationPlaybackTiming();
    initializeLightSource(model);
    resetSelfShadowing();
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
}

void createSidePanel(HWND parent)
{
    addControl(parent, "BUTTON", "Model", BS_GROUPBOX, 0, 0, 10, 10, IDC_MODEL_GROUP);
    addControl(parent, "BUTTON", "Surface options", BS_GROUPBOX, 0, 0, 10, 10, IDC_SURFACE_GROUP);
    addControl(parent, "BUTTON", "Open another OBJ...", BS_PUSHBUTTON, 0, 0, 10, 10, IDC_OPEN_ANOTHER);
    addControl(parent, "STATIC", "No model loaded", SS_LEFT, 0, 0, 10, 10, IDC_CURRENT_FILE);
    addControl(parent, "STATIC", "Select a polygon to edit", SS_LEFT, 0, 0, 10, 10, IDC_SURFACE_STATUS);
    addControl(parent, "STATIC", "Ctrl+click: add/remove | Ctrl+A: all", SS_LEFT, 0, 0, 10, 10, IDC_SELECTION_HELP);
    addControl(parent, "BUTTON", "Double-sided", BS_3STATE, 0, 0, 10, 10, IDC_SURFACE_DOUBLE_SIDED);
    addControl(parent, "BUTTON", "Reverse UV direction", BS_3STATE, 0, 0, 10, 10, IDC_SURFACE_UV_DIRECTION);
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
    addControl(parent, "STATIC", "Animation speed", SS_LEFT, 0, 0, 10, 10, IDC_ANIMATION_SPEED_LABEL);
    addControl(parent, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP, 0, 0, 10, 10, IDC_ANIMATION_SPEED);
    addControl(parent, "STATIC", "Camera speed", SS_LEFT, 0, 0, 10, 10, IDC_CAMERA_SPEED_LABEL);
    addControl(parent, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP, 0, 0, 10, 10, IDC_CAMERA_SPEED);
    addControl(parent, "BUTTON", "Wireframe", BS_AUTOCHECKBOX | WS_TABSTOP, 0, 0, 10, 10, IDC_WIREFRAME);
    addControl(parent, "BUTTON", "Self-shadowing", BS_AUTOCHECKBOX | WS_TABSTOP, 0, 0, 10, 10, IDC_SELF_SHADOWING);
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
    addControl(parent, "BUTTON", "Generate LOD", BS_PUSHBUTTON | WS_TABSTOP, 0, 0, 10, 10, IDC_LOD_GENERATE);
    addControl(parent, "BUTTON", "Show LOD", BS_AUTOCHECKBOX | WS_TABSTOP, 0, 0, 10, 10, IDC_LOD_SHOW);
    addControl(parent, "STATIC", "LOD: not generated", SS_LEFT, 0, 0, 10, 10, IDC_LOD_STATUS);
    EnableWindow(GetDlgItem(parent, IDC_LOD_SHOW), FALSE);
    EnableWindow(GetDlgItem(parent, IDC_LOD_GENERATE), FALSE);
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
        IDC_OPEN_ANOTHER, IDC_CURRENT_FILE, IDC_SURFACE_STATUS, IDC_SELECTION_HELP,
        IDC_SURFACE_DOUBLE_SIDED, IDC_SURFACE_UV_DIRECTION, IDC_SURFACE_MESH,
        IDC_SURFACE_GOURAUD, IDC_SURFACE_LIGHT, IDC_SURFACE_DEPTH,
        IDC_SURFACE_PALETTE, IDC_SURFACE_NEAR_CLIP, IDC_SURFACE_REALTIME_GOURAUD,
        IDC_SURFACE_FLIP, IDC_SURFACE_ROTATE, IDC_SURFACE_SORT, IDC_SORT_STATUS,
        IDC_ANIMATION_SPEED_LABEL, IDC_ANIMATION_SPEED, IDC_CAMERA_SPEED_LABEL, IDC_CAMERA_SPEED,
        IDC_WIREFRAME, IDC_SELF_SHADOWING, IDC_LIGHT_AZIMUTH_LABEL, IDC_LIGHT_AZIMUTH,
        IDC_LIGHT_ELEVATION_LABEL, IDC_LIGHT_ELEVATION,
        IDC_LOD_REDUCTION_LABEL, IDC_LOD_REDUCTION, IDC_LOD_GENERATE, IDC_LOD_SHOW, IDC_LOD_STATUS
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
        {IDC_SURFACE_STATUS, 20, 154, width - 24, 32},
        {IDC_SURFACE_DOUBLE_SIDED, 20, 187, width - 24, 20},
        {IDC_SURFACE_UV_DIRECTION, 20, 208, width - 24, 20},
        {IDC_SURFACE_MESH, 20, 229, width - 24, 20},
        {IDC_SURFACE_GOURAUD, 20, 250, width - 24, 20},
        {IDC_SURFACE_LIGHT, 20, 271, width - 24, 20},
        {IDC_SURFACE_DEPTH, 20, 292, width - 24, 20},
        {IDC_SURFACE_PALETTE, 20, 313, width - 24, 20},
        {IDC_SURFACE_NEAR_CLIP, 20, 334, width - 24, 20},
        {IDC_SURFACE_REALTIME_GOURAUD, 20, 355, width - 24, 20},
        {IDC_SURFACE_FLIP, 20, 382, (width - 36) / 2, 24},
        {IDC_SURFACE_ROTATE, 24 + (width - 36) / 2, 382, (width - 36) / 2, 24},
        {IDC_SURFACE_SORT, 20, 412, width - 24, 24},
        {IDC_SORT_STATUS, 20, 440, width - 24, 20},
        {IDC_ANIMATION_SPEED_LABEL, 20, 475, 160, 22},
        {IDC_ANIMATION_SPEED, 190, 472, width - 194, 200},
        {IDC_CAMERA_SPEED_LABEL, 20, 507, 160, 22},
        {IDC_CAMERA_SPEED, 190, 504, width - 194, 200},
        {IDC_WIREFRAME, 20, 539, 146, 22},
        {IDC_SELF_SHADOWING, 174, 539, 146, 22},
        {IDC_LIGHT_AZIMUTH_LABEL, 20, 563, 146, 20},
        {IDC_LIGHT_AZIMUTH, 20, 583, 146, 26},
        {IDC_LIGHT_ELEVATION_LABEL, 174, 563, 146, 20},
        {IDC_LIGHT_ELEVATION, 174, 583, 146, 26},
        {IDC_LOD_REDUCTION_LABEL, 20, 614, 146, 22},
        {IDC_LOD_REDUCTION, 174, 611, 146, 26},
        {IDC_LOD_GENERATE, 20, 642, 146, 26},
        {IDC_LOD_SHOW, 174, 642, 146, 26},
        {IDC_LOD_STATUS, 20, 673, width - 24, 30}
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

void updateSurfacePanel(HWND parent, animated_model_t * model)
{
    const int editControls[] = {
        IDC_SURFACE_DOUBLE_SIDED, IDC_SURFACE_UV_DIRECTION, IDC_SURFACE_MESH,
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
        shownSelectionRevision = surfaceSelectionRevision();
        return;
    }

    char status[96];
    sprintf(status, "Model %d  |  Polygon %d  |  %u selected", activeModel + 1, activeFace + 1, selectedSurfaceCount());
    SetDlgItemTextA(parent, IDC_SURFACE_STATUS, status);
    const char * sortingModes[] = {"SORT_BFR", "SORT_MIN", "SORT_MAX", "SORT_CEN"};
    char sortingStatus[48];
    int sortMode = selectedSortMode(model);
    sprintf(sortingStatus, "Sort mode: %s", sortMode < 0 ? "MIXED" : sortingModes[sortMode]);
    SetDlgItemTextA(parent, IDC_SORT_STATUS, sortingStatus);
    const int optionControls[] = {
        IDC_SURFACE_DOUBLE_SIDED, IDC_SURFACE_UV_DIRECTION, IDC_SURFACE_MESH,
        IDC_SURFACE_GOURAUD, IDC_SURFACE_LIGHT, IDC_SURFACE_DEPTH,
        IDC_SURFACE_PALETTE, IDC_SURFACE_NEAR_CLIP, IDC_SURFACE_REALTIME_GOURAUD
    };
    const surface_edit_operation operations[] = {
        SURFACE_TOGGLE_DOUBLE_SIDED, SURFACE_TOGGLE_UV_DIRECTION, SURFACE_TOGGLE_MESH,
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
        case IDC_SURFACE_UV_DIRECTION: operation = SURFACE_TOGGLE_UV_DIRECTION; break;
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
    if (controlId >= IDC_SURFACE_DOUBLE_SIDED && controlId <= IDC_SURFACE_REALTIME_GOURAUD)
        setSelectedSurfaceOption(currentModel, operation, selectedSurfaceOptionState(currentModel, operation) != 1);
    else
        editSelectedSurface(currentModel, operation);
    updateSurfacePanel(parent, currentModel);
    return true;
}

INT_PTR CALLBACK OptionsDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    LoadOptions * options = reinterpret_cast<LoadOptions *>(GetWindowLongPtr(dialog, GWLP_USERDATA));
    if (message == WM_INITDIALOG)
    {
        options = reinterpret_cast<LoadOptions *>(lParam);
        SetWindowLongPtr(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(options));
        addControl(dialog, "STATIC", "Animation frames", 0, 10, 12, 120, 18, -1);
        addControl(dialog, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL, 138, 10, 100, 22, IDC_FRAME_COUNT);
        addControl(dialog, "STATIC", "SGL mode", 0, 10, 40, 120, 18, -1);
        HWND sglCombo = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST,
                                        138, 38, 150, 80, dialog,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SGL_MODE)),
                                        reinterpret_cast<HINSTANCE>(GetWindowLongPtr(dialog, GWLP_HINSTANCE)), NULL);
        SendMessageA(sglCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Disabled"));
        SendMessageA(sglCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Enabled"));
        SendMessageA(sglCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Palette export"));
        addControl(dialog, "STATIC", "Scale", 0, 10, 68, 120, 18, -1);
        addControl(dialog, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL, 138, 66, 100, 22, IDC_SCALE);
        addControl(dialog, "BUTTON", "Generate textures from UV", BS_AUTOCHECKBOX, 10, 96, 240, 20, IDC_USE_UV);
        addControl(dialog, "BUTTON", "Force 4 BPP", BS_AUTOCHECKBOX, 10, 120, 160, 20, IDC_FORCE_4BPP);
        addControl(dialog, "BUTTON", "Power-of-8 UV texture height", BS_AUTOCHECKBOX, 10, 144, 250, 20, IDC_POWER_OF_8);
        addControl(dialog, "BUTTON", "OK", BS_DEFPUSHBUTTON, 108, 174, 70, 24, IDOK);
        addControl(dialog, "BUTTON", "Cancel", 0, 184, 174, 70, 24, IDCANCEL);

        char value[32];
        sprintf(value, "%d", options->frameCount);
        SetDlgItemTextA(dialog, IDC_FRAME_COUNT, value);
        sprintf(value, "%.6g", options->scale);
        SetDlgItemTextA(dialog, IDC_SCALE, value);
        SendDlgItemMessageA(dialog, IDC_SGL_MODE, CB_SETCURSEL, options->sglMode, 0);
        CheckDlgButton(dialog, IDC_USE_UV, options->useUv ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(dialog, IDC_FORCE_4BPP, (options->specialOptions & 1) ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(dialog, IDC_POWER_OF_8, (options->specialOptions & 2) ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;
    }
    if (message == WM_COMMAND && options)
    {
        if (LOWORD(wParam) == IDOK)
        {
            char frameText[32] = {};
            char scaleText[64] = {};
            GetDlgItemTextA(dialog, IDC_FRAME_COUNT, frameText, sizeof(frameText));
            GetDlgItemTextA(dialog, IDC_SCALE, scaleText, sizeof(scaleText));
            char * frameEnd = NULL;
            char * scaleEnd = NULL;
            long frameCount = std::strtol(frameText, &frameEnd, 10);
            float scale = std::strtof(scaleText, &scaleEnd);
            int sglMode = static_cast<int>(SendDlgItemMessageA(dialog, IDC_SGL_MODE, CB_GETCURSEL, 0, 0));
            if (frameEnd == frameText || *frameEnd != '\0' || frameCount < 0 || frameCount > 256 ||
                scaleEnd == scaleText || *scaleEnd != '\0' || !std::isfinite(scale) || scale <= 0.0f ||
                sglMode < 0 || sglMode > 2)
            {
                MessageBoxA(dialog, "Enter 0-256 frames and a positive scale value.", "Invalid options", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            options->frameCount = static_cast<int>(frameCount);
            options->sglMode = sglMode;
            options->scale = scale;
            options->useUv = IsDlgButtonChecked(dialog, IDC_USE_UV) == BST_CHECKED;
            options->specialOptions = (IsDlgButtonChecked(dialog, IDC_FORCE_4BPP) == BST_CHECKED ? 1u : 0u) |
                                      (IsDlgButtonChecked(dialog, IDC_POWER_OF_8) == BST_CHECKED ? 2u : 0u);
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

bool showOptionsDialog(HWND owner, LoadOptions * options)
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
    templateData.dialog.cx = 250;
    templateData.dialog.cy = 205;
    lstrcpyA(templateData.title, "Converter options");
    return DialogBoxIndirectParamA(GetModuleHandle(NULL), &templateData.dialog, owner,
                                   OptionsDialogProc, reinterpret_cast<LPARAM>(options)) == IDOK;
}

void releaseModelTextures(animated_model_t * model)
{
    if (!model) return;
    if (model->nbTextures) glDeleteTextures(model->nbTextures, texId);
    glDeleteTextures(256, specialTextureId);
    glDeleteTextures(1, &asciiTextureId);
}

bool loadModel(const string & folder, const string & fileName, HWND owner)
{
    LoadOptions selectedOptions = loadOptions;
    if (!showOptionsDialog(owner, &selectedOptions)) return false;

    delete [] skin.pixel;
    skin.pixel = NULL;
    animated_model_t * nextModel = loadingConfigured(selectedOptions.frameCount, selectedOptions.sglMode,
        selectedOptions.scale, selectedOptions.useUv ? 1u : 0u, selectedOptions.specialOptions,
        folder, fileName);
    releaseModelTextures(currentModel);
    deleteModels(currentModel);
    currentModel = nextModel;
    loadOptions = selectedOptions;
    currentFolder = folder;
    currentFile = fileName;
    clearSelectedSurfaces();
    setTextures(currentModel);
    for (int player = 0; player < 4; ++player) resetPlayer(&PLAYER[player]);
    resetModelAnimation(currentModel);
    SetWindowTextA(owner, ("ZTP Converter - " + fileName).c_str());
    updateSurfacePanel(owner, currentModel);
    return true;
}

void openModelDialog(HWND owner)
{
    char filePath[MAX_PATH] = {};
    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = "Wavefront OBJ files (*.obj)\0*.obj\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = filePath;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = "obj";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&dialog)) return;

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
    if (extension != string::npos) fileName.resize(extension);
    loadModel(folder, fileName, owner);
}

HMENU createMainMenu()
{
    HMENU menu = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_OPEN, "Open OBJ...");
    AppendMenuA(fileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_EXIT, "Exit");
    AppendMenuA(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), "File");
    HMENU optionsMenu = CreatePopupMenu();
    AppendMenuA(optionsMenu, MF_STRING, IDM_OPTIONS, "Configure and reload...");
    AppendMenuA(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(optionsMenu), "Options");
    return menu;
}
}

void deleteModels(animated_model_t * model)
{
    if (!model) return;
    resetAutomaticLod();
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
extern int                      render(animated_model_t * aModel);
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
                           64, 64, 1360, 800, NULL, createMainMenu(), hInstance, NULL);
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
        arguments >> frameCount >> sglMode >> scale >> useUv >> specialOptions;
        loadOptions.frameCount = frameCount;
        loadOptions.sglMode = sglMode;
        loadOptions.scale = scale;
        loadOptions.useUv = useUv != 0;
        loadOptions.specialOptions = specialOptions;
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
            if (myControls(&PLAYER[0], currentModel) == -1)
            {
                bQuit = TRUE;
                break;
            }
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
            render(currentModel);
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
    if (currentModel)
    {
    extern string fileOutName;// = "OUT/" + fileIn;
    const char * path = "OUT/";
    CreateDirectory(path, NULL);
    cout << "NOW CREATING BINARY FILE " << fileOutName << ".ZTP\n";
    string binaryFileName = "OUT\\" + fileOutName + ".ZTP";  //Z-Treme character

    ofstream binFile(binaryFileName.c_str(), ios::out | ios::binary);
    if (!binFile.is_open()) {cout << "\nERROR WHILE WRITING BINARY FILE!\n";}
    else {
        extern write_model_binary(ofstream*file, animated_model_t*aModel);
        extern bool write_MOD_HEADER(string filename, animated_model_t * aModel);

        write_model_binary(&binFile, currentModel);
        cout << "Writing debug text file...\n";

        write_MOD_HEADER("OUT.TXT", currentModel);
        binFile.close();
        extern int writeShadows(string, animated_model_t*);
        writeShadows("SHADOWS.PNG", currentModel);
    }
    }
#endif // 1

    /* shutdown OpenGL */
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
            return 0;

        case WM_SIZE:
            layoutSidePanel(hwnd);
            if (glReady) resizeOpenGLView(hwnd);
            return 0;

        case WM_GETMINMAXINFO:
        {
            RECT minimum = {0, 0, SIDE_PANEL_WIDTH + 320, 710};
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
            if (LOWORD(wParam) == WA_INACTIVE && GetCapture() == hwnd) ReleaseCapture();
            return DefWindowProc(hwnd, uMsg, wParam, lParam);

        case WM_CLOSE:
            if (GetCapture() == hwnd) ReleaseCapture();
            PostQuitMessage(0);
        break;

        case WM_DESTROY:
            return 0;

        case WM_KEYDOWN:
        {
            switch (wParam)
            {
                case VK_ESCAPE:
                    PostQuitMessage(0);
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
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                int controlId = LOWORD(wParam);
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
            switch (LOWORD(wParam))
            {
                case IDM_FILE_OPEN:
                case IDC_OPEN_ANOTHER:
                    openModelDialog(hwnd);
                    return 0;
                case IDM_OPTIONS:
                    if (!currentFile.empty()) loadModel(currentFolder, currentFile, hwnd);
                    else showOptionsDialog(hwnd, &loadOptions);
                    return 0;
                case IDM_FILE_EXIT:
                    PostQuitMessage(0);
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
                case IDC_LOD_GENERATE:
                    if (HIWORD(wParam) == BN_CLICKED && currentModel)
                    {
                        unsigned int reduction = SendDlgItemMessageA(hwnd, IDC_LOD_REDUCTION, TBM_GETPOS, 0, 0);
                        HCURSOR previousCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
                        bool generated = generateAutomaticLod(currentModel, reduction);
                        SetCursor(previousCursor);
                        showAutomaticLod = generated;
                        resetSelfShadowing();
                        CheckDlgButton(hwnd, IDC_LOD_SHOW, generated ? BST_CHECKED : BST_UNCHECKED);
                        EnableWindow(GetDlgItem(hwnd, IDC_LOD_SHOW), generated);
                        unsigned int originalCount = 0;
                        unsigned int originalPoints = 0;
                        for (unsigned int mesh = 0; mesh < currentModel->nbModels; ++mesh)
                        {
                            originalCount += currentModel->model[mesh].nbPolygon;
                            originalPoints += currentModel->model[mesh].nbPoint;
                        }
                        char status[128];
                        unsigned int lodCount = automaticLodFaceCount();
                        if (generated)
                            sprintf(status, "LOD: %u -> %u faces (-%.0f%%)\r\nPoints: %u -> %u", originalCount, lodCount,
                                originalCount ? 100.0f * (originalCount - lodCount) / originalCount : 0.0f,
                                originalPoints, automaticLodPointCount());
                        else sprintf(status, "LOD generation failed");
                        SetDlgItemTextA(hwnd, IDC_LOD_STATUS, status);
                        clearSelectedSurfaces();
                        SetFocus(hwnd);
                    }
                    return 0;
                case IDC_LOD_SHOW:
                    if (HIWORD(wParam) == BN_CLICKED)
                    {
                        showAutomaticLod = IsDlgButtonChecked(hwnd, IDC_LOD_SHOW) == BST_CHECKED;
                        resetSelfShadowing();
                        clearSelectedSurfaces();
                        SetFocus(hwnd);
                    }
                    return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED && handleSurfaceCommand(hwnd, LOWORD(wParam)))
                return 0;
            break;

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

