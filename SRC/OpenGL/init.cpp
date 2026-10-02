#include "../COMMON.H"


void initOpenGL(void)
{


    glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    float persp = 60.0f * 224.0f/352.0f;
    gluPerspective(persp, 352.0f/224.0f, 188.0f/((float)(1<<6)), 6553600.0);
    glMatrixMode(GL_MODELVIEW);
    //gluLookAt(0.0, 0.0, -0.1, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0);
}

extern unsigned int texId[];
unsigned int specialTextureId[256];
unsigned int asciiTextureId;

void generateMousePickupTexture() { //Lazy workaround...

 for (int j=0; j<256; j++) {
    glGenTextures(1, &specialTextureId[j]);                                // Generate OpenGL texture IDs
    glBindTexture(GL_TEXTURE_2D, specialTextureId[j]);
    GLubyte imgData[256*256*4]={0};
    unsigned int cnt = 0;
    for (int i=0; i<256*256; i++){
        imgData[cnt++]=i&255;
        imgData[cnt++]=(i>>8)&255;
        imgData[cnt++]=(j)&255;
        imgData[cnt++]=255;
    }
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)imgData);
 }

}

void generateASCIIbitmap(){
    unsigned int cnt = 0;
    uint16_t asciiSet[] = {
        #include "ASCII.h"
    };

    #if 0
    for (int y = 0; y<256; y+=32){
        for (int x = 0; x<512; x+=32) {
           for (int yy=y; yy<y+32; yy++){
             for (int xx=x; xx<x+32; xx++) {
                int i = xx+yy*512;
                glGenTextures(1, &asciiTextureId[cnt/4]);
                glBindTexture(GL_TEXTURE_2D, asciiTextureId[cnt/4]);
                imgData[cnt++] = ((asciiSet[i])&0x1f)*8.25; //txt->pixel[ii*txt->width + j].b;
                imgData[cnt++] = ((asciiSet[i]>>5)&0x1f)*8.25; //txt->pixel[ii*txt->width + j].g;
                imgData[cnt++] = ((asciiSet[i]>>10)&0x1f)*8.25; //txt->pixel[ii*txt->width + j].r;
                imgData[cnt++]=255;

                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512*8, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)imgData);
             }
           }
        }
    }
    #else
    glGenTextures(1, &asciiTextureId);                                // Generate OpenGL texture IDs
    glBindTexture(GL_TEXTURE_2D, asciiTextureId);
    GLubyte imgData[256*256*4]={0};
    for (int i=0; i<256*256; i++) {
         imgData[cnt++] = ((asciiSet[i])&0x1f)*8.25; //txt->pixel[ii*txt->width + j].b;
         imgData[cnt++] = ((asciiSet[i]>>5)&0x1f)*8.25; //txt->pixel[ii*txt->width + j].g;
         imgData[cnt++] = ((asciiSet[i]>>10)&0x1f)*8.25; //txt->pixel[ii*txt->width + j].r;
         if (!asciiSet[i] || asciiSet[i]==32768) imgData[cnt++]=0;
         else
            imgData[cnt++]=255;

    }


    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)imgData);
    #endif



}

void setTextures(animated_model_t * aModel)
{
    uint16_t colorValue;
    for (unsigned int i=0; i<(unsigned int)aModel->nbTextures; i++)
    {
        glGenTextures(1, &texId[i]);                                // Generate OpenGL texture IDs
        glBindTexture(GL_TEXTURE_2D, texId[i]);
        texture_t * txt = &aModel->texture[i];

        GLubyte imgData[txt->width*txt->height*4]={0};
        unsigned int cnt=0;

        for (unsigned int ii=0; ii<txt->height; ii++)
        {
            for (unsigned int j=0; j<txt->width; j++)
            {
                if (txt->colorDepth==COL_32K) colorValue=txt->pixel[ii*txt->width + j].rgb;
                else colorValue=txt->clut[txt->pixel[ii*txt->width + j].palette_idx];
                imgData[cnt++] = ((colorValue)&0x1f)*8.25; //txt->pixel[ii*txt->width + j].b;
                imgData[cnt++] = ((colorValue>>5)&0x1f)*8.25; //txt->pixel[ii*txt->width + j].g;
                imgData[cnt++] = ((colorValue>>10)&0x1f)*8.25; //txt->pixel[ii*txt->width + j].r;
                imgData[cnt++] = txt->pixel[ii*txt->width + j].a;
            }
        }
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, txt->width, txt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)imgData);
    }

    generateMousePickupTexture();
    generateASCIIbitmap();

}
