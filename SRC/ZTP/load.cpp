#include "../COMMON.H"
#include <iomanip>
#include <vector>
extern int USE_SGL;
int generate_texture_list(string inString, animated_model_t * model)
{
    if (inString=="None") return -1;
    for (unsigned int i=0; i<model->nbTextures; i++)
    {
        if (inString == model->texture[i].name) return i;
    }
    model->texture[model->nbTextures].name = inString;
    model->texture[model->nbTextures].textureId = model->nbTextures;
    model->nbTextures++;
    return model->nbTextures-1;
}

/**Old implementation, weird code, it works, so who cares?**/
int sort_faces(string inString, model_t * t_PDATA, polygon_t * bufPol, int index, int total_verts)
{
            string f1, f2, f3;
            char bufferChar[128]={}; //Don't ask, I'm more used to C than C++, so just went "fuck it"....
            int i = 0;
            for (i=0; i<128; i++)
            {
                if (inString[i]=='/') {i++; break;}
                bufferChar[i]=inString[i];
            }
            f1 = bufferChar;
            int charPtr=0;            char bufferChar2[128]={};            f2 = "";
            for (i=i; i<128; i++)
            {
                if (inString[i]=='/') {i++; break;}
                bufferChar2[charPtr]= inString[i];
                charPtr++;
            }
            f2 = bufferChar2;
            charPtr=0; char bufferChar3[128]={}; f3 = "";
            for (i=i; i<128; i++)
            {
                if (inString[i]=='/') {i++; break;}
                bufferChar3[charPtr]=inString[i];
                charPtr++;
            }
            f3=bufferChar3;

  int i1, i2, i3;
  if (f1 != "")     i1 = std::stoi(f1) - 1;
  else (i1 = 0);
  if (f2 != "")     i2 = std::stoi(f2) - 1;
  else (i2 = 0);
  if (f3 != "")     i3 = std::stoi(f3) - 1;
  else (i3 = 0);


  bufPol->vertIdx[index]=i1-total_verts;
  bufPol->vertUV[index].U = texture_coord[i2].U;
  bufPol->vertUV[index].V = texture_coord[i2].V;

  return i2;
}


void rotatePolygon(model_t * m) {
if (useUV) return;
    polygon_t * pol = &m->pltbl[m->nbPolygon];

    int indexZero = 0;
    float minUV[2]={99999, 99999};
    for (int i=0; i<4; i++){
        if (pol->vertUV[i].U==0 && pol->vertUV[i].V==0) {
            indexZero=i;
            break;
        }
        if (pol->vertUV[i].U > minUV[0]) continue;
        if (pol->vertUV[i].V > minUV[1]) continue;
        minUV[0]=pol->vertUV[i].U;
        minUV[1]=pol->vertUV[i].V;
        indexZero=i;
    }

    unsigned int idxArray[4];
    uv_t         vertUV[4];

    for (int i=0; i<4; i++){
        idxArray[i]=pol->vertIdx[i];
        vertUV[i].U = pol->vertUV[i].U;
        vertUV[i].V = pol->vertUV[i].V;
    }
    for (int i=0; i<4; i++){
        int idx = (indexZero+i+1)&3;
        pol->vertIdx[i]=idxArray[idx];
        pol->vertUV[i].U=vertUV[idx].U;
        pol->vertUV[i].V=vertUV[idx].V;
    }
}

