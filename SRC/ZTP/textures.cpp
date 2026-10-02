#include "../COMMON.H"
#include <algorithm>
#include <vector>
/*****
CREDIT FOR TGA LOADING FUNCTIONS : JOHANNES FETZ, JO ENGINE
DIRTY ADAPTATION BY XL2
******/








#define			CONVERT_COLOR(TGA, IDX)		        (((*((unsigned char *)(TGA + IDX))))/8 & 0x1f)

#define         TGA_RGB(R, G, B)		                    (0x8000 | ((((B))<<10) | (((G)) << 5) | (R)))

#define			TGA_24BITS_GET_PIXEL(TGA, X, Y, WIDTH)	TGA_RGB(CONVERT_COLOR(TGA, (((X) * 3) + 2) + ((Y) * (WIDTH * 3))), \
                                                                CONVERT_COLOR(TGA, (((X) * 3) + 1) + ((Y) * (WIDTH * 3))), \
                                                                CONVERT_COLOR(TGA, (((X) * 3)) + ((Y) * (WIDTH * 3))))

#define			TGA_32BITS_GET_PIXEL(TGA, X, Y, WIDTH)	TGA_RGB(CONVERT_COLOR(TGA, ((4*X) + 2) + ((Y) * (4*WIDTH))), \
                                                                CONVERT_COLOR(TGA, ((4*X) + 1) + ((Y) * (4*WIDTH))), \
                                                                CONVERT_COLOR(TGA, ((4*X) + 0) + ((Y) * (4*WIDTH))))




/**** Bytes alignment can be problematic, and since I'm only trying to read the file format, I just went all-short to save time  ****/
typedef struct
{
   unsigned short  idlength;
   unsigned short  colourmaptype;
   unsigned short  datatypecode;
   short colourmaporigin;
   short colourmaplength;
   unsigned short  colourmapdepth;
   short x_origin;
   short y_origin;
   short width;
   short height;
   unsigned short  bitsperpixel;
   unsigned short  imagedescriptor;
} TGA_HEADER;





/****/
uint16_t CompressColor(uint16_t color, int compression)
{
    if (color == 0)
        return 0;
    else
        return color & (64478<<(compression));
}

bool checkSimilarColor(uint16_t color1, uint16_t color2, int maxDistance)
{
    if ((color1&(1<<15)) != (color2 & (1<<15))) return false;
    int redDifference = (color1 & 0x1F) - (color2 & 0x1F);
    int greenDifference = ((color1 >> 5) & 0x1F) - ((color2 >> 5) & 0x1F);
    int blueDifference = ((color1 >> 10) & 0x1F) - ((color2 >> 10) & 0x1F);
    return redDifference * redDifference + greenDifference * greenDifference + blueDifference * blueDifference
        <= maxDistance * maxDistance;
}

