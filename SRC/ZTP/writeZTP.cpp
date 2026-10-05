#include "../COMMON.H"

int USE_SGL = 1; //For now, default option?


void writeUint8(ofstream * file, uint8_t data);
void writeSint8(ofstream * file, int8_t data);
void writeUint16(ofstream * file, uint16_t data);
void writeSint16(ofstream * file, int16_t data);
void writeUint32(ofstream * file, uint32_t data);
void writeSint32(ofstream * file, int32_t data);
extern void recalculateNormals(model_t *);
extern void calcLight(animated_model_t*);
extern void pointNormals(model_t *);
#define roundCVert(v) (int8_t)(v<0?v-0.5:v+0.5)
#define Max_Normals (162)
VECTOR normLUT[] =
{
    #include "anorms.h"
};

uint8_t normalLookUp(float * curNormal)
{
    uint8_t  bestRes = 0;
    float   bestDist = 65535.0;

    for (uint8_t i=0; i<Max_Normals; i++)
    {
        float dist = sqrtf(pow(curNormal[X]-normLUT[i][X], 2) + pow(curNormal[Y]-normLUT[i][Y], 2) + pow(curNormal[Z]-normLUT[i][Z], 2));
        if ((dist) < (bestDist))
        {
            bestDist=dist;
            bestRes=i;
        }
    }
    return bestRes;
}


void WRITE_MDATA(ofstream * file, animated_model_t * aModel)
{
    writeUint16(file, aModel->nbModels);
    writeUint16(file, aModel->nbTextures);

    uint32_t countSize = 0;
    for (uint32_t i = 0; i<aModel->nbTextures; i++) {
        if (aModel->texture[i].colorDepth != COL_32K){
            countSize += (aModel->texture[i].height*aModel->texture[i].width)/2; //>>aModel->texture[i].colorDepth;
            countSize += 12+32;//*aModel->texture[i].colorDepth; //If colorDepth = 0, then there is no color look-up table.
        } else {
            countSize += (aModel->texture[i].height*aModel->texture[i].width)*2 + 12+32; //Because of stupid decisions taken a long while ago I still need to waste some VRAM holding ghost palettes...
        }

    }
    writeUint32(file, countSize);

    countSize=0;
    for (uint32_t i = 0; i<aModel->nbModels; i++) {
        if (USE_SGL)
        {
            countSize += aModel->model[i].nbPolygon * 20 ; //Each polygon = 12 bytes
            countSize += aModel->model[i].nbPoint * 32;
        }
        else
            countSize += aModel->model[i].nbPoint;

    }
    if (USE_SGL)
    {
        countSize += (aModel->nbFrames * aModel->model[0].nbPoint * 8); //Each compressed vertex is a sint16_t[3]
        writeUint32(file, countSize);
    }
    else
        writeUint16(file, countSize);

    countSize=0;
    if (!USE_SGL)
    {
        for (uint32_t i=0; i<aModel->nbModels; i++)
        {
            countSize+=aModel->model[i].nbPolygon;
        }
        writeUint16(file, countSize);
    }

    for (uint32_t i=0; i<3; i++) {
            writeSint32(file, toFIXED(aModel->BoxMin[i]));
            cout << (int)(aModel->BoxMin[i]) << ", ";
    }
    for (uint32_t i=0; i<3; i++) {
            writeSint32(file, toFIXED(aModel->BoxMax[i]));
            cout << (int)(aModel->BoxMax[i]) << ", ";
    }

    if (!USE_SGL){
        for (uint32_t i=0; i<3; i++){
            writeSint32(file, toFIXED(aModel->scale[i]));
        }
       // writeSint32(file, 0); //Dummy stuff for future options perhaps?
    }
    cout << "\n";

    //Animation : Updated 2018/05/31
    writeUint16(file, aModel->nbFrames);
    writeUint16(file, aModel->framerate);

    writeUint32(file, 0x00000000); //Space for a pointer

    if (!USE_SGL) {
        for (uint32_t i = 0; i<9; i++) writeUint32(file, 0);
    }

}