/****
*Main function to read the OBJ file
*Param : Obj file, upscale value (integer), your model data (_MOD_DATA) and boolean if it's a base model
*****/
bool load_OBJ_to_mesh(ifstream *file, float * scalefactor, animated_model_t * aModel, int keyFrameID)
{
    string line;
    tot_tex_coord=0;
    int pointer = 0;
    int textpointer = -1;
    int bufTotalVerts = 0;
    int MeshPtr = -1;

    if (!file->is_open()) return false;
    file->seekg(pointer, ios::beg);

    model_t * o_PDATA;

    while(!file->eof())
    {
        getline(*file, line);
        //cout << line << "\n";
        if (((line[0] == 'o' && line[1] == ' ') || (line[0]== 'g')) && (keyFrameID==-1))
        {
            if (MeshPtr >= 0) bufTotalVerts += aModel->model[MeshPtr].nbPoint;
            MeshPtr++;
            aModel->model[MeshPtr].nbPoint=0;
            aModel->model[MeshPtr].nbPolygon=0;
            aModel->model[MeshPtr].pntbl = new vertex_t[maxPolygons];
            aModel->model[MeshPtr].pltbl = new polygon_t[maxPolygons];

            o_PDATA = &aModel->model[MeshPtr];
            stringstream NameSS(line);            string buffer;            getline(NameSS, buffer, ' ');            NameSS >> buffer;
            cout << "\nMESH NAME : " << buffer;
            for (int i = 0; i<128; i++){
                aModel->model[MeshPtr].name[i] = buffer[i];
                if (!buffer[i]) break;
            }
            cout <<  "... Now loading polygons!\n";

        }
        else if (line[0] == 'v' && line[1] == ' ')
        {
            stringstream PointSS(line);
            string uselessbuffer;
            getline(PointSS, uselessbuffer, ' ');

            string sx = ""; string sy = ""; string sz = "";
            PointSS >> sx >> sy >> sz;

            float fx=std::atof(sx.c_str()); float fy=std::atof(sy.c_str()); float fz=std::atof(sz.c_str());

            if (keyFrameID==-1)
            {
                o_PDATA->pntbl[o_PDATA->nbPoint].point[X] = fx * scalefactor[X];
                o_PDATA->pntbl[o_PDATA->nbPoint].point[Y] = fy * scalefactor[Y];
                o_PDATA->pntbl[o_PDATA->nbPoint].point[Z] = fz * scalefactor[Z];
                o_PDATA->nbPoint++;
            }
            else
            {
                aModel->keyFrames[keyFrameID].cVert[bufTotalVerts].point[X] = fx * scalefactor[X];
                aModel->keyFrames[keyFrameID].cVert[bufTotalVerts].point[Y] = fy * scalefactor[Y];
                aModel->keyFrames[keyFrameID].cVert[bufTotalVerts].point[Z] = fz * scalefactor[Z];
                bufTotalVerts++;
            }

        }
        else if (line[0]== 'v' && line[1]== 't') //Texture coordinates...not implemented
        {
            stringstream TCoord(line);         string bufferStr;
            getline(TCoord, bufferStr, ' ');
            string tc0 = "", tc1 = "";
            TCoord >> tc0 >> tc1;
            texture_coord[tot_tex_coord].U = std::stof(tc0);
            texture_coord[tot_tex_coord].V = std::stof(tc1);
            tot_tex_coord++;
        }
        else if ((line[0]=='u' && line[1]=='s' && line[2]=='e' && line[3]=='m' && line[4]=='t' && line[5]=='l')&& (keyFrameID==-1)) //Yeah, I know we all got it after the 2 letters, but just in case some weird Obj format popup...
        {
            stringstream MtlSS(line);         string bufferStr;
            getline(MtlSS, bufferStr, ' ');
            string mtlName = {""};
            MtlSS >> mtlName;
            textpointer=generate_texture_list(mtlName, aModel);

        }
        else if ((line[0] == 'f' && line[1] == ' ')&& (keyFrameID==-1))
        {
            stringstream FaceSS(line);
            string bufferStr;
            getline(FaceSS, bufferStr, ' ');
            string p[4];
            FaceSS >> p[0] >> p[1] >> p[2] >> p[3];
            if (p[3]=="") p[3]=p[2];

            polygon_t bufPol;
            sort_faces(p[3], o_PDATA, &bufPol, 0, bufTotalVerts);
            sort_faces(p[2], o_PDATA, &bufPol, 1, bufTotalVerts);
            sort_faces(p[1], o_PDATA, &bufPol, 2, bufTotalVerts);
            sort_faces(p[0], o_PDATA, &bufPol, 3, bufTotalVerts);

            o_PDATA->pltbl[o_PDATA->nbPolygon]=bufPol;
            memcpy(&o_PDATA->pltbl[o_PDATA->nbPolygon], &bufPol, sizeof(polygon_t));
            o_PDATA->pltbl[o_PDATA->nbPolygon].texture = textpointer;

            rotatePolygon(o_PDATA);
            o_PDATA->nbPolygon++;
        }
        pointer++;
    }

    file->clear();

    if (keyFrameID==-1)
    {
        extern void recalculateNormals(model_t * pol);
        extern void pointNormals(model_t * pol);
        aModel->nbModels=MeshPtr + 1;
        for (unsigned i = 0; i<aModel->nbModels; i++)
        {
            recalculateNormals(&aModel->model[i]);
            pointNormals(&aModel->model[i]);
        }
    }

cout << "...done!\n";
    return true;
}