/**This function fills the current image's palette in a 16 colors palette**/
int FillPalette(texture_t * texture, int compression)
{
   /** if (texture->width*texture->height<16){
        texture->colorDepth = COL_32K;
        return 0;
    }*/
    if (compression > 0 || !(specialOption&1)) {
        if (!(specialOption&1)) cout << "Texture no. " << texture->textureId << " set to 16 bpp\n";
        else        cout << "Texture no. " << texture->textureId <<" has more than 16 colors, can't compress more, resorting to 16 bpp\n";
        texture->colorDepth = COL_32K;
        return 0;
    }
    for (unsigned int i=0; i<16; i++) texture->clut[i]=0x00;
    unsigned short total_palette = 0;
    bool exceededPalette = false;
    std::vector<uint8_t> colorPaletteIndex(65536, 0xFF);

   // uint16_t tmpPalette[texture->width*texture->height]={0};
    //First pass to reserve the first palette entry for transparency, only if transparency is used
    for (unsigned int i=0; i< (unsigned int)(texture->width*texture->height); i++)
    {
        if (texture->pixel[i].rgb==0)
        {
            total_palette=1;
            texture->clut[0]=0;
            break;
        }
    }

    for (unsigned int i=0; i< (unsigned int)(texture->width*texture->height); i++)
    {
        const uint16_t color = texture->pixel[i].rgb;
        uint8_t paletteIndex = colorPaletteIndex[color];
        if (paletteIndex != 0xFF) {
            texture->pixel[i].palette_idx = paletteIndex;
            continue;
        }

        if (color == 0) {
            paletteIndex = 0;
        } else {
            for (unsigned short ii=0; ii<total_palette; ii++) {
                if (checkSimilarColor(color, texture->clut[ii], compression+1)) {
                    paletteIndex = static_cast<uint8_t>(ii);
                    break;
                }
            }

            if (paletteIndex == 0xFF) {
                if (total_palette >= 16) {
                    paletteIndex = 15;
                    exceededPalette = true;
                } else {
                    paletteIndex = static_cast<uint8_t>(total_palette);
                    texture->clut[total_palette] = color;
                    total_palette++;
                }
            }
        }
        colorPaletteIndex[color] = paletteIndex;
        texture->pixel[i].palette_idx = paletteIndex;
    }
    if (exceededPalette)
    {
        int compr=compression + 1;
        cout << "More than 16 colors...reducing the amount of colors...\n";
        return FillPalette(texture, compr);
    }
    else
    {
        texture->colorDepth=COL_16; //4 bpp
        if (compression >= 0)
            cout << "The image contains a total of " << total_palette << " colors after compression.\n";
        else
            cout << "The image contains a total of " << total_palette << " colors.\n";
        return 1;
    }

}


int ReadTGAHeader(ifstream *ibinfile, TGA_HEADER * header)
{
    ZeroMemory(header, sizeof(*header));
    ibinfile->read((char*)&header->idlength, sizeof(uint8_t));
    ibinfile->read((char*)&header->colourmaptype, sizeof(uint8_t));
    ibinfile->read((char*)&header->datatypecode, sizeof(uint8_t));
    ibinfile->read((char*)&header->colourmaporigin, sizeof(uint16_t));
    ibinfile->read((char*)&header->colourmaplength, sizeof(uint16_t));
    ibinfile->read((char*)&header->colourmapdepth, sizeof(uint8_t));
    ibinfile->read((char*)&header->x_origin, sizeof(uint16_t));
    ibinfile->read((char*)&header->y_origin, sizeof(uint16_t));
    ibinfile->read((char*)&header->width, sizeof(uint16_t));
    ibinfile->read((char*)&header->height, sizeof(uint16_t));
    ibinfile->read((char*)&header->bitsperpixel, sizeof(uint8_t));
    ibinfile->read((char*)&header->imagedescriptor, sizeof(uint8_t));
    return *ibinfile ? 1 : 0;
}


int ReadTGAData(ifstream * file, texture_t * img, unsigned short bits)
{
    const size_t bytesPerPixel = bits / 8;
    const size_t pixelCount = static_cast<size_t>(img->width) * img->height;
    std::vector<uint8_t> rawPixels(pixelCount * bytesPerPixel);
    file->read(reinterpret_cast<char*>(rawPixels.data()), static_cast<std::streamsize>(rawPixels.size()));
    if (!*file) return 0;

    for (size_t sourceRow=0; sourceRow<img->height; sourceRow++) {
        const size_t y = useUV ? sourceRow : img->height - 1 - sourceRow;
        for (size_t x=0; x<img->width; x++) {
            const size_t sourceIndex = (sourceRow * img->width + x) * bytesPerPixel;
            pixel_t & pixel = img->pixel[y * img->width + x];
            const uint8_t blue = rawPixels[sourceIndex];
            const uint8_t green = rawPixels[sourceIndex + 1];
            const uint8_t red = rawPixels[sourceIndex + 2];

            if (bits == 32) {
                pixel.rgb = ((rawPixels[sourceIndex + 3] / 8) & 0x1F) == 0
                    ? 0
                    : TGA_RGB((red / 8) & 0x1F, (green / 8) & 0x1F, (blue / 8) & 0x1F);
                pixel.a = rawPixels[sourceIndex + 3];
            } else {
                pixel.rgb = TGA_RGB((red / 8) & 0x1F, (green / 8) & 0x1F, (blue / 8) & 0x1F);
                pixel.a = 0xFF;
            }
            pixel.r = red;
            pixel.g = green;
            pixel.b = blue;
        }
    }
    return 1;
}

