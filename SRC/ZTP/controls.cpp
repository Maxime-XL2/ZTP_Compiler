#include "../COMMON.H"
#include <set>

/***
In case you are wondering, I reused code from my early FPS tests, so you can see stuff like "is_firing" which is obviously not for the model converter...
***/

extern int DisplayTEST;
extern int ZT_FRAMERATE;

#define KB_A 65
#define KB_D 68
#define KB_W 87
#define KB_S 83
#define KB_Z 90
#define KB_Q 81
#define KB_E 69
#define KB_C 67
#define KB_ESC 126

#define AslMulFX(a,b) (a*b)

sKeyState m_keys[256], m_mouse[5];
sKeyState GetKey(int nKeyID){ return m_keys[nKeyID]; }


short   m_keyNewState[256]    = { 0 };
short   m_keyOldState[256]    = { 0 };
POINT   p                     = { 0 };
VECTOR  m_pos                 = { 0 };

void resetKeyPresses(player_t * player)
{
    if (player->PLAYER_ID >= 4) return;

    for (int i = 0; i < 256; i++)
    {
        m_keyNewState[i] = GetAsyncKeyState(i);
		m_keys[i].bPressed = false;	m_keys[i].bReleased = false;

		if (m_keyNewState[i] != m_keyOldState[i])
		{
            if (m_keyNewState[i] & 0x8000)
			{
                m_keys[i].bPressed = !m_keys[i].bHeld;
				m_keys[i].bHeld = true;
            }
			else
			{
                m_keys[i].bReleased = true;
				m_keys[i].bHeld = false;
            }
        }
        m_keyOldState[i] = m_keyNewState[i];
    }

}


#define SPEED_INCREASE (0.5f*ZT_FRAMERATE)
#define FRICTION        (0.25f)
#define SPEED_VALUE     (3.0f)
#define TURN_INCREASE   (0.001375f * ZT_FRAMERATE)
#define LOOK_INCREASE   (0.001f * ZT_FRAMERATE)
/**Movement**/
void move_front(player_t * currentPlayer){   if ((currentPlayer->MOVEMENT_SPEED+=SPEED_INCREASE)>SPEED_VALUE)       currentPlayer->MOVEMENT_SPEED=SPEED_VALUE;}
void move_back(player_t * currentPlayer){    if ((currentPlayer->MOVEMENT_SPEED-=SPEED_INCREASE)<-SPEED_VALUE)      currentPlayer->MOVEMENT_SPEED=-SPEED_VALUE;}
void move_left(player_t * currentPlayer){    if ((currentPlayer->LATERAL_SPEED -= SPEED_INCREASE) < -SPEED_VALUE)   currentPlayer->LATERAL_SPEED = -SPEED_VALUE;}
void move_right(player_t * currentPlayer){   if ((currentPlayer->LATERAL_SPEED += SPEED_INCREASE) > SPEED_VALUE)    currentPlayer->LATERAL_SPEED = SPEED_VALUE;}

void move_friction(player_t * currentPlayer)
{
    if (currentPlayer->MOVEMENT_SPEED > SPEED_INCREASE)          currentPlayer->MOVEMENT_SPEED-=FRICTION*ZT_FRAMERATE;
    else if (currentPlayer->MOVEMENT_SPEED < -SPEED_INCREASE)   currentPlayer->MOVEMENT_SPEED+=FRICTION*ZT_FRAMERATE;
    else currentPlayer->MOVEMENT_SPEED = 0;
}


/**For the Lookup/down values : DEGtoANG(45) = 45/360 * 65536.0**/
void look_up(player_t * currentPlayer)   {    currentPlayer->ROTATION[Z]+=LOOK_INCREASE;    if (currentPlayer->ROTATION[Z] > 8192)  currentPlayer->ROTATION[Z] = 8192;}
void look_down(player_t * currentPlayer) {    currentPlayer->ROTATION[Z] -= LOOK_INCREASE;    if (currentPlayer->ROTATION[Z] < -8192) currentPlayer->ROTATION[Z] = -8192;}

/**Turn left/right - SATURN ONLY, DIGITAL PAD ONLY**/
void turn_left(player_t * currentPlayer){    currentPlayer->ROTATION[Z] -= TURN_INCREASE;}
void turn_right(player_t * currentPlayer){    currentPlayer->ROTATION[Z] += TURN_INCREASE;}


