#include "../COMMON.H"


#define test_convert(x) (x)//(x/65536)

#if 0
{
                            extern float SaturnUV[];
                            model_t * curModel = &aModel->model[m];
                            vertex_t * curVert;
                            file << "o " << pcx.name << "." << yVal++ << "\n";
                            for (unsigned int i=0; i<curModel->nbPoint; i++){
                                file << "v ";
                                file << curModel->pntbl[i].point[0]<< " ";
                                file << curModel->pntbl[i].point[1] << " ";
                                file << curModel->pntbl[i].point[2] << " ";
                                file << "\n";
                            }

                            for (unsigned int i=0; i<curModel->nbPolygon; i++){
                                int dir = (curModel->pltbl[i].SGL_ATTR.dir>>4)&3;
                                for (int ii=3; ii>=0; ii--){
                                    file << "vt " << SaturnUV[ii*2+(dir*8)] << " " << (SaturnUV[(ii*2) + 1 + (dir*8)]) << "\n";
                                }
                            }
                            int byteCode=0;
                            for (unsigned int i=0; i<curModel->nbPolygon; i++)
                            {
                                int j=polArray[i];
                                //if (aModel->texture[curModel->pltbl[j].texture].colorDepth==COL_16) {continue;} //temp

                                float transValue = 1.0f;
                                int dir = (curModel->pltbl[j].SGL_ATTR.dir>>4)&3;
                                //if (curModel->pltbl[i].atrb&MESHon) transValue=0.5f;


                                    file << "usemtl " << aModel->texture[curModel->pltbl[j].texture].name << "\n";
                                    file << "f ";

                                    for (int32_t ii=3; ii>=0; --ii) {
                                        file << curModel->pltbl[i].vertIdx[ii] + offsetV << "/" << ii+offsetVt /*(SaturnUV[ii*2+(dir*8)], (SaturnUV[(ii*2) + 1 + (dir*8)]))*/ << "/1 ";
                                    }
                                    file << "\n";


                            }
                            offsetV+=curModel->nbPoint;
                            offsetVt+=curModel->nbPolygon*4;
                        }

#endif // 0
extern string PdataName[];