void textureSpecialAttributes(texture_t * texture)
{
        stringstream classType(texture->name);            string bufferStr;
        getline(classType, bufferStr, '_');

        int tot=1;
        string c[20]; //Stupidly high for no real reasons, could be smarter
        c[0]=bufferStr;
        texture->nbFrames=1;
        texture->attributes=0;
        while (tot < 20 && getline(classType, bufferStr, '_'))
        {  //cout << c[tot];
            c[tot]=bufferStr;
            //if (c[tot]=="ANIM") cout << "anim!!!!!!!!!!!!!!!!!\n\n\n!!!!!!\n";
            tot++;
        }
        for (int i=0; i<20; i++)//Stupidly high for no real reasons, could be smarter
        {
            if (c[i]=="ANIM")
            {
                if (i + 1 < 20 && c[i+1]!= "")
                texture->nbFrames=std::atoi(c[i+1].c_str());
            }
            if (c[i]=="2D")
                texture->attributes|=TEX_IS_2D|TEX_NO_GOURAUD;

        }
       // cout << "TOTAL TESTING ATTRIBUTES : " << tot << ", total frames : " << texture->nbFrames << "\n";
}

/**If the loading of the texture fails, at least you have a sad face to look at!**/
void createFakeTexture(texture_t * t)
{
    t->width = 8;
    t->height = 8;
    t->pixel = new pixel_t[64]();
    for (uint16_t i=0; i<64; i++)
    {
        t->pixel[i].a=t->pixel[i].r=t->pixel[i].g=t->pixel[i].b=255;
        t->pixel[i].rgb = 0xFFFF;
    }
    t->pixel[2*8+2].rgb = C_RGB(1, 1, 1);
    t->pixel[2*8+5].rgb = C_RGB(1, 1, 1);
    t->pixel[3*8+6].rgb = C_RGB(1, 1, 31);

    t->pixel[5*8+2].rgb = C_RGB(1, 1, 1);
    t->pixel[5*8+3].rgb = C_RGB(1, 1, 1);
    t->pixel[5*8+4].rgb = C_RGB(1, 1, 1);
    t->pixel[5*8+5].rgb = C_RGB(1, 1, 1);
    t->pixel[6*8+1].rgb = C_RGB(1, 1, 1);
    t->pixel[6*8+6].rgb = C_RGB(1, 1, 1);
    FillPalette(t, -1);
    textureSpecialAttributes(t);

}