/**
New Z-Treme engine format (no more SGL)

typedef int16_t ZPOINT[4]; //vertex data = 3 Points, 1 color data. Use int8_t instead?

typedef struct {
    Uint8   vertices[4];
    Uint16  texno;
    Uint16  cmdctrl;
    Uint16  cmdpmod;
    Uint16  cmdcolr;
} ZPOLYGON; //12 bytes each

typedef struct {
    Uint16 StartPoint;
    Uint16 EndPoint;
    Uint16 StartPol;
    Uint16 EndPol;
    Uint16 LightID; //A copy to transfer to the slave
    Sint16 LightDistance;
} ZPDATA;
**/
#define	    GRTBL(r,g,b)	        (((b&0x1f)<<10) | ((g&0x1f)<<5) | (r&0x1f) )
#define  maxColorBrightness 7
const uint16_t minColorBrightness=0;
const uint16_t colorBrightnessMult=maxColorBrightness/2 + 1;

uint16_t getColor(float * color)
{
    uint16_t rgb[XYZ]={0};

    rgb[0]=MIN((uint16_t)((color[0]+1.0f)*colorBrightnessMult), maxColorBrightness);
    rgb[1]=MIN((uint16_t)((color[1]+1.0f)*colorBrightnessMult), maxColorBrightness);
    rgb[2]=MIN((uint16_t)((color[2]+1.0f)*colorBrightnessMult), maxColorBrightness);

    return GRTBL(rgb[0],rgb[1],rgb[2]);
}

void WRITE_ZPDATA(ofstream * file, animated_model_t * aModel, bool preserveLoadedZTP = false)
{
    uint16_t startPt=0;
    uint16_t startPol=0;
    //ZPDATA
    for (unsigned int i=0; i<aModel->nbModels; i++){
        model_t * m = &aModel->model[i];

        //points
        writeUint16(file, startPt);
        startPt += m->nbPoint;
        writeUint16(file, startPt);

        //polygon
        writeUint16(file, startPol);
        startPol += m->nbPolygon;
        writeUint16(file, startPol);

        //Light data, not precalculated, might not even be used
        writeUint16(file, 0);
        writeUint16(file, 0);

        cout << "Model " << i+1 << ", number of vertices : " << m->nbPoint << "\n";
    }

    //Zpoints
    //if (!aModel->nbFrames)
    {
        for (unsigned int i=0; i<aModel->nbModels; i++){
            model_t * m = &aModel->model[i];

            //points
            for (unsigned int ii=0; ii<m->nbPoint; ii++) {
                for (unsigned int j=0; j<3; j++) {
                    writeSint8(file, (int8_t)roundCVert(m->pntbl[ii].point[j]));
                }
                //Get color for now, to be changed with a cool precalculated table instead
                ///writeUint8(file, ((uint8_t)getColor(m->pntbl[ii].color)&0x1F));
                writeUint8(file, preserveLoadedZTP ? static_cast<uint8_t>(m->pntbl[ii].normalArray) :
                    normalLookUp((float*)&m->pntbl[ii].normal[X]));
            }
        }
    } //else
    { //Compressed vertices
        vertex_t * v[aModel->nbModels];
        for (unsigned int i=0; i<aModel->nbModels; i++){
            v[i] = aModel->model[i].pntbl;
        }

        for (unsigned int i=0; i<aModel->nbFrames; i++)
        {
            uint32_t v_count = 0;
            for (unsigned int ii=0; ii<aModel->nbModels; ii++)
            {
                aModel->model[ii].pntbl = &aModel->keyFrames[i].cVert[v_count];
                v_count += aModel->model[ii].nbPoint;
                if (!preserveLoadedZTP)
                {
                    recalculateNormals(&aModel->model[ii]);
                    pointNormals(&aModel->model[ii]);
                    calcLight(aModel);
                }
                for (unsigned int k=0; k<(aModel->model[ii].nbPoint); k++) {
                    for (unsigned int j=0; j<3; j++) {
                        writeSint8(file, (int8_t)roundCVert(aModel->model[ii].pntbl[k].point[j]));}
                writeUint8(file, preserveLoadedZTP ? static_cast<uint8_t>(aModel->model[ii].pntbl[k].normalArray) :
                    normalLookUp((float*)&aModel->model[ii].pntbl[k].normal[X]));
                }
            }


            for (unsigned int i=0; i<aModel->nbModels; i++){
                aModel->model[i].pntbl = v[i]; //That was to save the default value I guess?
            }
        }
    }





    startPt=0;
    for (unsigned int i=0; i<aModel->nbModels; i++) {
        model_t * m = &aModel->model[i];
        for (unsigned int ii=0; ii<m->nbPolygon; ii++){
                polygon_t * pol = &m->pltbl[ii];
            for (unsigned int j=0; j<4; j++){
                //Write the index
                uint16_t buf = pol->vertIdx[j]+startPt;
                file->write((char*)&buf, sizeof(uint8_t));
            }
            texture_t * t = &aModel->texture[pol->texture];
            writeUint16(file, t->textureId);
            writeUint16(file, (pol->SGL_ATTR.dir&0x0FFF) |((pol->SGL_ATTR.sorting&3)<<12)
                        | ((pol->SGL_ATTR.flag&1)<<11));
            writeUint16(file, (pol->SGL_ATTR.atrb) /*|(((pol->isMask^1)&1)<<13)*/); //if isMask==1, the polygon is disposable at long range
            writeUint16(file, preserveLoadedZTP ? pol->SGL_ATTR.colno : t->SGL_ATTR.colno);
        }
        startPt+=m->nbPoint;
    }

    for (unsigned int i=0; i<aModel->nbModels; i++) {
        model_t * m = &aModel->model[i];
        for (unsigned int ii=0; ii<m->nbPoint; ii++){
            for (unsigned int j=0; j<3; j++){
                writeSint32(file, toFIXED(aModel->model[i].pntbl[ii].normal[j]));
            }
        }
    }

}