void specialConditions(unsigned short startPtr, unsigned short endPtr, animated_model_t * aModel)
{
    if (aModel->nbTextures <= 0) return;

    texture_t * t;
    for (unsigned short i=startPtr; i<endPtr; i++)
    {
        t = &aModel->texture[i];
        uint16_t colorDepth = CL16Look;
        if (t->colorDepth==COL_32K) colorDepth=CL32KRGB;

        std::size_t findDual = t->name.find("DUAL_");  //Legacy stuff...
        std::size_t findWhite = t->name.find("C_WHITE"); //Legacy stuff...

        if ((findDual==0) && (t->clut[1]==0 && t->clut[0]!=0) && colorDepth==CL16Look)
        {
            cout << "Found a dual plane attribute texture (no backface culling)\n";
            t->SGL_ATTR=ATTRIBUTE(Dual_Plane, SORT_MIN, No_Texture, t->clut[0], CL32KRGB|No_Gouraud, CL32KRGB| MESHoff | Window_In, sprPolygon, 0|UseNearClip);
        }
        else if ((findWhite == 0) && (t->clut[1]==0 && t->clut[0]!=0)&& colorDepth==CL16Look)
        {
            cout << "Found a quad with SORT_MIN flag.\n";
            t->SGL_ATTR=ATTRIBUTE(Single_Plane, SORT_MIN, No_Texture, t->clut[0], CL32KRGB|No_Gouraud, CL32KRGB| MESHoff | Window_In, sprPolygon, 0|UseNearClip);
        }
        else if (t->clut[0]==0)
        {
           t->SGL_ATTR = ATTRIBUTE(Dual_Plane,SORT_CEN, i, LUTidx(i), No_Gouraud,Window_In|MESHoff|HSSon|ECdis|SPenb|colorDepth,sprNoflip,0|UseNearClip);
        }
        else
        {
            if (t->clut[1]==0 && t->clut[0]!=0)
                t->SGL_ATTR=ATTRIBUTE(Single_Plane, SORT_CEN, No_Texture, t->clut[0], CL32KRGB|No_Gouraud, CL32KRGB| MESHoff | Window_In, sprPolygon, 0|UseNearClip);
            else
                t->SGL_ATTR=ATTRIBUTE(Single_Plane,SORT_CEN, i, LUTidx(i), No_Gouraud,Window_In|MESHoff|HSSon|ECdis|SPdis|colorDepth,sprNoflip,0|UseNearClip);
        }

        if ((t->clut[0] != 0) && (t->clut[1]!=0)) {
            uint32_t color[3] = {0,0,0};
            uint32_t cnt = t->width*t->height;
            for (uint32_t i=0; i<cnt; i++) {
                color[0]+=t->pixel[i].r;
                color[1]+=t->pixel[i].g;
                color[2]+=t->pixel[i].b;
            }
            if (cnt != 0 ){
                for (uint32_t i=0; i<3; i++) {
                    color[i] /= cnt;
                    color[i] = color[i]/8;
                }
            }
            t->SGL_ATTR.colno=C_RGB((uint16_t)color[0], (uint16_t)color[1], (uint16_t)color[2]);
            t->SGL_ATTR.atrb |= CL_Gouraud;
        }
    }

    ///???????? What's that crap??*/
    if (!useUV){
        for (unsigned int i=0; i<aModel->nbModels; i++){
                model_t * m = &aModel->model[i];

            for (unsigned int ii=0; ii<m->nbPolygon; ii++){
                polygon_t * p = &m->pltbl[ii];
                t = &aModel->texture[p->texture];
                //p->sorting = t->SGL_ATTR.sorting;
                p->SGL_ATTR = t->SGL_ATTR;
            }
        }
    }
}

#if 0
bool used_textures(animated_model_t * aModel, int textId)
{
    for (unsigned int mesh=0; mesh<aModel->nbModels; mesh++)
    {
        model_t * m = &aModel->model[mesh];
        for (unsigned int i=0; i< m->nbPolygon; i++)
        {
            if (LOD_mesh[mesh].attbl[i].texno == textID)
                return true;
        }
    }
    return false;
}