/**TO DO : ADD RLE SUPPORT */
int ReadTGAFile (string folder, texture_t * texture)
{

/**PNG**/
#if 0
    std::vector<unsigned char> png;
    std::vector<unsigned char> image; //Raw pixels

    //Load and decode PNG
    string pngName = folder + texture->name + ".PNG";
    unsigned error = lodepng::load_file(png, pngName);
    if (!error) {

        unsigned int width=0, height=0;
        error=lodepng::decode(image, width, height, png);
        texture->width=width;
        texture->height=height;
texture->pixel = new pixel_t[height*width];
      //  int tot =texture->width*texture->height;
        int i=0;

        for (int h=texture->height-1; h>=0; h--){
            for (int w=0; w<texture->width; w++){
                unsigned char * p = &image[(h*texture->width+w) *4];
                if (p[3] < 128) {
                    texture->pixel[i].a=texture->pixel[i].r=texture->pixel[i].g=texture->pixel[i].b=0;
                } else {
                    texture->pixel[i].r = p[0];
                    texture->pixel[i].g = p[1];
                    texture->pixel[i].b = p[2];
                    texture->pixel[i].a = p[3];
                }
                texture->pixel[i].rgb=C_RGB(texture->pixel[i].r/8, texture->pixel[i].g/8, texture->pixel[i].b/8);
                i++;
            }
        }
        FillPalette(texture, -1);
        textureSpecialAttributes(texture);
        return 1;
    }
#endif
/**TGA**/
    //cout << "\nAttempting to read TGA texture files...\n";
    string name;
    name =  folder + texture->name + ".TGA"; //Temporary : Will break under several conditions
    TGA_HEADER header = {};
    ifstream ibinfile(name.c_str(), ios::in | ios::binary);

    if (!ibinfile.is_open())
        {
            cout << "ERROR : COULDN'T LOAD FILE " << texture->name.c_str() << ".TGA, creating a sad face texture\n";
            createFakeTexture(texture);
            return -1;
        }

    if (!ReadTGAHeader(&ibinfile, &header) || header.width <= 0 || header.height <= 0 ||
        header.colourmaptype != 0 || (header.bitsperpixel != 24 && header.bitsperpixel != 32))
    {
        cout << "ERROR : INVALID OR UNSUPPORTED TGA HEADER " << name << "\n";
        createFakeTexture(texture);
        return -1;
    }
        cout << "Texture " << texture->name << ", size : " << header.width << "x" << header.height << ", pixel depth : " << header.bitsperpixel << ", RLE = " << header.datatypecode << "\n";

    if (header.datatypecode!=2) {cout << "RLE currently not supported\n"; createFakeTexture(texture); return -1;}
    texture->width = header.width;
    texture->height = header.height;
    texture->pixel = new pixel_t[header.height*header.width]();

    ibinfile.ignore(header.idlength);
    if (!ReadTGAData(&ibinfile, texture, header.bitsperpixel))
    {
        cout << "ERROR : INCOMPLETE TGA PIXEL DATA " << name << "\n";
        delete [] texture->pixel;
        texture->pixel = NULL;
        createFakeTexture(texture);
        return -1;
    }
    ibinfile.close();

    #if 0
    if ((!(specialOption&1)) || (texture->width*texture->height >= 128*128)){
        texture->colorDepth=COL_32K;
    } else {
        FillPalette(texture, -1);
    }
    #else
    FillPalette(texture, -1);
    #endif

    textureSpecialAttributes(texture);

    return 7777;
}


int checkIsTriangle(polygon_t * pol){
    for (int i=0; i<4; i++){
        for (int j=0; j<4; j++){
            if (j==i) continue;
            if (pol->vertIdx[i]==pol->vertIdx[j]) return j;
        }
    }
    return 0;
}

#define uvTol (1.0f/256.0f)
int uvCompare(polygon_t * pol1, polygon_t * pol2){
 /*   if (pol1->vertIdx[0]==pol1->vertIdx[1] ||
        pol1->vertIdx[0]==pol1->vertIdx[3] ||
        pol1->vertIdx[2]==pol1->vertIdx[3] ||
        pol1->vertIdx[2]==pol1->vertIdx[1]) return 0; //Triangles...not working well atm...*/
    if (checkIsTriangle(pol1) && !checkIsTriangle(pol2)) return 0;
    if (checkIsTriangle(pol2) && !checkIsTriangle(pol1)) return 0;

    int cnt = 0;
    for (int i=0; i<4; i++){
        for (int j=0; j<4; j++){
            if ((fabs(pol1->vertUV[i].U-pol2->vertUV[j].U)<=uvTol)  && (fabs(pol1->vertUV[i].V-pol2->vertUV[j].V)<=uvTol)){
                cnt++;
                break;
            }
        }
    }
    if (cnt==4) return 1;
    return 0;
}