/*****
FOR SGL (mainly for you Ponut64!) : This writes all the PDATA in a sequential order
*****/
void WRITE_SGL_PDATA(ofstream * file, animated_model_t * aModel)
{
    for (unsigned int i=0; i<aModel->nbModels; i++){
        //PDATA, including buffers for the pointers
        writeUint32(file, 0);
        writeUint32(file, aModel->model[i].nbPoint);
        writeUint32(file, 0);
        writeUint32(file, aModel->model[i].nbPolygon);
        writeUint32(file, 0);
        writeUint32(file, 0);//VECTOR TABLE POINTER

        //POINT, 12 bytes each
        for (unsigned int ii=0; ii<aModel->model[i].nbPoint; ii++) {
            for (unsigned int j=0; j<3; j++) {
                writeSint32(file,toFIXED(aModel->model[i].pntbl[ii].point[j]));
            }
        }
        //POLYGON, 12 bytes for normals and 8 bytes for vertices
        for (unsigned int ii=0; ii< aModel->model[i].nbPolygon; ii++) {
            //Normals
            for (unsigned int j=0; j<3; j++) {
                writeSint32(file, toFIXED(aModel->model[i].pltbl[ii].normal[j]));
            }
            //Vertices
            for (unsigned int j=0; j<4; j++) {
                writeUint16(file, aModel->model[i].pltbl[ii].vertIdx[j]);
            }
        }
        //ATTR, 12 bytes each
        for (unsigned int ii=0; ii< aModel->model[i].nbPolygon; ii++)
        {
            file->write((char*)&aModel->model[i].pltbl[ii].SGL_ATTR.flag, sizeof(uint8_t));
            file->write((char*)&aModel->model[i].pltbl[ii].SGL_ATTR.sorting, sizeof(uint8_t));
            writeUint16(file, aModel->model[i].pltbl[ii].SGL_ATTR.texno);
            writeUint16(file, aModel->model[i].pltbl[ii].SGL_ATTR.atrb); //t->SGL_ATTR.atrb|CL_Gouraud| (pol->flag& (MESHon|CL_Trans|CL_Shadow))
            writeUint16(file, aModel->model[i].pltbl[ii].SGL_ATTR.colno);
            writeUint16(file, aModel->model[i].pltbl[ii].SGL_ATTR.gstb);
            writeUint16(file, aModel->model[i].pltbl[ii].SGL_ATTR.dir);
        }
        //VECTOR NORMALS
        for (unsigned int ii=0; ii<aModel->model[i].nbPoint; ii++) //VECTOR : 12 bytes each
        {
            for (unsigned int j=0; j<3; j++)
            {
                writeSint32(file, toFIXED(aModel->model[i].pntbl[ii].normal[j]));
            }
        }
    }
}

