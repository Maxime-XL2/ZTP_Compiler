#include "../COMMON.H"
#include <limits>

extern player_t PLAYER[];
extern int ZT_FRAMERATE;


int testAnim = 0;

float SaturnUV[8*4] =
{
    0, 0,
    1, 0,
    1, 1,
    0, 1,

    1, 0,
    0, 0,
    0, 1,
    1, 1,

    0, 1,
    1, 1,
    1, 0,
    0, 0,

    1, 1,
    0, 1,
    0, 0,
    1, 0,
};


GLubyte meshEffectArray[]={
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
    15, 15, 15, 15, 240, 240, 240, 240,
//    85, 85, 85, 85, 170, 170, 170, 170,

};

//Selection ID
int activeModel = 0;
int activeFace = -1;
int playAnimation = 1;
int activeShading = 1;
extern float animationPlaybackSpeed;
static double animationAdvanceRemainder = 0.0;

void resetAnimationPlaybackTiming()
{
    animationAdvanceRemainder = 0.0;
}

void ztUpdateAnimation(animationControl_t * animCtrl, animated_model_t * currentModel)
{
    if (!animCtrl || !currentModel || !currentModel->keyFrames || currentModel->nbFrames == 0) return;

    //XPDATA * currentPDATA = currentModel->pol[0];
    //model_t * currentPDATA = currentModel->model[0];

    /**Sets the animation data**/
    uint32_t FPS = MIN(animCtrl->fps, 10);
    uint32_t frameUnit = 1u << FPS;
    float percent = 1.0f / static_cast<float>(frameUnit);
    uint32_t startFrame = animCtrl->startFrm;
    uint32_t endFrame = MIN(animCtrl->endFrm, currentModel->nbFrames);
    if (endFrame <= startFrame) return;

    uint32_t rangeStart = startFrame * frameUnit;
    uint32_t rangeEnd = endFrame * frameUnit;
    uint32_t currentFrame = animCtrl->currentFrm;
    if (currentFrame < rangeStart || currentFrame >= rangeEnd) currentFrame = rangeStart;
    animationAdvanceRemainder += ZT_FRAMERATE * static_cast<double>(animationPlaybackSpeed);
    uint32_t advance = static_cast<uint32_t>(animationAdvanceRemainder);
    animationAdvanceRemainder -= advance;
    currentFrame += advance;
    if (currentFrame >= rangeEnd)
        currentFrame = rangeStart + (currentFrame - rangeStart) % (rangeEnd - rangeStart);
    animCtrl->currentFrm = static_cast<uint16_t>(currentFrame);
    animCtrl->fps = static_cast<uint16_t>(FPS);

    animCtrl->currentKeyFrm = static_cast<uint16_t>(currentFrame >> FPS);

    uint16_t nextKeyFrm=animCtrl->currentKeyFrm+1;
    if (nextKeyFrm>=animCtrl->endFrm)
        nextKeyFrm=animCtrl->startFrm;


    vertex_t * curKeyFrame = (vertex_t*)currentModel->keyFrames[animCtrl->currentKeyFrm].cVert;
    vertex_t * nextKeyFrame = (vertex_t*)currentModel->keyFrames[nextKeyFrm].cVert;

    if (!curKeyFrame || !nextKeyFrame) return;
    uint32_t interpFctr = currentFrame - (animCtrl->currentKeyFrm * frameUnit);

    extern int enableInterpolation;
    if (!enableInterpolation) {interpFctr=0;}
    unsigned int j=0;
    for (unsigned int i=0; i<currentModel->nbModels; i++)
        {
            model_t * currentPDATA = &currentModel->model[i];
            for (unsigned int ii=0; ii<currentModel->model[i].nbPoint; ii++)
            {
                vertex_t *dst=(vertex_t*)&currentPDATA->pntbl[ii];
                vertex_t *src=(vertex_t*)&curKeyFrame[j];
                vertex_t *nxt=(vertex_t*)&nextKeyFrame[j];

                for (uint32_t v=0; v<3; v++) {
                   dst->point[v]=(src->point[v]) + (((nxt->point[v]-src->point[v])*interpFctr)*percent);
                }
                j++;
            }
        }
}