void resetPlayer(player_t * player)
{
    player->POSITION[X]=player->POSITION[Y]=player->POSITION[Z]=0;
    player->NEXT_POSITION[X]=player->NEXT_POSITION[Y]=player->NEXT_POSITION[Z]=0;
    player->IS_FIRING=0;
    player->LATERAL_SPEED=0;
    player->MOVEMENT_SPEED=0;
    player->ROTATION[X]=0;

    player->POSITION[Z]=128;
    player->ROTATION[Y]=player->ROTATION[Z]=-180.0;
}



void debugKeys(player_t * player)
{
    if (m_keys['X'].bPressed)
    {if (++DisplayTEST > 3) DisplayTEST=0;}

    if (m_keys[32].bPressed)
        resetPlayer(player);

    /*if (m_keys['F'].bPressed)
    {
        ZT_FRAMERATE++;
        if (ZT_FRAMERATE>4) ZT_FRAMERATE=1;
    }*/

    extern int enableInterpolation;
    if (m_keys['I'].bPressed)
        enableInterpolation ^= 1;
}

void mouse_controls(player_t * player, animated_model_t * model)
{
    static bool leftButtonWasDown = false;
    static bool rightButtonWasDown = false;
    static POINT previousCursor = {0, 0};
    bool leftButtonDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool rightButtonDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    bool leftButtonPressed = leftButtonDown && !leftButtonWasDown;
    leftButtonWasDown = leftButtonDown;
    extern HWND hwnd;
    RECT client;
    POINT cursor;
    if (!GetCursorPos(&cursor) || !ScreenToClient(hwnd, &cursor)) return;
    GetClientRect(hwnd, &client);
    int viewWidth = MAX(1, client.right - ZTP_SIDE_PANEL_WIDTH);
    bool insideView = cursor.x >= 0 && cursor.x < viewWidth && cursor.y >= 0 && cursor.y < client.bottom;

    if (rightButtonDown && GetCapture() == hwnd)
    {
        if (rightButtonWasDown)
        {
            player->ROTATION[Z] -= (cursor.x - previousCursor.x) * 0.0625f;
            player->ROTATION[X] += (cursor.y - previousCursor.y) * 0.0625f;
        }
        previousCursor = cursor;
        rightButtonWasDown = true;
    }
    else rightButtonWasDown = false;

    if (!insideView) return;

    if (leftButtonPressed) {
            glPushMatrix();

            glRotatef(player->ROTATION[X], 1.0f, 0.0f, 0.0f);
            glRotatef(player->ROTATION[Y], 0.0f, 1.0f, 0.0f);
            glTranslatef(-player->POSITION[X], -player->POSITION[Y], -player->POSITION[Z]);
            glScalef(model->scale[X],model->scale[Y], model->scale[Z]);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            extern int activeModel;
    //extern unsigned int texId[];
    extern int activeFace;
    extern unsigned int specialTextureId[];

            glDisable(GL_CULL_FACE);
            glDisable(GL_POLYGON_STIPPLE);
            //glDisable(GL_TEXTURE);

        for (int j=0; j<(int)model->nbModels; j++){
            model_t * curModel = &model->model[j];
            vertex_t * curVert;
            glColor4f(1,1,1,1);
            glBindTexture(GL_TEXTURE_2D, specialTextureId[j]);
            for (int i=0; i<(int)model->model[j].nbPolygon; i++){
           // glColor4f((i&255)/255.0,((i>>8)&255)/255.0,((i>>16)&255)/255.0,1);
            //glColor4ub((i&255),((i>>8)&255),((i>>16)&255),255);

                glBegin(GL_QUADS);
                {
                    for (int32_t ii=3; ii>=0; --ii) {
                        curVert = &curModel->pntbl[curModel->pltbl[i].vertIdx[ii]];
                        glTexCoord2f((i&255)/255.0f, (((i>>8)&255))/ 255.0);
                        glVertex3f(curVert->point[X], curVert->point[Y], curVert->point[Z]);
                    }
                }
                glEnd();
            }
        }

        glFlush();
        glFinish();

        unsigned char data[4];
        glReadPixels(cursor.x, client.bottom - 1 - cursor.y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
       // cout << ((*(int32_t*)&data[0])&255) << " " << (((*(int32_t*)&data[0])>>8)&255) << " " << (((*(int32_t*)&data[0])>>16)&255) << " " << (((*(int32_t*)&data[0])>>24)&255) << "\n";

        activeModel = ((*(int32_t*)&data)>>16)&255;
        if (activeModel >=0 && activeModel < (int)model->nbModels) {
            activeFace=((*(int32_t*)&data)&255) | ((((*(int32_t*)&data)>>8)&255)<<8);
            if (activeFace >= (int)model->model[activeModel].nbPolygon || activeFace<0) activeFace=-1;
        } else {
            activeModel=0;
            activeFace=-1;
        }
        bool additive = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        selectSurface(model, activeModel, activeFace, additive);

    glPopMatrix();
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }
}

void rotatePol(polygon_t * pol){
    unsigned int idxArray[4];
    uv_t         vertUV[4];

    for (int i=0; i<4; i++){
        idxArray[i]=pol->vertIdx[i];
        vertUV[i].U = pol->vertUV[i].U;
        vertUV[i].V = pol->vertUV[i].V;
    }
    for (int i=0; i<4; i++){
        int idx = (i+1)&3;
        pol->vertIdx[i]=idxArray[idx];
        pol->vertUV[i].U=vertUV[idx].U;
        pol->vertUV[i].V=vertUV[idx].V;
    }
}

void textureFlip(polygon_t * pol){
    pol->SGL_ATTR.dir=(pol->SGL_ATTR.dir&0xFFCF)|
        ((pol->SGL_ATTR.dir+(1<<4))&0x0030);
}

namespace
{
std::set<std::pair<int, int> > selectedSurfaces;
uint32_t selectionRevision = 0;

polygon_t * getSurface(animated_model_t * model, int modelIndex, int faceIndex)
{
    if (!model || modelIndex < 0 || modelIndex >= static_cast<int>(model->nbModels) ||
        faceIndex < 0 || faceIndex >= static_cast<int>(model->model[modelIndex].nbPolygon))
        return NULL;
    return &model->model[modelIndex].pltbl[faceIndex];
}

bool surfaceOptionValue(const polygon_t * polygon, surface_edit_operation operation)
{
    switch (operation)
    {
        case SURFACE_TOGGLE_DOUBLE_SIDED: return (polygon->SGL_ATTR.flag & Dual_Plane) != 0;
        case SURFACE_TOGGLE_UV_DIRECTION: return (polygon->SGL_ATTR.dir & (1 << 8)) != 0;
        case SURFACE_TOGGLE_MESH: return (polygon->SGL_ATTR.atrb & MESHon) != 0;
        case SURFACE_TOGGLE_GOURAUD: return (polygon->SGL_ATTR.atrb & CL_Gouraud) != 0;
        case SURFACE_TOGGLE_LIGHT: return (polygon->SGL_ATTR.sorting & UseLight) != 0;
        case SURFACE_TOGGLE_DEPTH: return (polygon->SGL_ATTR.sorting & UseDepth) != 0;
        case SURFACE_TOGGLE_PALETTE: return (polygon->SGL_ATTR.sorting & UsePalette) != 0;
        case SURFACE_TOGGLE_NEAR_CLIP: return (polygon->SGL_ATTR.sorting & UseNearClip) != 0;
        case SURFACE_TOGGLE_REALTIME_GOURAUD: return (polygon->SGL_ATTR.sorting & UseGouraud) != 0;
        default: return false;
    }
}

void assignBit(uint16_t & value, uint16_t mask, bool enabled)
{
    if (enabled) value |= mask;
    else value &= static_cast<uint16_t>(~mask);
}

void assignBit(uint8_t & value, uint8_t mask, bool enabled)
{
    if (enabled) value |= mask;
    else value &= static_cast<uint8_t>(~mask);
}

bool assignSurfaceOption(polygon_t * polygon, surface_edit_operation operation, bool enabled)
{
    if (!polygon) return false;
    switch (operation)
    {
        case SURFACE_TOGGLE_DOUBLE_SIDED:
            if (enabled) polygon->SGL_ATTR.flag |= Dual_Plane;
            else polygon->SGL_ATTR.flag &= static_cast<uint8_t>(~Dual_Plane);
            break;
        case SURFACE_TOGGLE_UV_DIRECTION: assignBit(polygon->SGL_ATTR.dir, 1 << 8, enabled); break;
        case SURFACE_TOGGLE_MESH: assignBit(polygon->SGL_ATTR.atrb, MESHon, enabled); break;
        case SURFACE_TOGGLE_GOURAUD: assignBit(polygon->SGL_ATTR.atrb, CL_Gouraud, enabled); break;
        case SURFACE_TOGGLE_LIGHT: assignBit(polygon->SGL_ATTR.sorting, UseLight, enabled); break;
        case SURFACE_TOGGLE_DEPTH: assignBit(polygon->SGL_ATTR.sorting, UseDepth, enabled); break;
        case SURFACE_TOGGLE_PALETTE: assignBit(polygon->SGL_ATTR.sorting, UsePalette, enabled); break;
        case SURFACE_TOGGLE_NEAR_CLIP: assignBit(polygon->SGL_ATTR.sorting, UseNearClip, enabled); break;
        case SURFACE_TOGGLE_REALTIME_GOURAUD: assignBit(polygon->SGL_ATTR.sorting, UseGouraud, enabled); break;
        default: return false;
    }
    return true;
}

bool applySurfaceOperation(polygon_t * polygon, surface_edit_operation operation)
{
    if (!polygon) return false;
    switch (operation)
    {
        case SURFACE_CYCLE_TEXTURE_FLIP: textureFlip(polygon); break;
        case SURFACE_ROTATE: rotatePol(polygon); break;
        case SURFACE_CYCLE_SORTING:
            polygon->SGL_ATTR.sorting = (polygon->SGL_ATTR.sorting & 0xFFFC) |
                                        ((polygon->SGL_ATTR.sorting + 1) & 3);
            break;
        default:
            return false;
    }
    return true;
}
}

void clearSelectedSurfaces()
{
    selectedSurfaces.clear();
    ++selectionRevision;
    extern int activeFace;
    extern int activeModel;
    activeFace = -1;
    activeModel = 0;
}

bool isSurfaceSelected(int modelIndex, int faceIndex)
{
    return selectedSurfaces.find(std::make_pair(modelIndex, faceIndex)) != selectedSurfaces.end();
}

unsigned int selectedSurfaceCount()
{
    return static_cast<unsigned int>(selectedSurfaces.size());
}

uint32_t surfaceSelectionRevision()
{
    return selectionRevision;
}

void selectSurface(animated_model_t * model, int modelIndex, int faceIndex, bool additive)
{
    extern int activeFace;
    extern int activeModel;
    if (!additive) selectedSurfaces.clear();
    if (!getSurface(model, modelIndex, faceIndex))
    {
        if (!additive) clearSelectedSurfaces();
        return;
    }

    std::pair<int, int> surface(modelIndex, faceIndex);
    std::set<std::pair<int, int> >::iterator selected = selectedSurfaces.find(surface);
    if (additive && selected != selectedSurfaces.end())
        selectedSurfaces.erase(selected);
    else
        selectedSurfaces.insert(surface);
    ++selectionRevision;

    if (selectedSurfaces.empty())
    {
        activeFace = -1;
        activeModel = 0;
    }
    else if (selectedSurfaces.find(surface) != selectedSurfaces.end())
    {
        activeModel = modelIndex;
        activeFace = faceIndex;
    }
    else
    {
        activeModel = selectedSurfaces.begin()->first;
        activeFace = selectedSurfaces.begin()->second;
    }
}

void selectAllSurfaces(animated_model_t * model)
{
    clearSelectedSurfaces();
    if (!model) return;
    for (unsigned int modelIndex = 0; modelIndex < model->nbModels; ++modelIndex)
        for (unsigned int faceIndex = 0; faceIndex < model->model[modelIndex].nbPolygon; ++faceIndex)
            selectedSurfaces.insert(std::make_pair(static_cast<int>(modelIndex), static_cast<int>(faceIndex)));
    if (!selectedSurfaces.empty())
    {
        extern int activeFace;
        extern int activeModel;
        activeModel = selectedSurfaces.begin()->first;
        activeFace = selectedSurfaces.begin()->second;
    }
    ++selectionRevision;
}

int selectedSurfaceOptionState(animated_model_t * model, surface_edit_operation operation)
{
    switch (operation)
    {
        case SURFACE_TOGGLE_DOUBLE_SIDED:
        case SURFACE_TOGGLE_UV_DIRECTION:
        case SURFACE_TOGGLE_MESH:
        case SURFACE_TOGGLE_GOURAUD:
        case SURFACE_TOGGLE_LIGHT:
        case SURFACE_TOGGLE_DEPTH:
        case SURFACE_TOGGLE_PALETTE:
        case SURFACE_TOGGLE_NEAR_CLIP:
        case SURFACE_TOGGLE_REALTIME_GOURAUD:
            break;
        default:
            return -2;
    }

    bool found = false;
    bool firstValue = false;
    for (std::set<std::pair<int, int> >::const_iterator selected = selectedSurfaces.begin();
         selected != selectedSurfaces.end(); ++selected)
    {
        polygon_t * polygon = getSurface(model, selected->first, selected->second);
        if (!polygon) continue;
        bool value = surfaceOptionValue(polygon, operation);
        if (found && value != firstValue) return -1;
        firstValue = value;
        found = true;
    }
    if (!found)
    {
        extern int activeFace;
        extern int activeModel;
        polygon_t * polygon = getSurface(model, activeModel, activeFace);
        if (!polygon) return -2;
        return surfaceOptionValue(polygon, operation) ? 1 : 0;
    }
    return firstValue ? 1 : 0;
}

int selectedSortMode(animated_model_t * model)
{
    bool found = false;
    int sortMode = -1;
    for (std::set<std::pair<int, int> >::const_iterator selected = selectedSurfaces.begin();
         selected != selectedSurfaces.end(); ++selected)
    {
        polygon_t * polygon = getSurface(model, selected->first, selected->second);
        if (!polygon) continue;
        int value = polygon->SGL_ATTR.sorting & 3;
        if (found && value != sortMode) return -1;
        sortMode = value;
        found = true;
    }
    if (found) return sortMode;
    extern int activeFace;
    extern int activeModel;
    polygon_t * polygon = getSurface(model, activeModel, activeFace);
    return polygon ? polygon->SGL_ATTR.sorting & 3 : -1;
}

bool setSelectedSurfaceOption(animated_model_t * model, surface_edit_operation operation, bool enabled)
{
    bool changed = false;
    for (std::set<std::pair<int, int> >::const_iterator selected = selectedSurfaces.begin();
         selected != selectedSurfaces.end(); ++selected)
        changed = assignSurfaceOption(getSurface(model, selected->first, selected->second), operation, enabled) || changed;
    if (!selectedSurfaces.empty()) {
        if (changed) ++selectionRevision;
        return changed;
    }

    extern int activeFace;
    extern int activeModel;
    changed = assignSurfaceOption(getSurface(model, activeModel, activeFace), operation, enabled);
    if (changed) ++selectionRevision;
    return changed;
}

bool editSelectedSurface(animated_model_t * model, surface_edit_operation operation)
{
    int state = selectedSurfaceOptionState(model, operation);
    if (state != -2)
        return setSelectedSurfaceOption(model, operation, state != 1);

    bool changed = false;
    for (std::set<std::pair<int, int> >::const_iterator selected = selectedSurfaces.begin();
         selected != selectedSurfaces.end(); ++selected)
        changed = applySurfaceOperation(getSurface(model, selected->first, selected->second), operation) || changed;
    if (!selectedSurfaces.empty()) {
        if (changed) ++selectionRevision;
        return changed;
    }

    extern int activeFace;
    extern int activeModel;
    changed = applySurfaceOperation(getSurface(model, activeModel, activeFace), operation);
    if (changed) ++selectionRevision;
    return changed;
}

int keyboard_controls(player_t * player, animated_model_t * model)
{
    //glDisable(GL_TEXTURE);

    /*for (int i=0; i<model->model[0]->nbPolygon; i++){
        render
    }*/
    if (m_keys[27].bHeld || m_keys['O'].bHeld /*|| m_keys['Q'].bHeld*/) return -1;
        extern int activeFace;
        extern int activeModel;
    if (m_keys['P'].bPressed){
        extern int playAnimation;
        playAnimation^=1;
    }
    if (m_keys['L'].bPressed){
        extern int activeShading;
        activeShading^=1;
    }

    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && m_keys['A'].bPressed)
        selectAllSurfaces(model);

    /*if (m_keys['U'].bPressed) {
        if (++activeModel >= model->nbModels) activeModel=0;
        activeFace=-1;
    }*/



    if (m_keys[9].bPressed && model->model[activeModel].nbPolygon > 0){
        if (++activeFace >= (int)model->model[activeModel].nbPolygon) activeFace=0;
        selectSurface(model, activeModel, activeFace, false);
    } else if (m_keys[VK_SHIFT].bPressed && model->model[activeModel].nbPolygon > 0) {
        if (--activeFace<0) activeFace=model->model[activeModel].nbPolygon-1;
        selectSurface(model, activeModel, activeFace, false);
    }


    if (m_keys['C'].bPressed) editSelectedSurface(model, SURFACE_TOGGLE_DOUBLE_SIDED);
    if (m_keys['F'].bPressed) editSelectedSurface(model, SURFACE_CYCLE_TEXTURE_FLIP);
    if (m_keys['G'].bPressed) editSelectedSurface(model, SURFACE_TOGGLE_UV_DIRECTION);
    if (m_keys['R'].bPressed) editSelectedSurface(model, SURFACE_ROTATE);
    if (m_keys['M'].bPressed) editSelectedSurface(model, SURFACE_TOGGLE_MESH);
    if (m_keys['H'].bPressed) editSelectedSurface(model, SURFACE_TOGGLE_GOURAUD);
    if (m_keys['N'].bPressed) editSelectedSurface(model, SURFACE_CYCLE_SORTING);

    /**REMOVED FOR DEBUGGING**/
    /*if ((player->STATUS & CAN_JUMP)&&(m_keys[' '].bPressed || (GetKeyState(VK_RBUTTON) < 0)))
    {
        player->SPEED[Y] = -20.0f;
        //player->STATUS &= ~CAN_JUMP;
    }*/

    if (m_keys['D'].bHeld)
        move_right(player);
    else if (m_keys['A'].bHeld && !(GetAsyncKeyState(VK_CONTROL) & 0x8000))
        move_left(player);
    else if (player->LATERAL_SPEED > SPEED_INCREASE) player->LATERAL_SPEED-=SPEED_INCREASE;
    else if (player->LATERAL_SPEED < -SPEED_INCREASE) player->LATERAL_SPEED+=SPEED_INCREASE;
    else player->LATERAL_SPEED=0;

    if (fabs(player->LATERAL_SPEED)< FRICTION) player->LATERAL_SPEED=0;

    if (m_keys['W'].bHeld)
    {
        move_front(player);
    }
    else if (m_keys['S'].bHeld)
    {
        move_back(player);
    }
    else move_friction(player);

    /**For debugging, no collision**/
#if 1

    if (m_keys['Q'].bHeld)
        player->SPEED[Y]-=1.0f*ZT_FRAMERATE;
    else if (m_keys['Z'].bHeld)
        player->SPEED[Y]+=1.0f*ZT_FRAMERATE;
    else player->SPEED[Y]*=0.5f;
#endif

    if (fabs(player->SPEED[Y]) < 1.0f/32.0f) player->SPEED[Y]=0;

    debugKeys(player);

    return 0;
}