float getDist(uint16_t color1, uint16_t color2){
    float c1[XYZ] = {(float)(color1&0x1F), (float)((color1>>5)&0x1F), (float)((color1>>10)&0x1F)};
    float c2[XYZ] = {(float)(color2&0x1F), (float)((color2>>5)&0x1F), (float)((color2>>10) & 0x1F)};

    float dif[XYZ] = {c1[X]-c2[X], c1[Y]-c2[Y], c1[Z]-c2[Z]};
    return sqrtf(dif[X]*dif[X]+dif[Y]*dif[Y]+dif[Z]*dif[Z]);
}


uint16_t getPaletteID(animated_model_t * aModel, texture_t * t, unsigned int idx){
    texture_t * pal = &aModel->texture[aModel->nbTextures];
    float dist=99999.0, newDist=dist;
    uint32_t bestRes=0;
    for (uint32_t i=0; i<pal->height*pal->width; i++){
        newDist=getDist(t->clut[idx], pal->pixel[i].rgb);
        if (newDist<dist){
            dist=newDist;
            bestRes=i;
        }
    }
    return bestRes;
}
unsigned short convert_to_4bpp(unsigned short a, unsigned short b){    return (((a&0xff)<<4) | (b));}
void WRITE_TEXTURES(ofstream * file, animated_model_t * aModel, bool writeDebug = true)
{
    cout << "Writing the textures to binary file...\n\n";
   // uint16_t buf16;
    //uint32_t bufPtr=0;
    texture_t * t;
    for (unsigned short i=0; i<aModel->nbTextures; i++)
    {
        t=&aModel->texture[i];
        writeUint16(file, t->width);
        writeUint16(file, t->height);
        if (t->colorDepth!=COL_32K){
            writeUint16(file, COL_16);
            writeUint16(file, 16);
        } else {
            writeUint16(file, COL_32K);
            writeUint16(file, 0);
        }

        writeUint32(file, 0); //Just a pointer

        if (t->colorDepth==COL_32K) {
            for (unsigned int ii=0; ii< (t->width * t->height); ii++){
                writeUint16(file, t->pixel[ii].rgb);
            }
        } else {
            for (unsigned int ii=0; ii< (t->width * t->height);)
            {
                uint8_t buf = 0;
                buf =  (uint8_t)(convert_to_4bpp(t->pixel[ii].palette_idx, t->pixel[ii+1].palette_idx)); //(((t->pixel[ii].palette_idx &0x0F)<<4) |  (t->pixel[ii+1].palette_idx&0x0F));
                file->write((char*)&buf, sizeof(uint8_t));
                ii+=2;
            }
        }
    }


    extern int USE_SGL;
    for (unsigned short i=0; i<aModel->nbTextures; i++)
    {
        t=&aModel->texture[i];
       // if (t->colorDepth == COL_32K) continue;
        for (unsigned int ii=0; ii<16; ii++)
        {
            if (USE_SGL==2) {
                    //writeUint16(file, t->clut[ii]);
                writeUint16(file, getPaletteID(aModel, t, ii));
            } else {
                writeUint16(file, t->clut[ii]);}
        }
    }

    #if 0 //Debug : Writes the individual textures generated for each face to files.
    if (useUV) {
        cout << "WRITING PNG TEXTURES\n";
        const char * path = "OUT/TEXTURES/";
        CreateDirectory(path, NULL);
        string modelName = aModel->texture[0].name;
        for (int i=0; i<aModel->nbTextures; i++){
            char s[6];
            sprintf(s, "%06d", i);
            string postVal(s);
            string filename = "OUT/TEXTURES\\" + modelName + s + ".PNG";
            std::vector<unsigned char> png;
            //unsigned char image[aModel->texture[i].height*aModel->texture[i].width*4];
            int ii=0;
            for (int h=aModel->texture[i].height-1; h>=0; h--){
                for (int w=aModel->texture[i].width-1; w>=0; w--){
                    pixel_t * pixel = &aModel->texture[i].pixel[h*aModel->texture[i].width+w];
                   /* image[ii++] = pixel->r;
                    image[ii++] = pixel->g;
                    image[ii++] = pixel->b;
                    image[ii++] = pixel->a;*/
                    png.push_back((pixel->rgb&31)*8);
                    png.push_back(((pixel->rgb>>5)&31)*8);
                    png.push_back(((pixel->rgb>>10)&31)*8);
                    png.push_back(((pixel->rgb>>15)&1)*255);
                }

            }
          ///  cout << "TRY......................\n";
            unsigned error = lodepng::encode(filename , png, aModel->texture[i].width, aModel->texture[i].height);
            if (error) cout << error << " : ERROR\n";
            //if(!error) lodepng::save_file(png, filename);
//            else cout << "ERROR WRITING TEXTURE " << i << "\n";
          //  break;

        }
    }
    #endif // 0




    if (!writeDebug) return;
    cout << "Writing the textures to text file (for debugging)...\n\n";
    ofstream tFile("OUT/TEXTURES.TXT", ios::out);
    if (!tFile.is_open()) {cout << "ERROR WRITING TEXT FILE TEXTURE DATA...\n"; return;}

    tFile << "//Text version of the texture data. FOR TESTING/DEBUGGING ONLY! USE THE BINARY FORMAT INSTEAD!\n\n";
    tFile << "//Uint32 nbTextures = " << aModel->nbTextures << ";\n";
    tFile << "\nstruct {\n Uint16 w;\nUint16 h;\nUint16 * pixData;\n} textData;\n\n";

    tFile << "textData testTextures[" << aModel->nbTextures << "];\n\n";


    for (unsigned short i=0; i<aModel->nbTextures; i++)
    {
        t=&aModel->texture[i];
        tFile << "\n//Texture " << i << "\n";
        tFile << "//Width = " << t->width << ", height = " << t->height << "\n";
        tFile << "Uint16 textData TEXTURE" << i << " = {\n";
        tFile << t->width << ", " << t->height << ", \n{\n";
        for (unsigned int ii=0; ii<t->height*t->width; ii+=8)
        {
            for (unsigned int j=0; j<8 && ii+j<t->height*t->width; j++)
            {
                tFile << "C_RGB(" << (int)(t->pixel[ii+j].rgb&0x1F) << ", " <<
                (int)((t->pixel[ii+j].rgb>>5)&0x1F)  << ", " <<
                (int)((t->pixel[ii+j].rgb>>10)&0x1F) << "), ";
            }
            tFile << "\n";
        }
        tFile << "}\n};\n\n";
    }

    for (uint32_t i=0; i<aModel->model[0].nbPolygon; i++)
    {
        polygon_t * pol = &aModel->model[0].pltbl[i];
        for (uint32_t ii=0; ii<4; ii++) {
            tFile << pol->vertUV[ii].U << ", " << pol->vertUV[ii].V << " : ";
        }
        tFile << "\n";
    }
    tFile.close();

}