void vectorNormalize(float * v) {
    double dist = v[X]*v[X] + v[Y]*v[Y] + v[Z]*v[Z];
    if (dist==0) return;
    double len = sqrt(dist);

    for (int i=0; i<3; i++){
        v[i] = v[i]/len;
    }
}

float lightDir[3] = {1, 1.5, -1};
extern float defLight[3];

void renderLightSource(animated_model_t * model)
{
    float position[3];
    float radius;
    if (!getLightSourcePosition(model, position, &radius)) return;

    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_STIPPLE);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glColor3f(1.0f, 0.85f, 0.15f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPushMatrix();
    glTranslatef(position[X], position[Y], position[Z]);
    GLUquadric * sphere = gluNewQuadric();
    if (sphere)
    {
        gluSphere(sphere, radius, 12, 8);
        gluDeleteQuadric(sphere);
    }
    glPopMatrix();
    glPopAttrib();
}

void renderShadow(model_t * m, const polygon_t * p) {
    if (lightDir[Y] <= 0.0001f) return;
    float groundY = m->BoxMax[1];

    glDepthMask(GL_FALSE);
    bool triangle = p->vertIdx[2] == p->vertIdx[3];
    glBegin(triangle ? GL_TRIANGLES : GL_QUADS);
    glColor4f(0,0,0,1);

    for (int32_t i=triangle ? 2 : 3; i>=0; --i) {
        glTexCoord2f(SaturnUV[i*2], (SaturnUV[(i*2) + 1 ]));
        vertex_t * v = &m->pntbl[p->vertIdx[i]];
        float t = (groundY - (float)v->point[1])/lightDir[1];
        float ptArray[3];
        ptArray[X] = v->point[0] + (lightDir[0]* t);
        ptArray[Y] = groundY;
        ptArray[Z] = v->point[2] + (lightDir[2] * t);
        glVertex3f(ptArray[0], ptArray[1], ptArray[2]);
    }
    glEnd();
    glDepthMask(GL_TRUE);
}


void render2D(model_t * m){
    return;
    glPushMatrix();
    glColor4f(0,0,0,1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    extern unsigned int texId[];
//glDepthMask(GL_FALSE);
    for (uint32_t i=0; i<m->nbPolygon; i++){
        polygon_t * pol = &m->pltbl[i];
        glBindTexture(GL_TEXTURE_2D, texId[pol->texture]);
        glBegin(GL_QUADS)  ;

        for (uint32_t ii=3; ii>=0; --ii){
            vertex_t * curVert = &m->pntbl[pol->vertIdx[ii]];
            glTexCoord2f(pol->vertUV[ii].U, pol->vertUV[ii].V);
            glVertex3f(curVert->point[0], curVert->point[2], 400.0f);
        }
        glEnd();
    }
    glPopMatrix();
//glDepthMask(GL_TRUE);
}

void renderAutomaticLod(animated_model_t * model)
{
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_POLYGON_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_STIPPLE);
    glDisable(GL_BLEND);
    for (unsigned int meshIndex = 0; meshIndex < model->nbModels; ++meshIndex)
    {
        unsigned int count = 0;
        const polygon_t * faces = automaticLodFaces(model, meshIndex, &count);
        model_t * mesh = &model->model[meshIndex];
        for (unsigned int faceIndex = 0; faceIndex < count; ++faceIndex)
        {
            const polygon_t & face = faces[faceIndex];
            float normal[3] = {};
            const float * first = mesh->pntbl[face.vertIdx[0]].point;
            const float * second = mesh->pntbl[face.vertIdx[1]].point;
            const float * third = mesh->pntbl[face.vertIdx[2]].point;
            float edge[3], next[3];
            for (unsigned int axis = 0; axis < 3; ++axis)
            {
                edge[axis] = second[axis] - first[axis];
                next[axis] = third[axis] - first[axis];
            }
            normal[X] = edge[Y] * next[Z] - edge[Z] * next[Y];
            normal[Y] = edge[Z] * next[X] - edge[X] * next[Z];
            normal[Z] = edge[X] * next[Y] - edge[Y] * next[X];
            vectorNormalize(normal);
            float intensity = 0.35f + 0.65f * std::fabs(normal[X] * lightDir[X] + normal[Y] * lightDir[Y] + normal[Z] * lightDir[Z]);
            glColor3f(intensity * face.color.r / 255.0f, intensity * face.color.g / 255.0f, intensity * face.color.b / 255.0f);
            bool triangle = face.vertIdx[2] == face.vertIdx[3];
            glBegin(triangle ? GL_TRIANGLES : GL_QUADS);
            for (int corner = triangle ? 2 : 3; corner >= 0; --corner)
                glVertex3fv(mesh->pntbl[face.vertIdx[corner]].point);
            glEnd();
            glColor3f(0.0f, 0.0f, 0.0f);
            renderShadow(mesh, &face);
        }
    }
    glPopAttrib();
}