bool write_MOD_HEADER(string filename, animated_model_t * aModel)
{
    ofstream file(filename, ios::out);
    if (!file.is_open()) {cout << "ERROR...\n"; return false;}

    file << "#ifndef LUTaddr\n";
    file << "#define     LUTaddr             (0x25C7A960)\n";
    file << "#define     returnLUTaddr(n)    (LUTaddr + (0x20 * n))\n";
    file << "#define     LUTidx(n)           (Uint16)(62764 + (4 * n))\n";
    file << "#define     CRAM_Base           (0x25f00200)\n";
    file << "#define     returnCRAMaddr(n)   (CRAM_Base + (0x20 * n))\n";
    file << "#define     LUTcramIdx(n)       ((256 + (16 * n)))\n";
    file << "#endif\n\n";

    for (unsigned int mesh=0; mesh < aModel->nbModels; mesh++)
    {
        model_t * m = &aModel->model[mesh];
        if (m->nbPolygon > 0)
        {
            file << "//MESH NO." << mesh << " : " << PdataName[mesh] << "\n";

            file << "POINT point_" << mesh << "[] = {\n";
            for (unsigned int i=0; i<m->nbPoint; i++)
            {
                file << "       POStoFIXED(" << (test_convert(m->pntbl[i].point[X])) << ", "
                << (test_convert(m->pntbl[i].point[Y])) << ", "
                << (test_convert(m->pntbl[i].point[Z])) << "),\n";
            }
            file << "};\n\n";

            file << "POLYGON polygon_" << mesh << "[] = {\n";
            for (unsigned int i=0; i<m->nbPolygon; i++)
            {
                file << "       NORMAL(";
                for (unsigned int j=0; j<3; j++)
                {file << (m->pltbl[i].normal[j]); if (j<2) file << ", ";}
                file << "), VERTICES(";
                for (unsigned int j=0; j<4; j++)
                {file << m->pltbl[i].vertIdx[j]; if (j<3) file << ", ";}
                file << "),\n";
            }
            file << "};\n\n";
#if 0
            file->write((char*)&aModel->texture[aModel->model[i].pltbl[ii].texture].SGL_ATTR.flag, sizeof(uint8_t));
            file->write((char*)&aModel->texture[aModel->model[i].pltbl[ii].texture].SGL_ATTR.sorting, sizeof(uint8_t));
            writeUint16(file, aModel->texture[aModel->model[i].pltbl[ii].texture].SGL_ATTR.texno);
            writeUint16(file, aModel->texture[aModel->model[i].pltbl[ii].texture].SGL_ATTR.atrb);
            writeUint16(file, aModel->texture[aModel->model[i].pltbl[ii].texture].SGL_ATTR.colno);
            writeUint16(file, aModel->texture[aModel->model[i].pltbl[ii].texture].SGL_ATTR.gstb);
            writeUint16(file, aModel->texture[aModel->model[i].pltbl[ii].texture].SGL_ATTR.dir);
#endif // 0
            file << "ATTR attribute_" << mesh << "[] = {\n";
            for (unsigned int i=0; i<m->nbPolygon; i++)
            {
                file << "       ATTRIBUTE(";
                file << "Single_Plane, SORT_CEN, ";
                file << m->pltbl[i].texture << ", LUTidx(" << m->pltbl[i].texture << "),";
                file << " No_Gouraud,Window_In|MESHoff|HSSon|ECdis|SPdis|CL_Gouraud|CL16Look,sprNoflip, UseNearClip),\n";
            }
            file << "};\n\n";

            file << "VECTOR normal_" << mesh << "[] = {\n";
            for (unsigned int i=0; i<m->nbPoint; i++)
            {
                file << "       POStoFIXED(" << (test_convert(m->pntbl[i].normal[X]))
                << ", " << (test_convert(m->pntbl[i].normal[Y])) << ", "
                << (test_convert(m->pntbl[i].normal[Z])) << "),\n";
            }
            file << "};\n\n";

            file << "XPDATA xpdata_" << mesh << "[] = {\n   {\n";
            file << "       point_" << mesh << ", sizeof(point_" << mesh << ")/sizeof(POINT),\n";
            file << "       polygon_" << mesh << ", sizeof(polygon_" << mesh << ")/sizeof(POLYGON),\n";
            file << "       attribute_" << mesh << ",\n       normal_" << mesh << "\n  }\n";
            file << "};\n\n";


        }
    }
#if 0
    unsigned int cntPnt = 0;
    unsigned int cntPlt = 0;

    for (unsigned int mesh=0; mesh<model->TOTAL_MESH; mesh++)
    {
        cntPnt += model->pol[mesh].nbPoint;
        cntPlt += model->pol[mesh].nbPolygon;
    }
    file << "//TOTAL POINTS : " << cntPnt << "\n";
    file << "//TOTAL POLYGONS : " << cntPlt << "\n";


    file << "#ifndef _ANORM_h\n#define _ANORM_h\n";
    file << "\nVECTOR ANORMS[]={\n";
    for (int i=0; i<model->pol[0].nbPoint; i++)
    {
        float buf[3];
        buf[X]=toFLOAT(model->pol[0].pntbl[i][X]);
        buf[Y]=toFLOAT(model->pol[0].pntbl[i][Y]);
        buf[Z]=toFLOAT(model->pol[0].pntbl[i][Z]);
        float dist=sqrtf((buf[X]*buf[X])+(buf[Y]*buf[Y])+(buf[Z]*buf[Z]));
        if (dist != 0)
        {
            buf[X]= (buf[X]/dist);
            buf[Y]= (buf[Y]/dist);
            buf[Z]= (buf[Z]/dist);
        }
        file << "POStoFIXED(" << buf[X] << ", " << buf[Y] << ", " << buf[Z] << "),\n";
    }
    file << "};";
#endif // 0
    file.close();
    return true;
}