void rotatePol(polygon_t * pol);
void textureFlip(polygon_t * pol);
int checkExists (polygon_t * pol, animated_model_t * aModel, const string & sourceName){
    for (int m=0; m<aModel->nbModels; m++){
       polygon_t * newPol = &aModel->model[m].pltbl[0];
       for (int i=0; i<aModel->model[m].nbPolygon; i++, newPol++){
            if (newPol==pol) return 0;
            if (newPol->texture < 0 || newPol->texture >= static_cast<int>(aModel->nbTextures) ||
                aModel->texture[newPol->texture].name != sourceName) continue;
            if (uvCompare(pol, newPol)){
                pol->texture = newPol->texture;
                memcpy(&pol->SGL_ATTR, &newPol->SGL_ATTR, sizeof(ATTR));


                int maxTry=0;
                int idx = (checkIsTriangle(pol))&3;
                while (1){
                    if ((fabs(pol->vertUV[idx].U-newPol->vertUV[idx].U)<=uvTol)  && (fabs(pol->vertUV[idx].V-newPol->vertUV[idx].V)<=uvTol)) break;
                    rotatePol(pol);
                    if (++maxTry >= 4) {
                       cout << "(!) Weird rotation bug...\n";
                       break;
                       // exit(123456789);
                    }
                }
                //Triangle = buggy...
                if (((fabs(pol->vertUV[1].U-newPol->vertUV[1].U)<=uvTol)  && (fabs(pol->vertUV[1].V-newPol->vertUV[1].V)<=uvTol))&&
                    ((fabs(pol->vertUV[2].U-newPol->vertUV[2].U)<=uvTol)  && (fabs(pol->vertUV[2].V-newPol->vertUV[2].V)<=uvTol))) return 1; //No flip, all good
                /*if (((fabs(pol->vertUV[1].U-newPol->vertUV[3].U)<=uvTol)  && (fabs(pol->vertUV[1].V-newPol->vertUV[3].V)<=uvTol)) &&
                    ((fabs(pol->vertUV[3].U-newPol->vertUV[1].U)<=uvTol)  && (fabs(pol->vertUV[3].V-newPol->vertUV[1].V)<=uvTol))){*/
                    //V flip
                    while (idx) {
                        rotatePol(pol);
                        idx--;
                    }
                    textureFlip(pol);
                    rotatePol(pol);
                    rotatePol(pol);
                    rotatePol(pol);
                   /* maxTry=0;
                   while (((fabs(pol->vertUV[1].U-newPol->vertUV[3].U)>uvTol)  || (fabs(pol->vertUV[1].V-newPol->vertUV[3].V)>uvTol))){
                        rotatePol(pol);
                        if (++maxTry > 10) {
                            cout << "BUG...\n";
                            exit(123456789);
                        }
                   }*/
                   /* rotatePol(pol);

                    textureFlip(pol);*/
                    return 1;
              //  }
                /*if (((fabs(pol->vertUV[1].U-newPol->vertUV[3].U)<=uvTol)  && (fabs(pol->vertUV[1].V-newPol->vertUV[3].V)<=uvTol)) &&
                    ((fabs(pol->vertUV[3].U-newPol->vertUV[1].U)<=uvTol)  && (fabs(pol->vertUV[3].V-newPol->vertUV[1].V)<=uvTol))){
                    //V flip
                    rotatePol(pol);
                    textureFlip(pol);
                    textureFlip(pol);
                }*/

                //Check winding, then Rotate/flip fix here...
               // return 1;
            }

       }
    }

    return 0;
}

void  createTexture( polygon_t * pol, animated_model_t * aModel ) {
    createTexture(pol, aModel, skin);
}