int render(animated_model_t * aModel)
{
            float sourcePosition[3], markerRadius;
            getLightSourcePosition(aModel, sourcePosition, &markerRadius);

            vectorNormalize(&lightDir[0]);
            glPushMatrix();

            glRotatef(PLAYER[0].ROTATION[X], 1.0f, 0.0f, 0.0f);
            glRotatef(PLAYER[0].ROTATION[Y], 0.0f, 1.0f, 0.0f);
            glTranslatef(-PLAYER[0].POSITION[X], -PLAYER[0].POSITION[Y], -PLAYER[0].POSITION[Z]);
            glScalef(aModel->scale[X],aModel->scale[Y], aModel->scale[Z]);

            extern int DisplayTEST;
            if (!(DisplayTEST&1))glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            else  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        /*    if (DisplayTEST>1)
            else useUV=1;*/
//useUV=0;
uint32_t newUseUV=useUV;
#if 1
newUseUV=0;
#endif


            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
           // glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, 0x0105);
            //glBlendFunc(GL_FUNC_ADD, GL_ONE_MINUS_SRC_ALPHA);

            bool geometryChanged = false;
            if (aModel->nbFrames>0 && playAnimation)
            {

                uint16_t previousFrame = PLAYER[0].ANIMCTRL.currentFrm;
                ztUpdateAnimation(&PLAYER[0].ANIMCTRL, aModel);
                geometryChanged = PLAYER[0].ANIMCTRL.currentFrm != previousFrame;
                //testAnim+=ZT_FRAMERATE;
                //if (testAnim >= aModel->nbFrames*playSpeed) testAnim=0;
            }
            if (showAutomaticLod)
            {
                renderAutomaticLod(aModel);
                renderLightSource(aModel);
                glPopMatrix();
                return 1;
            }
            updateSelfShadowing(aModel, geometryChanged);
            updatePreviewColors(aModel);

            extern unsigned int texId[];
            for (unsigned int m=0; m<aModel->nbModels; m++)
            {
                model_t * curModel = &aModel->model[m];
                vertex_t * curVert;
                for (unsigned int i=0; i<curModel->nbPolygon; i++)
                {

                    polygon_t * pol = &curModel->pltbl[i];
                    bool useGouraud = activeShading && !(aModel->texture[pol->texture].attributes & TEX_NO_GOURAUD) &&
                                      (pol->SGL_ATTR.atrb & CL_Gouraud);
                    glBindTexture(GL_TEXTURE_2D, texId[pol->texture]);
                    if (aModel->texture[pol->texture].attributes & TEX_NO_GOURAUD) glColor4f(1,1,1,1);
                    if (pol->SGL_ATTR.flag & Dual_Plane) glDisable(GL_CULL_FACE);
                    else glEnable(GL_CULL_FACE);

                   // float transValue = 1.0f;
                    int dir = (pol->SGL_ATTR.dir>>4)&3;
                    if (pol->SGL_ATTR.atrb &MESHon){
                        glEnable(GL_POLYGON_STIPPLE);
                        glPolygonStipple(meshEffectArray);
                    } else {
                        glDisable(GL_POLYGON_STIPPLE);
                    }

                    glBegin(GL_QUADS);
                    {

                        for (int32_t ii=3; ii>=0; --ii) {
                            if (newUseUV)
                                glTexCoord2f(pol->vertUV[ii].U, pol->vertUV[ii].V);
                            else
                                glTexCoord2f(SaturnUV[ii*2+(dir*8)], (SaturnUV[(ii*2) + 1 + (dir*8)]));
                            curVert = &curModel->pntbl[pol->vertIdx[ii]];
                            float visibility = useGouraud ? previewVertexVisibility(m, pol->vertIdx[ii]) : 1.0f;
                            float shadow = visibility;

                            if (isSurfaceSelected(static_cast<int>(m), static_cast<int>(i)))
                                glColor4f(0.75f*shadow,0.25f*shadow,0.25f*shadow,0.66f);
                            else{
                                if (useGouraud) {
                                    const float * intensity = previewVertexColor(m, pol->vertIdx[ii]);
                                    glColor4f(intensity[0], intensity[1], intensity[2], 1.0f);
                                } else {
                                    glColor4f(1.0f,1.0f,1.0f,1.0f);
                                }

                            }
                            glVertex3f(curVert->point[X], curVert->point[Y], curVert->point[Z]);
                        }

                    }
                    glEnd();
                    renderShadow(curModel, pol);
                }
            }
render2D(&aModel->model[0]);
            renderLightSource(aModel);
            glPopMatrix();



            return 1;
}