#if 0
int writeShadows(string filename, animated_model_t * aModel)
{
  //  return 0;
    #if 0
    #else
        extern player_t PLAYER[];
        extern void resetPlayer(player_t*player);
        resetPlayer(&PLAYER[0]);
        PLAYER[0].POSITION[X]=0;
        PLAYER[0].POSITION[Z]=0;
        PLAYER[0].POSITION[Y]= aModel->BoxMin[Y]-128; // -128.0f ; //-256.0f;
        PLAYER[0].ROTATION[X]=90;
        PLAYER[0].ROTATION[Y]=0;
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_CULL_FACE);
        glColor4f(1.0f,1.0f,1.0f,1.0f);

        glPushMatrix();

        #if 0
        glMatrixMode(GL_MODELVIEW);
        gluLookAt(0.0, 0.0, -0.1, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0);
        #else
        glMatrixMode(GL_PROJECTION);
glLoadIdentity();

float minX = aModel->BoxMin[X];
float maxX = aModel->BoxMax[X];
float minZ = aModel->BoxMin[Z];
float maxZ = aModel->BoxMax[Z];

// marge pour éviter le clipping
float margin = 1.0f;

glOrtho(minX - margin, maxX + margin,
        minZ - margin, maxZ + margin,
        -1000.0f, 1000.0f);

glMatrixMode(GL_MODELVIEW);
glLoadIdentity();
gluLookAt(0.0, 1000.0, 0.0,   // caméra très haut
          0.0, 0.0, 0.0,      // regarde le centre
          0.0, 0.0, -1.0);    // orientation

        #endif // 0

            glRotatef(PLAYER[0].ROTATION[X], 1.0f, 0.0f, 0.0f);
            glRotatef(PLAYER[0].ROTATION[Y], 0.0f, 1.0f, 0.0f);
            glTranslatef(-PLAYER[0].POSITION[X], -PLAYER[0].POSITION[Y], -PLAYER[0].POSITION[Z]);
            glScalef(1.0f,0.0f, 1.0f);


            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_POLYGON_STIPPLE);
            glDisable(GL_DEPTH_TEST);
           // glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, 0x0105);
            //glBlendFunc(GL_FUNC_ADD, GL_ONE_MINUS_SRC_ALPHA);

            if (aModel->nbFrames>0){
                extern ztUpdateAnimation(animationControl_t * animCtrl, animated_model_t * currentModel);
                PLAYER[0].ANIMCTRL.currentFrm=0;
                extern int ZT_FRAMERATE;
                int tmpFramerate = ZT_FRAMERATE;
                ZT_FRAMERATE=0;
                ztUpdateAnimation(&PLAYER[0].ANIMCTRL, aModel);
                ZT_FRAMERATE=tmpFramerate;
            }

            const float cOffset = 0.5;
            extern unsigned int texId[];
            for (unsigned int m=0; m<aModel->nbModels; m++)
            {
                model_t * curModel = &aModel->model[m];
                vertex_t * curVert;
                for (unsigned int i=0; i<curModel->nbPolygon; i++)
                {

                    polygon_t * pol = &curModel->pltbl[i];
                    glBindTexture(GL_TEXTURE_2D, texId[pol->texture]);
                    if (aModel->texture[pol->texture].attributes & TEX_NO_GOURAUD) glColor4f(1,1,1,1);
                    int dir = (pol->SGL_ATTR.dir>>4)&3;



                    int newUseUV=0;
                    extern float SaturnUV[];

                    glBegin(GL_QUADS);
                    {

                        for (int32_t ii=3; ii>=0; --ii) {
                            if (newUseUV)
                                glTexCoord2f(pol->vertUV[ii].U, pol->vertUV[ii].V);
                            else
                                glTexCoord2f(SaturnUV[ii*2+(dir*8)], (SaturnUV[(ii*2) + 1 + (dir*8)]));
                            curVert = &curModel->pntbl[pol->vertIdx[ii]];
                            {
                                {
                                    glColor4f(1,1,1,1.0f);
                                }
                            }
                            glVertex3f(curVert->point[X]/aModel->scale[X], curVert->point[Y]/aModel->scale[Y], curVert->point[Z]/aModel->scale[Z]);
                        }

                    }
                    glEnd();
                }
            }

            glPopMatrix();
