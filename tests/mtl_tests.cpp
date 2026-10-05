#include "../SRC/COMMON.H"
#include "../SRC/ZTP/mtl.h"
#include <cassert>
#include <cstdio>
#include <iostream>

uint32_t specialOption = 1;
uint32_t useUV = 0;
texture_t skin = {};
void rotatePol(polygon_t *) { assert(false); }
void textureFlip(polygon_t *) { assert(false); }
void specialConditions(unsigned short, unsigned short, animated_model_t *) { assert(false); }
extern int ReadTGAFile(std::string folder, texture_t * texture, std::string sourcePath);
extern int checkExists(polygon_t * polygon, animated_model_t * model, const std::string & sourceName);

void testTriangleDeduplication()
{
    texture_t texture = {};
    texture.name = "atlas";
    polygon_t faces[2] = {};
    faces[0].vertIdx[0] = 2;
    faces[0].vertIdx[1] = 2;
    faces[0].vertIdx[2] = 1;
    faces[0].vertIdx[3] = 0;
    faces[0].vertUV[0] = faces[0].vertUV[1] = uv_t{0, 1};
    faces[0].vertUV[2] = uv_t{1, 0};
    faces[0].vertUV[3] = uv_t{0, 0};
    model_t mesh = {};
    mesh.pltbl = faces;
    mesh.nbPolygon = 2;
    animated_model_t model = {};
    model.model = &mesh;
    model.nbModels = 1;
    model.texture = &texture;
    model.nbTextures = 1;

    for (unsigned int referenceRotation = 0; referenceRotation < 4; ++referenceRotation)
    {
        const polygon_t reference = faces[0];
        for (unsigned int rotation = 0; rotation < 4; ++rotation)
            for (unsigned int reflected = 0; reflected < 2; ++reflected)
                for (unsigned int direction = 0; direction < 4; ++direction)
                {
                    faces[0].SGL_ATTR.dir = 2 | (direction << 4);
                    faces[1] = {};
                    faces[1].texture = -1;
                    for (unsigned int corner = 0; corner < 4; ++corner)
                    {
                        const unsigned int sourceCorner = ((corner + rotation) & 3) ^ reflected;
                        faces[1].vertIdx[corner] = reference.vertIdx[sourceCorner] + 10;
                        faces[1].vertUV[corner] = reference.vertUV[sourceCorner];
                    }
                    assert(checkExists(&faces[1], &model, "atlas") == 1);
                    assert(faces[1].texture == 0);
                    const unsigned int flip = ((faces[1].SGL_ATTR.dir ^ faces[0].SGL_ATTR.dir) >> 4) & 3;
                    assert(flip == reflected);
                    assert((faces[1].SGL_ATTR.dir & ~0x30) == 2);
                    for (unsigned int corner = 0; corner < 4; ++corner)
                    {
                        assert(faces[1].vertUV[corner].U == faces[0].vertUV[corner ^ flip].U);
                        assert(faces[1].vertUV[corner].V == faces[0].vertUV[corner ^ flip].V);
                        assert(faces[1].vertIdx[corner] == faces[0].vertIdx[corner ^ flip] + 10);
                    }
                }
        for (unsigned int corner = 0; corner < 4; ++corner)
        {
            faces[0].vertIdx[corner] = reference.vertIdx[(corner + 1) & 3];
            faces[0].vertUV[corner] = reference.vertUV[(corner + 1) & 3];
        }
    }

    faces[1] = {};
    faces[1].vertIdx[0] = faces[1].vertIdx[1] = 10;
    faces[1].vertIdx[2] = 12;
    faces[1].vertIdx[3] = 11;
    faces[1].vertUV[0] = faces[1].vertUV[1] = uv_t{0, 0};
    faces[1].vertUV[2] = uv_t{0, 1};
    faces[1].vertUV[3] = uv_t{1, 0};
    faces[1].texture = -1;
    const polygon_t unmatched = faces[1];
    assert(checkExists(&faces[1], &model, "atlas") == 0);
    assert(faces[1].texture == unmatched.texture);
    assert(faces[1].SGL_ATTR.dir == unmatched.SGL_ATTR.dir);
    for (unsigned int corner = 0; corner < 4; ++corner)
    {
        assert(faces[1].vertIdx[corner] == unmatched.vertIdx[corner]);
        assert(faces[1].vertUV[corner].U == unmatched.vertUV[corner].U);
        assert(faces[1].vertUV[corner].V == unmatched.vertUV[corner].V);
    }
    faces[1] = faces[0];
    assert(checkExists(&faces[1], &model, "other atlas") == 0);
    faces[1].vertIdx[1] = 3;
    assert(checkExists(&faces[1], &model, "atlas") == 0);
    faces[1] = faces[0];
    faces[1].vertUV[2].U += 0.01f;
    assert(checkExists(&faces[1], &model, "atlas") == 0);
    faces[1] = faces[0];
    faces[1].vertUV[2].U += 0.001f;
    assert(checkExists(&faces[1], &model, "atlas") == 1);
}