void createTexture(polygon_t * pol, animated_model_t * aModel, const texture_t & sourceTexture) {
    if (!useUV) return;
    if (checkExists(pol, aModel, sourceTexture.name)) return;

    float bbMin[2]={99999, 99999};
    float bbMax[2]={-99999, -99999};

    for (uint32_t i=0; i<4; i++){
        for (uint32_t j=0; j<3; j++)
        {
            if (pol->vertUV[i].U > bbMax[0]) bbMax[0]=pol->vertUV[i].U;
            if (pol->vertUV[i].V > bbMax[1]) bbMax[1]=pol->vertUV[i].V;
            if (pol->vertUV[i].U < bbMin[0]) bbMin[0]=pol->vertUV[i].U;
            if (pol->vertUV[i].V < bbMin[1]) bbMin[1]=pol->vertUV[i].V;
        }
    }

    /*
    for (uint32_t i=0; i<2; i++){ //Useless?
        if (bbMin[i]<0) bbMin[i]=0;
        if (bbMax[i]>=1) bbMax[i]=0.9999;
    }*/
    /**for (uint32_t i=0; i<2; i++){
       bbMax[i]-=0.1;
       if (bbMax[i]<bbMin[i]) bbMax[i]=bbMin[i];
    }*/


    pol->texture = aModel->nbTextures;
    texture_t * text = &aModel->texture[aModel->nbTextures];
    aModel->texture[aModel->nbTextures].textureId = aModel->nbTextures;
    text->name = sourceTexture.name;
    text->width = MAX( ((int)(sourceTexture.width*(bbMax[0]-bbMin[0])+7)/8)<<3, 8);
    if (specialOption &  (1<<1))
        text->height = MAX( ((int)(sourceTexture.height*(bbMax[1]-bbMin[1])+4)/8)<<3, 8);
    else
        text->height = MAX((int)sourceTexture.height*(bbMax[1]-bbMin[1]), 1);
    text->pixel = new pixel_t[text->width*text->height]();

    cout << "Texture width = " << text->width << ", height = " << text->height << "\n";

    float startX1 = (pol->vertUV[3].U-pol->vertUV[0].U)/text->height;
    float startY1 = (pol->vertUV[3].V-pol->vertUV[0].V)/text->height;

    float startX2 = (pol->vertUV[2].U-pol->vertUV[1].U)/text->height;
    float startY2 = (pol->vertUV[2].V-pol->vertUV[1].V)/text->height;

   // double frac[2] = {1.0/(double)(skin.width), 1.0/(double)(skin.height)};
    int uvPos[2];
    int j=0;
    for (uint32_t y=0; y<text->height; y++) {

        float x1 = pol->vertUV[0].U + (startX1*y);
        float x2 = pol->vertUV[1].U + (startX2*y);
        float y1 = pol->vertUV[0].V + (startY1*y);
        float y2 = pol->vertUV[1].V + (startY2*y);

        for (uint32_t x=0; x<text->width; x++, j++) {

            /*uvPos[0] = MAX(0, MIN(text->width-1, (int)(x1+((x2-x1)/text->width))*x));
            uvPos[1] = MAX(0, MIN(text->height-1, (int)(y1+((y2-y1)/text->height))*x));*/

            uvPos[0] = ((int)((x1 + ((x2-x1)/text->width)*x) * sourceTexture.width));
            uvPos[0] = MAX(0, MIN(static_cast<int>(sourceTexture.width) - 1, uvPos[0]));
            uvPos[1] = ((int)((y1 + ((y2-y1)/text->width)*x) * sourceTexture.height));
            uvPos[1] = MAX(0, MIN(static_cast<int>(sourceTexture.height) - 1, uvPos[1]));

            const pixel_t & sourcePixel = sourceTexture.pixel[uvPos[Y]*sourceTexture.width + uvPos[X]];
            text->pixel[j].rgb = sourcePixel.rgb;
            text->pixel[j].r = sourcePixel.r;
            text->pixel[j].g = sourcePixel.g;
            text->pixel[j].b = sourcePixel.b;
            if (sourcePixel.rgb==0){
                text->pixel[j].a = 0;
            } else {
                text->pixel[j].a = 255;
            }


        }
    }

    extern void specialConditions(unsigned short, unsigned short, animated_model_t*);
    //text->colorDepth=COL_32K;

    #if 0
    if ((!(specialOption&1)) || text->width*text->height >= 128*128){
        text->colorDepth=COL_32K;
    } else {
        FillPalette(text, -1);
    }
    #else
    FillPalette(text, -1);
    #endif
    textureSpecialAttributes(text);

    cout << "\nTEST..." << aModel->nbTextures << "\n";
    aModel->nbTextures++;

    specialConditions(aModel->nbTextures-1, aModel->nbTextures, aModel);
    pol->SGL_ATTR = aModel->texture[pol->texture].SGL_ATTR;


    ///delete [] bufText->pixel;
    ///delete bufText;
}