glPopMatrix();
//glReadBuffer(GL_DEPTH);
//unsigned int data;
unsigned char data[4];
                    std::vector<unsigned char> png;

                    #define screen_width (1024)
                    #define screen_height (640)
                    #define width_offset (192)
                    #define width_jump (5)
                    #define height_jump (5)
                    for (int h=1; h<=screen_height; h+=height_jump){
                        for (int w=width_offset; w<screen_width-width_offset; w+=width_jump){


                            glReadPixels(w, screen_height-h, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);


                            if ((int)data[0]!=0xff && (int)data[1]!=0xff & (int)data[2]!=0xff){
                                data[0]=data[1]=data[2]=0;
                                data[3]=0xff;
                            } else {
                                data[3]=0;
                            }

                           // png.push_back(data);

                            for (int i=0; i<4; i++){
                                png.push_back(data[i]); //Si c'est blanc et alpha 0, c'est transparent
                            }
                        }

                    }

cout << (int)data[0] << ", " << (int)data[1] << ", " << (int)data[2] << " = color value\n";


                unsigned error = lodepng::encode("SHADOW.PNG" , png, (screen_width-width_offset*2)/width_jump, screen_height/height_jump);
                cout << error << " = error code\n";
                cout << aModel->scale[X] << ", " << aModel->scale[Y] << ", " << aModel->scale[Z] << " == model scale.\n";

glEnable(GL_DEPTH_TEST);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    #endif
}
#else

void resetAnimation(animated_model_t * aModel) {
    extern int ZT_FRAMERATE;
    extern player_t PLAYER[];
    extern int ztUpdateAnimation(animationControl_t * animCtrl, animated_model_t * currentModel);

    if (aModel->nbFrames>0){
        PLAYER[0].ANIMCTRL.currentFrm=0;
        int tmpFramerate = ZT_FRAMERATE;
        ZT_FRAMERATE=0;
        ztUpdateAnimation(&PLAYER[0].ANIMCTRL, aModel);
        ZT_FRAMERATE=tmpFramerate;
    }
}

struct Vec3 {
    float x, y, z;
};