const float offsetU = 16.0/256.0;
const float offsetV = 16.0/256.0;

float textUV[] = {
   /* 0, 1,
    1, 1,
    1, 0,*/


    0,       offsetV,
    offsetU, offsetV,
    offsetU, 0,
    0,       0,
};

const float textVertValue = 1/28.0f;
float textVert[] = {
    0, 0,
    textVertValue, 0,
    textVertValue, textVertValue,
    0, textVertValue,
};

float drawText(char * ptr, float x, float y){
    unsigned int t;
    float posX = -1 + textVertValue + x*textVertValue;
    float posY = 1 - textVertValue - y*textVertValue;
    while (*ptr)
    {
        t = (unsigned int)*ptr;
        if (t=='\n' || t=='\r\n') {
            ptr++;
            posX = -1 + textVertValue + x*textVertValue;
            posY -= textVertValue;
            continue;
        }
        glBegin(GL_QUADS);
        {
            for (int32_t ii=3; ii>=0; --ii) {
                glTexCoord2f(textUV[ii*2] + (t&15)*offsetU, textUV[ii*2+1] + t/16*offsetV);
                glVertex2f(textVert[ii*2]+posX, textVert[ii*2+1]+posY);
            }
        }
        glEnd();
        posX += textVertValue;
        ptr++;

        if (posX>=1) {
            posX = -1 + textVertValue + x*textVertValue;
            posY -= textVertValue;
        }
    }
    return posY;
}

string sortInfoArray[]={
    "SORT_BFR" ,			/* 直前に表示したポリゴンの位置を使う */
	"SORT_MIN" ,			/* ４点の内?A一番手前の点を使う */
	"SORT_MAX" ,			/* ４点の内一番遠い点を使う */
	"SORT_CEN"
};

string cmdCtrlInfoArray[]={
    "FUNC_Normal",              //0
    "FUNC_Scaled_sprite",       //1
    "FUNC_Distorted_sprite",    //2
    "INVALID...",               //3
    "FUNC_Polygon",             //4
    "FUNC_PolyLine",	        //5
    "FUNC_Line",        		//6
    "FUNC_SystemClip",      	//9
    "FUNC_UserClip", 	        //8
    "FUNC_BasePosition",    	//10
    "FUNC_End",
    "*",
    "*",
    "*",
    "*",
    "*",
    "NO_FLIP",
    "H_FLIP",
    "V_FLIP",
    "HV_FLIP",
};

string cmdtSpecialOptions[]={
    "",
    "UseTexture", //1
    "UseLight\n", //2
    "UseDepthShading\n",//3
    "UsePalette\n", //4
    "UseNearClip\n", //5
    "UseRealTimeGouraud\n", //6
};

string flagInfo[] = {
    "Backface culling ON",
    "Backface culling OFF",
};