#if 0
int readPNGfile(string folder, texture_t * texture){
    string filename;
    filename =  folder + texture->name + ".PNG"; //Temporary : Will break under several conditions

    std::vector<unsigned char> png;
    std::vector<unsigned char> image; //Raw pixels

    //Load and decode
    string pngName = folder + texture->filename + ".PNG";
    unsigned error = lodepng::load_file(png, pngName);
    if (!error) error=lodepng::decode(image, texture->width, texture->height, png);

    //If there is an error, display it
    if (error) {

            if (texture->name.find("TXR") != string::npos || (texture->defColor[0]+texture->defColor[1]+texture->defColor[2])) {
                texture->width=16;
                texture->height=1;
                cout << filename << ", autogenerated texture : " << texture->width << ", " << texture->height << "\n";
                texture->colorDepth=COL_32K;
                texture->pixel = new pixel_t[texture->width*texture->height];
                for (int i=0; i<texture->width*texture->height; i++){
                    texture->pixel[i].a=255;
                    texture->pixel[i].r=texture->defColor[0];
                    texture->pixel[i].g=texture->defColor[1];
                    texture->pixel[i].b=texture->defColor[2];
                    texture->pixel[i].rgb=C_RGB(texture->pixel[i].r/8, texture->pixel[i].g/8, texture->pixel[i].b/8);
                }
                return 1;
            } else {
                cout << "ERROR : COULDN'T LOAD FILE " << pngName << ", creating a sad face texture\n";
                createFakeTexture(texture);
                return 0;
            }


    }
    texture->pixel = new pixel_t[texture->width*texture->height];
    texture->colorDepth = COL_32K;
    int i=0;
    unsigned char * ptr;

    cout << filename << " : " << texture->width << ", " << texture->height << "\n";
    for (int h=0; h<texture->height; h++){
        for (int w=0; w<texture->width; w++){
            ptr = &image[i*4];
            texture->pixel[i].r = *ptr++;
            texture->pixel[i].g = *ptr++;
            texture->pixel[i].b = *ptr++;
            texture->pixel[i].a = *ptr++;

            #if 0
            if (/*(texture->attributes&TEX_AUTOTRANS) &&*/ (texture->pixel[i].r==texture->defColor[0])
                && (texture->pixel[i].g==texture->defColor[1])
                && (texture->pixel[i].b==texture->defColor[2])) {
                    /*texture->pixel[i].r=texture->pixel[i].g=texture->pixel[i].b=*/texture->pixel[i].a=0;
            }
            #endif // 0

            if (texture->pixel[i].a < 64) texture->pixel[i].rgb=0;
            else  texture->pixel[i].rgb = C_RGB(texture->pixel[i].r/8, texture->pixel[i].g/8, texture->pixel[i].b/8);

            i++;
        }
    }

    return 1;
}
#endif // 0