/**
Writes the vertex data and compressed normals (to keep SGL happy)
**/
void writeAnim(ofstream * file, animated_model_t * aModel, unsigned int FrameID)
{
    if (!USE_SGL) return;


    int32_t bSint32 = swap_endian_sint(FrameID);
    uint8_t bUint8 = 0;
    file->write((char*)&bSint32, sizeof(int32_t));
    file->write((char*)&bSint32, sizeof(int32_t));
    unsigned int tot_vertices=0, tot_normals=0;

    for (unsigned int i=0; i< aModel->nbModels; i++ ) {
        for (unsigned int ii=0; ii<(aModel->model[i].nbPoint); ii++) {
            for (unsigned int j=0; j<3; j++) {
                ///if (!USE_SGL)writeSint8(file, roundCVert(aModel->keyFrames[FrameID].cVert[tot_vertices].point[j]));
                ///else
                    writeSint16(file, toFIXED4(aModel->keyFrames[FrameID].cVert[tot_vertices].point[j]));
            }
            ///if (!USE_SGL) writeUint8(file, (uint8_t)getColor(aModel->keyFrames[FrameID].cVert[ii].color)); //That means that I didn't recalculate the light values yet...
            tot_vertices++;
        }
    }



    while (tot_vertices%2 !=0) {
        writeSint16(file, 0);
        tot_vertices++;
    }




    for (unsigned int i=0; i< aModel->nbModels; i++ ) {
        for (unsigned int ii=0; ii<(aModel->model[i].nbPolygon); ii++) {
            bUint8 = (uint8_t)normalLookUp((float*)&aModel->model[i].pltbl[ii].normal[X]); //baseModel->animation[FrameID].cNorm[tot_normals]=(uint8_t)(normalLookUp(kfModel->pol[i].pltbl[ii].norm));
            file->write((char*)&bUint8, sizeof(uint8_t));
            tot_normals++;
        }
    }
    while (tot_normals%4 !=0) {
        file->write((char*)(&bSint32), sizeof(int8_t));
        tot_normals++;
    }
}