void delete_unused_texture(unsigned int textID)
{

    for (unsigned int mesh=0; mesh<MAP_DATA.TOTAL_MESH; mesh++)
    {
       // cout << "\n" << mesh << " ";
        for (unsigned int i=0; i< LOD_mesh[mesh].nbPolygon; i++)
        {
            //cout << i << ", ";
            if (LOD_mesh[mesh].attbl[i].texno > textID)
                LOD_mesh[mesh].attbl[i].texno--;
        }
        for (unsigned int i=0; i< LOD_mesh2[mesh].nbPolygon; i++)
        {
            //cout << i << ", ";
            if (LOD_mesh2[mesh].attbl[i].texno > textID)
                LOD_mesh2[mesh].attbl[i].texno--;
            LOD_mesh2[mesh].attbl[i].extraInfo=0;
        }
    }

//cout << "Deleting2 " << textID << ", " << tmpTotalLODtxt-1 << "\n";
    for (int i=textID; i<tmpTotalLODtxt-1; i++)
    {

        img[i].height=img[i+1].height;
        img[i].width=img[i+1].width;
        img[i].colorDepth=img[i+1].colorDepth;
        //cout << "W:" << img[i].width << ", H:" << img[i].height << ", " << TextureArray[i] << "\n";
        if (img[i].pixel) delete [] img[i].pixel;
        img[i].pixel = new pixel_t[MAX(img[i].width*img[i].height, 8)];
        for (unsigned int ii=0; ii< MAX(img[i].width*img[i].height, 8); ii++)
        {
            img[i].pixel[ii].color=img[i+1].pixel[ii].color;
        }
        TextureArray[i]=TextureArray[i+1];
        TextureSpecialAttribute[i]=TextureSpecialAttribute[i+1];
    }
 //   cout << "Deleting3\n";
    delete [] img[tmpTotalLODtxt].pixel;

    tmpTotalLODtxt--;
}
#endif // 0


void findScale(animated_model_t * aModel){
    for (int j=0; j<3; j++){
       aModel->scale[j]=(float)1.0f;
    }

    if (USE_SGL) return; //Not supported for now...

    double scale[3]={1.0f, 1.0f, 1.0f};
    double maxValue[3]={0,0,0};

    for (uint32_t i=0; i<aModel->nbModels; i++) {
        for (uint32_t ii=0; ii<aModel->model[i].nbPoint; ii++){
           for (uint32_t j=0; j<3; j++){
                maxValue[j]=MAX(maxValue[j], fabs(aModel->model[i].pntbl[ii].point[j]));
           }
        }
    }
    for (uint32_t i=0; i<aModel->nbFrames; i++){
        vertex_t * v = &aModel->keyFrames[i].cVert[0];
        for (uint32_t j=0; j<aModel->model[0].nbPoint; j++, v++){
            for (uint32_t jj=0; jj<3; jj++){
              maxValue[jj]=MAX(maxValue[jj], fabs(v->point[jj]));
            }
        }
    }

    for (int i=0; i<3; i++){
        scale[i]=1.0f;
        //maxValue[i] = MAX(fabs(aModel->BoxMax[i]),fabs(aModel->BoxMin[i]));
        if (maxValue[i]>=126.5f){
            scale[i]=maxValue[i]/126.5f;
        }
    }

    double invScale[3]={1.0f/scale[0], 1.0f/scale[1], 1.0f/scale[2]};

    for (int j=0; j<3; j++){
       aModel->scale[j]=(float)scale[j];
    }

    for (uint32_t i=0; i<aModel->nbModels; i++) {
        for (uint32_t ii=0; ii<aModel->model[i].nbPoint; ii++){
           for (uint32_t j=0; j<3; j++){
                aModel->model[i].pntbl[ii].point[j]*=invScale[j];
           }
        }
    }
    for (uint32_t i=0; i<aModel->nbFrames; i++){
        vertex_t * v = &aModel->keyFrames[i].cVert[0];

        for (uint32_t j=0; j<aModel->model[0].nbPoint; j++, v++){

            for (uint32_t jj=0; jj<3; jj++){
              v->point[jj]*=invScale[jj];
            }
        }
    }
}