void testImages()
{
    char tempFolder[MAX_PATH], tempFile[MAX_PATH];
    assert(GetTempPathA(MAX_PATH, tempFolder));
    assert(GetTempFileNameA(tempFolder, "mtl", 0, tempFile));
    const std::string base = tempFile;
    const std::string tgaPath = base + ".TGA";
    const std::string pngPath = base + ".png";
    const unsigned char header[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 2, 0, 24, 0};
    {
        std::ofstream tga(tgaPath, std::ios::binary);
        tga.write(reinterpret_cast<const char *>(header), sizeof(header));
        for (unsigned int pixel = 0; pixel < 16; ++pixel)
        {
            const unsigned char color[3] = {static_cast<unsigned char>(pixel < 8 ? 255 : 0), 0,
                static_cast<unsigned char>(pixel < 8 ? 0 : 255)};
            tga.write(reinterpret_cast<const char *>(color), sizeof(color));
        }
    }
    std::vector<unsigned char> rgba(8 * 2 * 4, 0);
    for (unsigned int pixel = 0; pixel < 16; ++pixel)
    {
        rgba[pixel * 4 + (pixel < 8 ? 0 : 2)] = 255;
        rgba[pixel * 4 + 3] = 255;
    }
    rgba[3] = 0;
    assert(lodepng::encode(pngPath, rgba, 8, 2) == 0);
    for (unsigned int mode = 0; mode < 2; ++mode)
    {
        useUV = mode != 0;
        texture_t texture = {};
        texture.name = "ANIM_4_BODY_2D";
        assert(ReadTGAFile("", &texture, tgaPath) > 0);
        assert(texture.name == "ANIM_4_BODY_2D" && texture.nbFrames == 4);
        assert((texture.attributes & TEX_IS_2D) != 0);
        assert(useUV ? texture.pixel[0].b == 255 : texture.pixel[0].r == 255);
        delete [] texture.pixel;
        texture.pixel = NULL;
        assert(ReadTGAFile("", &texture, pngPath) > 0);
        assert(useUV ? texture.pixel[0].b == 255 : texture.pixel[0].r == 255);
        assert(texture.pixel[useUV ? 8 : 0].rgb == 0);
        delete [] texture.pixel;
    }
    texture_t fallback = {};
    fallback.name = base;
    assert(ReadTGAFile("", &fallback, base + ".missing.png") > 0);
    assert(fallback.width == 8 && fallback.height == 2);
    delete [] fallback.pixel;
    fallback.pixel = NULL;
    assert(ReadTGAFile("", &fallback, base + ".jpg") > 0);
    delete [] fallback.pixel;
    fallback.pixel = NULL;
    assert(ReadTGAFile("", &fallback, pngPath + ".tga") > 0);
    delete [] fallback.pixel;
    fallback.pixel = NULL;
    fallback.name = base + "_absent";
    assert(ReadTGAFile("", &fallback, "") < 0);
    assert(fallback.width == 8 && fallback.height == 8);
    delete [] fallback.pixel;
    std::remove(tgaPath.c_str());
    std::remove(pngPath.c_str());
    std::remove(base.c_str());
}

int main()
{
    ztp_mtl::MaterialTextures materials;
    std::istringstream input(
        "# Blender export\nnewmtl ANIM_4_Body\nKd 1 1 1\n"
        "map_Kd -s 1 1 1 -o -1 0 -clamp on \"textures/body color.png\" # comment\n"
        "newmtl Plain material\nmap_Kd textures/plain color.tga\n"
        "newmtl Windows\nmap_Kd C:\\Textures\\body.tga\n"
        "newmtl Empty\nKd 0 0 0\n"
        "newmtl Unsupported\nmap_Kd -unknown value file.tga\n"
        "newmtl Broken\nmap_Kd -mm 1\n");
    ztp_mtl::parse(input, "models/materials/", materials);
    assert(materials["ANIM_4_Body"] == "models/materials/textures/body color.png");
    assert(materials["Plain material"] == "models/materials/textures/plain color.tga");
    assert(materials["Windows"] == "C:\\Textures\\body.tga");
    assert(materials["Empty"].empty());
    assert(materials["Unsupported"].empty());
    assert(materials["Broken"].empty());
    assert(ztp_mtl::tokens("mtllib one.mtl \"two files.mtl\"").size() == 3);
    assert(ztp_mtl::directory("C:\\models\\body.obj") == "C:\\models\\");
    assert(ztp_mtl::resolve("models/", "/textures/body.tga") == "/textures/body.tga");
    std::istringstream replacement("newmtl ANIM_4_Body\nmap_Kd replacement.tga\n");
    ztp_mtl::parse(replacement, "", materials);
    assert(materials["ANIM_4_Body"] == "replacement.tga");
    std::istringstream missing("mtllib missing.mtl\no Body\n");
    std::ostringstream log;
    assert(ztp_mtl::load(missing, "tests/body.obj", log).empty());
    assert(!log.str().empty());
    std::string firstLine;
    std::getline(missing, firstLine);
    assert(firstLine == "mtllib missing.mtl");
    testTriangleDeduplication();
    testImages();
    std::cout << "MTL parser and texture tests passed\n";
}