/********************************
* WIP : Writes data to custom binary format
*********************************/
void write_model_binary(ofstream * file, animated_model_t * aModel, bool preserveLoadedZTP)
{
    const int previousSgl = USE_SGL;
    if (preserveLoadedZTP) USE_SGL = 0;
    WRITE_MDATA(file, aModel);
    WRITE_TEXTURES(file, aModel, !preserveLoadedZTP);
    if (USE_SGL==1)
        WRITE_SGL_PDATA(file, aModel);
    else {
        WRITE_ZPDATA(file, aModel, preserveLoadedZTP);
        USE_SGL = previousSgl;
        return;
    }

    vertex_t * v[aModel->nbModels];
    for (unsigned int i=0; i<aModel->nbModels; i++)
        v[i] = aModel->model[i].pntbl;


    for (unsigned int i=0; i<aModel->nbFrames; i++)
    {
        uint32_t v_count = 0;
        for (unsigned int ii=0; ii<aModel->nbModels; ii++)
        {
            aModel->model[ii].pntbl = &aModel->keyFrames[i].cVert[v_count];
            v_count += aModel->model[ii].nbPoint;
            recalculateNormals(&aModel->model[ii]);
        }

        //compareAnimationData(model, &animModel, i);
        writeAnim(file, aModel, i);
        for (unsigned int i=0; i<aModel->nbModels; i++) {
            aModel->model[i].pntbl = v[i];
        }
    }
    USE_SGL = previousSgl;
}

void write_model_binary(ofstream * file, animated_model_t * aModel)
{
    write_model_binary(file, aModel, false);
}

bool saveZTPFile(const string & path, animated_model_t * model)
{
    if (!model || !model->nbModels) return false;
    char destination[MAX_PATH] = {}, temporary[MAX_PATH] = {};
    DWORD length = GetFullPathNameA(path.c_str(), MAX_PATH, destination, NULL);
    if (!length || length >= MAX_PATH) return false;
    const string fullPath = destination;
    const string folder = fullPath.substr(0, fullPath.find_last_of("\\/"));
    if (!GetTempFileNameA(folder.c_str(), "ztp", 0, temporary)) return false;
    ofstream output(temporary, ios::binary | ios::trunc);
    bool written = output.is_open();
    if (written)
    {
        write_model_binary(&output, model, true);
        output.close();
        written = output.good();
    }
    const bool saved = written && MoveFileExA(temporary, destination,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!saved) DeleteFileA(temporary);
    return saved;
}