string fileOutName;
void LoadOBJ(string inFolder, string fileIn, float * scalefactor, animated_model_t * aModel)
{
    cout << "\nNow loading : " << fileIn << "\n";
    string fileInName = inFolder + fileIn + ".obj";


    ifstream file(fileInName, ios::in | ios::ate);
    if (!file.is_open()) {
            cout << "ERROR READING OBJ FILE : " << fileInName << "\n";
            exit(-1);
    }

    aModel->nbTextures=0;
    aModel->texture = new texture_t[2048]();

   // aModel->model[0].pntbl = new vertex_t[256];
   // aModel->model[0].pltbl = new polygon_t[256];
    cout << "Preparing to open OBJ file...\n";
    if (load_OBJ_to_mesh(&file, scalefactor, aModel, -1)) {cout << "\nSuccess : " << fileIn << " loaded...\n";}
    else {cout << "\nERROR : Couldn't load Obj\n"; return;}
    file.close();

    extern int ReadTGAFile (string folder, texture_t * texture);
    if (useUV) {
        std::vector<texture_t> sourceTextures(aModel->texture, aModel->texture + aModel->nbTextures);
        for (unsigned int sourceIndex = 0; sourceIndex < sourceTextures.size(); ++sourceIndex)
            ReadTGAFile(inFolder, &sourceTextures[sourceIndex]);

        extern void createFakeTexture(texture_t *);
        bool needsFallback = false;
        for (unsigned int modelIndex = 0; modelIndex < aModel->nbModels; ++modelIndex)
            for (unsigned int faceIndex = 0; faceIndex < aModel->model[modelIndex].nbPolygon; ++faceIndex)
            {
                int materialIndex = aModel->model[modelIndex].pltbl[faceIndex].texture;
                if (materialIndex < 0 || materialIndex >= static_cast<int>(sourceTextures.size())) needsFallback = true;
            }
        const unsigned int fallbackIndex = static_cast<unsigned int>(sourceTextures.size());
        if (needsFallback)
        {
            sourceTextures.push_back(texture_t());
            sourceTextures.back().name = "None";
            createFakeTexture(&sourceTextures.back());
        }
        aModel->nbTextures=0;

        polygon_t * pol;
        for (uint32_t i=0; i<aModel->nbModels; i++)  {//Ici faut une boucle pour créer les nouvelles textures
            for (uint32_t ii=0; ii<aModel->model[i].nbPolygon; ii++){
                pol=&aModel->model[i].pltbl[ii];
                int materialIndex = pol->texture;
                const texture_t & sourceTexture = sourceTextures[
                    materialIndex >= 0 && materialIndex < static_cast<int>(fallbackIndex) ?
                    static_cast<unsigned int>(materialIndex) : fallbackIndex];
                createTexture(pol, aModel, sourceTexture);
            }
        }
        for (unsigned int sourceIndex = 0; sourceIndex < sourceTextures.size(); ++sourceIndex)
            delete [] sourceTextures[sourceIndex].pixel;
    }
    else {
        for (unsigned int i=0; i<aModel->nbTextures; i++)    {ReadTGAFile(inFolder, &aModel->texture[i]);}
    }



    if (USE_SGL==2) {
            cout << USE_SGL << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n0";
        aModel->texture[aModel->nbTextures].name = "PALETTE";
        ReadTGAFile(inFolder, &aModel->texture[aModel->nbTextures]);
        extern void writePalette(animated_model_t * a);
        writePalette(aModel);
    }


#if 0
float dist[4]={0,0,0,0};
float v[3];
uint32_t cnt=0;
vertex_t * v1, * v2;
model_t * m = &aModel->model[0];
for (uint32_t i=0; i<m->nbPolygon; i++){
    polygon_t * p = &m->pltbl[i];
    for (uint32_t ii=0; ii<4; ii++) {
        cnt=0;
        if (p->vertIdx[ii&3]==p->vertIdx[(ii+1)&3]) {
            dist[ii]=0;
            continue; //If the vertices are the same, skip
        }
        cnt++;
        v1=&m->pntbl[p->vertIdx[ii&3]];
        v2=&m->pntbl[p->vertIdx[(ii+1)&3]];
        for (uint32_t j=0; j<3; j++) {
            v[j]=v1->point[j]-v2->point[j]; }
        dist[ii]=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    }
    float newDist = 0;
    for (uint32_t j=0; j<4; j++)
        { newDist+=dist[j];}
    if (cnt!=0){
        newDist/= (float)cnt;
        if (newDist > 10.0* (float)scalefactor[X])
            p->isMask=1;
    }
}
#endif // 0
    //if (aModel->nbFrames==0) reOrgModel(model);
    uint32_t totVertices = 0;
    for (uint32_t i=0; i<aModel->nbModels; i++) {
        totVertices+=aModel->model[i].nbPoint;
    }


    for (uint32_t i=0; i<aModel->nbFrames; i++)
    {
        aModel->keyFrames[i].cVert = new vertex_t [totVertices];
        stringstream frameNumber;
        frameNumber << std::setw(6) << std::setfill('0') << i + 1;
        string postVal = frameNumber.str();
        string animName = inFolder + fileIn + "_" + postVal + ".obj";
        cout << animName << "\n\n****************\n";
        ifstream animFile(animName, ios::in | ios::ate);
        if (!animFile.is_open()) {
            cout << "ERROR READING OBJ FILE\n";
            delete [] aModel->keyFrames[i].cVert;
            aModel->keyFrames[i].cVert = NULL;
            aModel->nbFrames = i;
            break;
        }
        if (load_OBJ_to_mesh(&animFile, scalefactor, aModel, i)) {cout << "\nKeyFrame No. " << i << " loaded...\n";}
        else {
            cout << "\nERROR : Couldn't load keyframe\n";
            delete [] aModel->keyFrames[i].cVert;
            aModel->keyFrames[i].cVert = NULL;
            aModel->nbFrames = i;
            break;
        }
        animFile.close();
    }


    void calculateBoundingBox(float * boxMin, float * boxMax, vertex_t * vertexArray, unsigned int totalVertices);
    aModel->BoxMin[X] = aModel->BoxMin[Y] = aModel->BoxMin[Z] =  65536.0f;
    aModel->BoxMax[X] = aModel->BoxMax[Y] = aModel->BoxMax[Z] = -65536.0f;
    for (uint32_t i = 0; i < aModel->nbModels; i++)
    {
        calculateBoundingBox(&aModel->BoxMin[X], &aModel->BoxMax[X], &aModel->model[i].pntbl[0], aModel->model[i].nbPoint); //Updates the whole thing
        aModel->model[i].BoxMax[X]=aModel->model[i].BoxMax[Y]=aModel->model[i].BoxMax[Z]=-65536.0f;
        aModel->model[i].BoxMin[X]=aModel->model[i].BoxMin[Y]=aModel->model[i].BoxMin[Z]= 65536.0f;
        calculateBoundingBox(&aModel->model[i].BoxMin[X], &aModel->model[i].BoxMax[X], &aModel->model[i].pntbl[0], aModel->model[i].nbPoint); //Calculates the sub-model's bounding box in case you want to add per limb collision, headshots, etc.
    }
    if (!useUV)    specialConditions(0, aModel->nbTextures, aModel);
    for (unsigned int modelIndex = 0; modelIndex < aModel->nbModels; ++modelIndex)
    {
        for (unsigned int polygonIndex = 0; polygonIndex < aModel->model[modelIndex].nbPolygon; ++polygonIndex)
            aModel->model[modelIndex].pltbl[polygonIndex].SGL_ATTR.atrb |= CL_Gouraud;
    }

    cout << "Calculating lighting...\n";
    extern void calcLight(animated_model_t*);
    calcLight(aModel);





    findScale(aModel);


#if 0
string fileOutName = "OUT/" + fileIn;
    cout << "NOW CREATING BINARY FILE " << fileOutName << ".ZTP\n";
    string binaryFileName = fileOutName + ".ZTP";  //Z-Treme character

    ofstream binFile(binaryFileName.c_str(), ios::out | ios::binary);
    if (!binFile.is_open()) {cout << "\nERROR WHILE WRITING BINARY FILE!\n"; return;}
    write_model_binary(&binFile, aModel);

    cout << "Writing debug text file...\n";
    extern bool write_MOD_HEADER(string filename, animated_model_t * aModel);
    write_MOD_HEADER("OUT.TXT", aModel);
    binFile.close();
#else
fileOutName = /*"OUT/" +*/ fileIn;
#endif // 0    void write_model_binary(ofstream * file, animated_model_t * aModel);



}