/**Update player's data**/
void update(player_t * player)
{
    extern float cameraMovementSpeed;
    player->ROTATION[Y] = player->ROTATION[Z];

    float forwardSpeed = player->MOVEMENT_SPEED * std::cos(player->ROTATION[X] * conv);
    float verticalForwardSpeed = player->MOVEMENT_SPEED * std::sin(player->ROTATION[X] * conv);

    player->SPEED[Z] = AslMulFX(std::cos(player->ROTATION[Y]*conv), forwardSpeed) +
        AslMulFX(std::sin(player->ROTATION[Y]*conv), (player->LATERAL_SPEED));

    player->SPEED[X] = -AslMulFX(std::sin(player->ROTATION[Y]*conv), forwardSpeed) +
        AslMulFX(std::cos(player->ROTATION[Y]*conv), (player->LATERAL_SPEED));

    if (player->SPEED[Y] > 15.0f) player->SPEED[Y] = 15.0f;
    float movementStep = ZT_FRAMERATE * cameraMovementSpeed;
    player->NEXT_POSITION[X] = player->POSITION[X] + player->SPEED[X] * movementStep;
    player->NEXT_POSITION[Y] = player->POSITION[Y] + (player->SPEED[Y] + verticalForwardSpeed) * movementStep;
    player->NEXT_POSITION[Z] = player->POSITION[Z] + player->SPEED[Z] * movementStep;

    //Disabled for debug
    //player->STATUS &= ~CAN_JUMP;
}
/**Temporary functions...**/
int myControls(player_t * player, animated_model_t*model)
{


   /* if (is_input_available(currentPlayer->PLAYER_ID) == false) return;

    if(Smpc_Peripheral[currentPlayer->PLAYER_ID].id == 0x16)
        {analog_control(currentPlayer);}
	else
        {digital_control(currentPlayer);}

    update(currentPlayer);*/


   // player->SPEED[Y] += 3.0f * ZT_FRAMERATE;

    mouse_controls(player, model);
    resetKeyPresses(player);
    if (keyboard_controls(player, model)==-1) return -1;

    update(player);
       //     cout << "Player Position : " << (int)player->POSITION.x << ", " << (int)player->POSITION.y << ", " << (int)player->POSITION.z << "\n";
    return 0;
}