/******
player->SPEED[Z] = AslMulFX(std::cos(player->ROTATION[Y]*conv), (player->MOVEMENT_SPEED)) +
        AslMulFX(std::sin(player->ROTATION[Y]*conv), (player->LATERAL_SPEED));

    player->SPEED[X] = -AslMulFX(std::sin(player->ROTATION[Y]*conv), (player->MOVEMENT_SPEED)) +
        AslMulFX(std::cos(player->ROTATION[Y]*conv), (player->LATERAL_SPEED));

******/

void writeNormLUT(void)
{
    ofstream file("OUT/Dynamic_LUT.H", ios::out);
    if (!file.is_open()) {cout << "ERROR...\n"; return ;}

    #define rIncrement 16
    float angIncr = 360.0/rIncrement;
    float angIncrX = 180.0f/(4);
    float defLight[3]={0, 0, -1.0f};

    file << "#ifndef     Dynamic_LUT__H\n";
    file << "#define     Dynamic_LUT__H\n\n\n";
    file << "//This file is for the lookup tables with the dot product precomputed.\n//It works by increments of " << rIncrement << " along the Y axis, " <<
            256/rIncrement << " along the X axis.\n//You just need to set the proper offsets and that gives you the lighting value for nearly free!\n";
    file << "//This file is the same for every model, no need to keep different lookup tables!\n";
   // file << "//Filesize = " << (256*Max_Normals+1023)/1024 << " KB\n";
    file << "\nuint8_t LIGHT_LUT[]=\n{\n";
    uint32_t n;
    int32_t xRot=0;
    VECTOR norm, ans;
    int cnt=0;

    for (xRot=-4; xRot<4; xRot++)
        {
        for (uint32_t i=0;i<rIncrement;i++){
            ztUnitMatrix(0);
            ztRotX(-xRot*angIncrX, &mtx[0][0]);
            ztRotY(i*angIncr, &mtx[0][0]);


           // file << "// " << cnt++ << "\n"; /// : " << toFIXED(norm[X])  << ", " << toFIXED(norm[Y]) << ", " << toFIXED(norm[Z]) << "\n";
            for (n=0; n<Max_Normals; n++)
            {
                for (int j=0; j<3; j++){
                   norm[X]=normLUT[n][X]; norm[Y]=normLUT[n][Y]; norm[Z]=normLUT[n][Z];
                }

                ztCalc3dPoint((float*)&norm[0], (float*)&ans[0], &mtx[0][0]);

                float d = dotProduct((float*)&ans, (float*)defLight);

                ///TEST
                /*if (xRot>0) file << 31 << ", ";
                else*/
                file << MAX(0, MIN((uint16_t)((d+1)* 5), 8)) << ", ";
            }
            file << "//Lines " << cnt++ << ", normal : " << toFIXED(ans[X]) << ", " << toFIXED(ans[Y]) << ", " << toFIXED(ans[Z]) << "\n";
        }

    }

    file << "};\n\n";
    file << "#endif\n\n";
}


void writePalette(animated_model_t * a)
{
    ofstream file("OUT/PALETTE.H", ios::out);
    if (!file.is_open()) {cout << "ERROR...\n"; return ;}

    file << "#ifndef     PALETTE_H\n";
    file << "#define     PALETTE_H\n\n\n";

    file << "uint8_t PALETTE_CRAM[]={\n";
    uint32_t i;
    texture_t * t = &a->texture[a->nbTextures];
    for (i=0;i<t->width*t->height;i++)
    {
       file << t->pixel[i].rgb << ", ";
       if (i%8==7) file << "\n";
    }
    file << "\n};\n\n";
    file << "#endif\n\n";
}