animated_model_t * loadingConfigured(unsigned int frameCount, int useSgl, float scale,
                                     uint32_t useUv, uint32_t specialOptions,
                                     string path, string fileName)
{
    USE_SGL = useSgl;
    useUV = useUv >= 1 ? 1 : 0;
    specialOption = specialOptions;

    float scaleFactor[XYZ] = {scale, scale, scale};
    animated_model_t * newModel = new animated_model_t;
    ZeroMemory(newModel, sizeof(animated_model_t));
    newModel->nbFrames = frameCount;
    newModel->framerate = 0;
    newModel->nbModels = 0;
    newModel->model = new model_t[256];
    ZeroMemory(newModel->model, sizeof(model_t) * 256);
    newModel->keyFrames = new key_frame_t[newModel->nbFrames];
    ZeroMemory(newModel->keyFrames, sizeof(key_frame_t) * newModel->nbFrames);
    LoadOBJ(path, fileName, scaleFactor, newModel);
    return newModel;
}

animated_model_t * loading(string line)
{

    stringstream classType(line);            string bufferStr;
    getline(classType, bufferStr, ' ');
    string c[32]; //Stupidly high for no real reasons, could be smarter
    c[0]=bufferStr;
    int tot=1;
    while (classType>>c[tot]){
        tot++;
    }

    string path;
    string fileName;
    float scale;
    unsigned int frameCount;

    cout << "SEGA SATURN Z-TREME ENGINE MODEL CONVERTER 0.45\n";
    cout << "BY XL2 (2022-02-02)\n\n";
    cout << "New since v.0.4 : Supports texture coordinates!\nSupports a maximum of 256 vertices/faces per model for the non-SGL version\n";

    cout << c[0] << "\n";
    if (c[0] != "") {
        frameCount = static_cast<unsigned int>(std::atoi(c[0].c_str()));
    } else {
        cout << "\n***STEP 1***";
        cout << "\n   Enter how many keyframes your model has\n";
        cout <<"   Example : for 6 keyframes to interpolate your animations, enter 6.\n   0 for no animation.\n\n";
        cout << "ENTER TOTAL KEYFRAMES : ";
        cin >> frameCount;
    }


    if (c[1]!="") {
        USE_SGL = std::atoi(c[1].c_str());
    } else {
        cout << "\n***STEP 2***\n   Will you be using SGL? '0' for NO, '1' for YES. Default : '1'\nUSE SGL : ";
        cin >> USE_SGL;
    }
    if (USE_SGL != 0 && USE_SGL != 2) USE_SGL=1;

    if (c[2]!="") {
        scale = std::atof(c[2].c_str());
    } else {
        cout << "\n***STEP 3***\n   You will now have to enter the scale value and press enter. \n   Example : 64, 128, 100 or any integer value.\n\nENTER SCALE : ";
        cin >> scale;
    }
    if (scale==0.0f) scale=1.0f;
    if (c[3]!=""){
        useUV=std::atoi(c[3].c_str());
    } else {
        cout << "\n***STEP 4***\n   Do you want to use UV coordinates to generate Saturn textures for each material?\n   Example : 0 for no, 1 for yes.\n\nUSE UV coordinates : ";
        cin >> useUV;
    }
    if (useUV>=1) useUV=1;

    if (c[4]!=""){
        specialOption=std::atoi(c[4].c_str());
    } else {
        cout << "\n\n***STEP 5***\n   Put your option flags (bits). Bit 0 = Force 4 BPP. Bit 1 = Force power of 8 height textures for UV. (Default = 0)\n\nSPECIAL OPTION : ";
        cin >> specialOption;
    }

    if (c[5]!="") {
        path = c[5];
    } else {
        cout << "\n***STEP 6***\n   You will now have to enter the folder's relative path\n   IMPORTANT : Don't forget to end it with a '/'.\n   Example : IN/TITLE/  and then press enter.\n\nENTER THE PATH : ";
        cin >> path;
    }


    if (c[6]!=""){
        fileName = c[6];
    } else {
        cout << "\n\n***STEP 7***\n\n   You will now have to enter the OBJ filename WITHOUT the extension.\n   Example : TITLE and then press enter.\n\nENTER THE NAME : ";
        cin >> fileName;
    }


    if (c[7]!=""){
        extern BOOL bQuit;
        bQuit = std::atoi(c[7].c_str())&1;
    }


    return loadingConfigured(frameCount, USE_SGL, scale, useUV, specialOption, path, fileName);
    //delete [] newModel.animation;


   /** uint32_t cnt=0;
    for (uint32_t i = 0; i<newModel->model[0].nbPolygon; i++){
        if (newModel->model[0].pltbl[i].isMask==0) cnt++;
    }
cout << "DEBUG....\n"; cout << cnt << " out of " << newModel->model[0].nbPolygon << " are masks.\n";*/
}