void renderText(animated_model_t * model){
    extern unsigned int asciiTextureId;
    glColor4f(1,1,1,0.4);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluLookAt(0, 0, 0, 0, 0, -1, 0, 1, 0);

    glBindTexture(GL_TEXTURE_2D, asciiTextureId);
    glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_STIPPLE);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);




    /**
            sprintf(s, "%06d", i+1);
        string postVal(s);
        string animName = inFolder + fileIn + "_" + postVal + ".obj";
    **/
    drawText( "ZTP MODEL CONVERTER 0.5", 16, 0);
    string nbModel = "ACTIVE MODEL : " +std::to_string(activeModel);
    string nbFace = "ACTIVE FACE : " +std::to_string(activeFace);
    drawText( &nbModel[0], 0, 1); //, 4);
    drawText( &nbFace  [0], 0, 2); //, 4);
    drawText("RIGHT CLICK TO LOOK, LEFT CLICK TO SELECT POLYGON", 0, 3);

    const int infoPosY = 47;
    const int polInfoPos[2] = {0, 35};
    int yPos=polInfoPos[1];
    if (activeFace!=-1){
    polygon_t * pol = &model->model[activeModel].pltbl[activeFace];
    string textureIndex = "Texture ID : " + std::to_string(pol->SGL_ATTR.texno);
    drawText(&textureIndex[0], 0, 4);

        drawText("POLYGON INFORMATION", polInfoPos[0], polInfoPos[1] );
        string polCTRL = "Ctrl : " + /*std::to_string(pol->SGL_ATTR.dir & (3 << 4))*/
            cmdCtrlInfoArray[((pol->SGL_ATTR.dir& (3<<4))>>4)+16] + " " +
            cmdCtrlInfoArray[pol->SGL_ATTR.dir&7] + ", Code " + std::to_string(pol->SGL_ATTR.dir);
        drawText(&polCTRL[0], polInfoPos[0], polInfoPos[1]+1 );

        string polPMOD = "Pmod : " + std::to_string(pol->SGL_ATTR.atrb) + ", Gouraud shading : "+ ((pol->SGL_ATTR.atrb&CL_Gouraud)?"YES":"NO");
        drawText(&polPMOD[0], polInfoPos[0], polInfoPos[1]+3 );

        string polFLAG = "Flag : " + flagInfo[pol->SGL_ATTR.flag];
        drawText(&polFLAG[0], polInfoPos[0], polInfoPos[1]+5 );

        /**string polColor = "Color : " + std::to_string(pol->SGL_ATTR.colno);
        drawText(&polColor[0], polInfoPos[0], polInfoPos[1]+4 );*/

        /**
            "UseTexture",
    "UseLight\n",
    "UseDepthShading\n",
    "UsePalette\n",
    "UseNearClip\n",
    "UseRealTimeGouraud\n",
        **/
        int opt = (pol->SGL_ATTR.sorting)&(0xFC);
        string polSort = "Options : " + ((opt&UseLight)?cmdtSpecialOptions[2]:cmdtSpecialOptions[0]) + ((opt&UseGouraud)?cmdtSpecialOptions[6]:cmdtSpecialOptions[0]) +
        ((opt&UsePalette)?cmdtSpecialOptions[4]:cmdtSpecialOptions[0]) +
        ((opt&UseDepth)?cmdtSpecialOptions[3]:cmdtSpecialOptions[0]) +
        ((opt&UseNearClip)?cmdtSpecialOptions[5]:cmdtSpecialOptions[0]) + "\n" +
        sortInfoArray[pol->SGL_ATTR.sorting&3];
        drawText(&polSort[0], polInfoPos[0], polInfoPos[1]+7 );

        drawText("R=ROTATE POLYGON\nM=TOGGLE MESH TRANSPARENCY, H=TOGGLE GOURAUD\nF=FLIP TEXTURE READ DIRECTION\nC=TOGGLE BACKFACE CULLING\nN=CHANGE SORTING RULE", 0, infoPosY+4);
    }
        drawText("P=PLAY/PAUSE ANIMATION\nI=TOGGLE INTERPOLATION (preview only)\nL=TOGGLE LIGHT (preview only)", 0, infoPosY);



    glPopMatrix();

}