int writeShadows(std::string filename, animated_model_t* aModel)
{
    cout << "Bounding box : \n";
    for (int i=0; i<3; i++) {cout<<aModel->BoxMin[i] << ", ";}
    for (int i=0; i<3; i++) {cout<<aModel->BoxMax[i] << ", ";}
    cout << "\n";
    resetAnimation(aModel);

    // --- Bounding box recalculée ---
    Vec3 min = {  1e9f,  1e9f,  1e9f };
    Vec3 max = { -1e9f, -1e9f, -1e9f };

    for (unsigned int m = 0; m < aModel->nbModels; m++) {
        const model_t* curModel = &aModel->model[m];
        for (unsigned int i = 0; i < curModel->nbPoint; i++) {
            const vertex_t* v = &curModel->pntbl[i];
            float x = v->point[X] / aModel->scale[X];
            float y = v->point[Y] / aModel->scale[Y];
            float z = v->point[Z] / aModel->scale[Z];
            min.x = MIN(min.x, x); 
            min.y = MIN(min.y, y); 
            min.z = MIN(min.z, z);
            max.x = MAX(max.x, x); 
            max.y = MAX(max.y, y); 
            max.z = MAX(max.z, z);
        }
    }

    float spanX = max.x - min.x; if (spanX <= 0) spanX = 1;
    float spanZ = max.z - min.z; if (spanZ <= 0) spanZ = 1;

    // --- Calcul dimensions finales ---
    float ratio = spanZ / spanX;

    // Largeur brute proportionnelle
    int width = (int)std::ceil(spanX / 4.0f);
    // Arrondir au multiple de 8
    width = (width + 7) & ~7;

    // Limiter largeur à 64 si modèle >= [-128,127]
    if (spanX >= 255 || spanZ >= 255) {
        width = std::min(width, 64);
    }

    // Hauteur proportionnelle au ratio
    int height = (int)std::ceil(width * ratio);

    // --- Contraintes minimales ---
    if (width  < 16) width  = 16;
    if (height < 16) height = 16;

    // --- Supersampling : rendu plus grand puis réduction ---
    int render_width  = width * 4;
    int render_height = height * 4;

    glViewport(0, 0, render_width, render_height);

    float halfX = spanX * 0.5f;
    float halfZ = spanZ * 0.5f;
    float marginFactor = 1.1f;
    halfX *= marginFactor;
    halfZ *= marginFactor;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-halfX, halfX, -halfZ, halfZ, -1000.0f, 1000.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0.0, 500.0, 0.0,
              0.0, 0.0, 0.0,
              0.0, 0.0, -1.0);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // --- Rendu des polygones (inchangé) ---
    extern unsigned int texId[];
    extern float SaturnUV[];
    for (unsigned int m = 0; m < aModel->nbModels; m++) {
        model_t* curModel = &aModel->model[m];
        for (unsigned int i = 0; i < curModel->nbPolygon; i++) {
            polygon_t* pol = &curModel->pltbl[i];
            glBindTexture(GL_TEXTURE_2D, texId[pol->texture]);
            if (aModel->texture[pol->texture].attributes & TEX_NO_GOURAUD)
                glColor4f(1, 1, 1, 1);
            int dir = (pol->SGL_ATTR.dir >> 4) & 3;
            glBegin(GL_QUADS);
            for (int32_t ii = 3; ii >= 0; --ii) {
                glTexCoord2f(SaturnUV[ii*2+(dir*8)], SaturnUV[(ii*2)+1+(dir*8)]);
                vertex_t* curVert = &curModel->pntbl[pol->vertIdx[ii]];
                glColor4f(1,1,1,1.0f);
                glVertex3f(curVert->point[X]/aModel->scale[X],
                           curVert->point[Y]/aModel->scale[Y],
                           curVert->point[Z]/aModel->scale[Z]);
            }
            glEnd();
        }
    }

    // --- Lecture haute résolution ---
    std::vector<unsigned char> hiRes(render_width * render_height * 4);
    glReadPixels(0, 0, render_width, render_height, GL_RGBA, GL_UNSIGNED_BYTE, hiRes.data());

    // --- Downscale avec flip vertical + filtrage ombre ---
    std::vector<unsigned char> png(width * height * 4);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int srcX = x * render_width / width;
            int srcY = (render_height - 1) - (y * render_height / height);
            int srcIndex = (srcY * render_width + srcX) * 4;
            int dstIndex = (y * width + x) * 4;

            unsigned char r = hiRes[srcIndex + 0];
            unsigned char g = hiRes[srcIndex + 1];
            unsigned char b = hiRes[srcIndex + 2];

            if (!(r == 0xFF && g == 0xFF && b == 0xFF)) {
                png[dstIndex + 0] = 0;
                png[dstIndex + 1] = 0;
                png[dstIndex + 2] = 0;
                png[dstIndex + 3] = 0xFF;
            } else {
                png[dstIndex + 0] = 0;
                png[dstIndex + 1] = 0;
                png[dstIndex + 2] = 0;
                png[dstIndex + 3] = 0;
            }
        }
    }

    unsigned error = lodepng::encode("SHADOW.PNG", png, width, height);
    std::cout << error << " = error code\n";

    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    return 0;
}
#endif
