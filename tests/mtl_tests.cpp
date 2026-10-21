#include "../SRC/COMMON.H"
#include "../SRC/ZTP/mtl.h"
#include <cassert>
#include <cstdio>
#include <iostream>

uint32_t specialOption = 1;
uint32_t useUV = 0;
texture_t skin = {};
bool setFbxBonePreviewPointRemap(animated_model_t *, const std::vector<unsigned int> &) { return true; }
void rotatePol(polygon_t * polygon)
{
    const polygon_t original = *polygon;
    for (unsigned int corner = 0; corner < 4; ++corner)
    {
        polygon->vertIdx[corner] = original.vertIdx[(corner + 1) & 3];
        polygon->vertUV[corner] = original.vertUV[(corner + 1) & 3];
    }
}
void textureFlip(polygon_t * polygon)
{
    polygon->SGL_ATTR.dir = (polygon->SGL_ATTR.dir & 0xFFCF) | ((polygon->SGL_ATTR.dir + (1 << 4)) & 0x0030);
}
static bool textureBakeEnabled = false;
void specialConditions(unsigned short first, unsigned short end, animated_model_t * model)
{
    assert(textureBakeEnabled);
    for (unsigned int texture = first; texture < end; ++texture)
        model->texture[texture].SGL_ATTR.texno = texture;
}
extern int ReadTGAFile(std::string folder, texture_t * texture, std::string sourcePath);
extern int checkExists(polygon_t * polygon, animated_model_t * model, const std::string & sourceName);
extern int FillPalette(texture_t * texture, int compression);

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

    faces[0] = {};
    faces[0].texture = 0;
    for (unsigned int corner = 0; corner < 4; ++corner)
        faces[0].vertIdx[corner] = corner;
    faces[0].vertUV[0] = uv_t{0, 0};
    faces[0].vertUV[1] = uv_t{1, 0};
    faces[0].vertUV[2] = uv_t{1, 1};
    faces[0].vertUV[3] = uv_t{0, 1};
    faces[1] = {};
    faces[1].texture = -1;
    for (unsigned int corner = 0; corner < 4; ++corner)
        faces[1].vertIdx[corner] = corner + 10;
    faces[1].vertUV[0] = uv_t{0, 0};
    faces[1].vertUV[1] = uv_t{0, 0};
    faces[1].vertUV[2] = uv_t{1, 0};
    faces[1].vertUV[3] = uv_t{0, 1};
    assert(checkExists(&faces[1], &model, "atlas") == 0);
    assert(faces[1].texture == -1);
}

void testEnabledCompressionAlwaysUsesFourBits()
{
    texture_t texture = {};
    texture.width = 8;
    texture.height = 4;
    texture.pixel = new pixel_t[texture.width * texture.height]();
    for (unsigned int pixel = 0; pixel < texture.width * texture.height; ++pixel)
    {
        const unsigned int shade = pixel;
        texture.pixel[pixel].rgb = C_RGB(shade, shade, shade);
    }

    specialOption |= 1;
    assert(FillPalette(&texture, -1) == 1);
    assert(texture.colorDepth == COL_16);
    for (unsigned int pixel = 0; pixel < texture.width * texture.height; ++pixel)
        assert(texture.pixel[pixel].palette_idx < 16);

    specialOption &= ~1u;
    assert(FillPalette(&texture, -1) == 0);
    assert(texture.colorDepth == COL_32K);
    specialOption |= 1;
    delete [] texture.pixel;
}

void testGeneratedTextureDimensionLimits()
{
    model_t mesh = {};
    polygon_t face = {};
    face.vertIdx[0] = face.vertIdx[1] = face.vertIdx[2] = face.vertIdx[3] = 0;
    face.vertUV[0] = uv_t{0.0f, 0.0f};
    face.vertUV[1] = uv_t{1.0f, 0.0f};
    face.vertUV[2] = uv_t{1.0f, 1.0f};
    face.vertUV[3] = uv_t{0.0f, 1.0f};
    mesh.nbPolygon = 1;
    mesh.pltbl = &face;
    animated_model_t model = {};
    model.nbModels = 1;
    model.model = &mesh;
    model.texture = new texture_t[2048]();
    texture_t source = {};
    source.name = "dimension_limit_source";
    source.width = source.height = 512;
    source.pixel = new pixel_t[512 * 512]();
    for (unsigned int pixel = 0; pixel < 512 * 512; ++pixel)
    {
        source.pixel[pixel].rgb = 0x801F;
        source.pixel[pixel].r = 255;
        source.pixel[pixel].a = 255;
    }

    const uint32_t oldWidthLimit = maxGeneratedTextureWidth;
    const uint32_t oldHeightLimit = maxGeneratedTextureHeight;
    const uint32_t oldSpecialOptions = specialOption;
    const uint32_t oldUseUv = useUV;
    maxGeneratedTextureWidth = 23;
    maxGeneratedTextureHeight = 10;
    useUV = 1;
    textureBakeEnabled = true;
    specialOption = 2;
    createTexture(&face, &model, source);
    assert(model.texture[0].width == 16);
    assert(model.texture[0].height == 8);
    assert(model.texture[0].width % 8 == 0 && model.texture[0].height % 8 == 0);

    maxGeneratedTextureWidth = 31;
    specialOption = 0;
    createTexture(&face, &model, source);
    assert(model.texture[1].width == 24);
    assert(model.texture[1].height == 10);
    assert(model.texture[1].width % 8 == 0 && model.texture[1].height <= 256);

    maxGeneratedTextureWidth = 1024;
    maxGeneratedTextureHeight = 300;
    createTexture(&face, &model, source);
    assert(model.texture[2].width == 512);
    assert(model.texture[2].height == 256);

    textureBakeEnabled = false;
    for (unsigned int texture = 0; texture < model.nbTextures; ++texture)
        delete [] model.texture[texture].pixel;
    delete [] model.texture;
    delete [] source.pixel;
    maxGeneratedTextureWidth = oldWidthLimit;
    maxGeneratedTextureHeight = oldHeightLimit;
    specialOption = oldSpecialOptions;
    useUV = oldUseUv;
}

void testReducedBaseTexturePixels()
{
    const unsigned int columns = 19, rows = 15;
    model_t mesh = {};
    mesh.nbPoint = columns * rows;
    mesh.pntbl = new vertex_t[mesh.nbPoint]();
    mesh.nbPolygon = (columns - 1) * (rows - 1);
    mesh.pltbl = new polygon_t[mesh.nbPolygon]();
    animated_model_t model = {};
    model.nbModels = 1;
    model.model = &mesh;
    model.texture = new texture_t[2048]();
    texture_t atlas = {};
    atlas.name = "seamed_atlas";
    atlas.width = atlas.height = 64;
    atlas.pixel = new pixel_t[64 * 64]();
    for (unsigned int pixel = 0; pixel < 64 * 64; ++pixel)
    {
        atlas.pixel[pixel].r = pixel % 64 < 32 ? 248 : 0;
        atlas.pixel[pixel].g = (pixel / 64 % 8) * 32;
        atlas.pixel[pixel].b = pixel % 64 < 32 ? 0 : 248;
        atlas.pixel[pixel].a = 255;
        atlas.pixel[pixel].rgb = C_RGB(atlas.pixel[pixel].r / 8, atlas.pixel[pixel].g / 8, atlas.pixel[pixel].b / 8);
    }
    for (unsigned int row = 0; row < rows; ++row)
        for (unsigned int column = 0; column < columns; ++column)
        {
            mesh.pntbl[row * columns + column].point[X] = float(column);
            mesh.pntbl[row * columns + column].point[Y] = float(row);
            if (column + 1 == columns || row + 1 == rows) continue;
            polygon_t & face = mesh.pltbl[row * (columns - 1) + column];
            face.vertIdx[0] = row * columns + column;
            face.vertIdx[1] = face.vertIdx[0] + 1;
            face.vertIdx[2] = face.vertIdx[1] + columns;
            face.vertIdx[3] = face.vertIdx[0] + columns;
            for (unsigned int corner = 0; corner < 4; ++corner)
            {
                const float x = float(column + (corner == 1 || corner == 2));
                face.vertUV[corner].U = 0.05f + x * 0.4f / 9.0f + (column >= 9 ? 0.1f : 0.0f);
                face.vertUV[corner].V = 0.05f + float(row + (corner >= 2)) * 0.9f / 14.0f;
            }
        }
    useUV = 1;
    textureBakeEnabled = true;
    cacheAutomaticLodSourceTextures(&model, &atlas, 1);
    extern void createTexture(polygon_t *, animated_model_t *, const texture_t &);
    for (unsigned int face = 0; face < mesh.nbPolygon; ++face)
        createTexture(&mesh.pltbl[face], &model, atlas);
    assert(replaceAutomaticLodBase(&model, 37));
    assert(mesh.nbPoint < columns * rows);
    for (unsigned int face = 0; face < mesh.nbPolygon; ++face)
    {
        const polygon_t & polygon = mesh.pltbl[face];
        float centerX = 0.0f;
        for (unsigned int corner = 0; corner < 4; ++corner)
            centerX += mesh.pntbl[polygon.vertIdx[corner]].point[X] * 0.25f;
        const texture_t & texture = model.texture[polygon.texture];
        assert(texture.pixel && texture.width && texture.height);
        for (unsigned int y = 0; y < texture.height; ++y)
            for (unsigned int x = 0; x < texture.width; ++x)
            {
                const float horizontal = float(x) / texture.width;
                const float vertical = float(y) / texture.height;
                const float topU = polygon.vertUV[0].U * (1.0f - horizontal) + polygon.vertUV[1].U * horizontal;
                const float bottomU = polygon.vertUV[3].U * (1.0f - horizontal) + polygon.vertUV[2].U * horizontal;
                const float topV = polygon.vertUV[0].V * (1.0f - horizontal) + polygon.vertUV[1].V * horizontal;
                const float bottomV = polygon.vertUV[3].V * (1.0f - horizontal) + polygon.vertUV[2].V * horizontal;
                const unsigned int atlasX = MIN(63u, static_cast<unsigned int>((topU * (1.0f - vertical) + bottomU * vertical) * 64));
                const unsigned int atlasY = MIN(63u, static_cast<unsigned int>((topV * (1.0f - vertical) + bottomV * vertical) * 64));
                const pixel_t & actual = texture.pixel[y * texture.width + x];
                assert(actual.r == (centerX < 9.0f ? 248 : 0));
                assert(actual.b == (centerX < 9.0f ? 0 : 248));
                const unsigned int previousY = atlasY ? atlasY - 1 : 0;
                const unsigned int nextY = MIN(63u, atlasY + 1);
                assert(actual.g == atlas.pixel[atlasY * 64 + atlasX].g ||
                    actual.g == atlas.pixel[previousY * 64 + atlasX].g ||
                    actual.g == atlas.pixel[nextY * 64 + atlasX].g);
            }
    }
    assert(restoreAutomaticLodBase(&model));
    discardAutomaticLodSourceTextures(&model);
    for (unsigned int texture = 0; texture < model.nbTextures; ++texture) delete [] model.texture[texture].pixel;
    delete [] model.texture;
    delete [] atlas.pixel;
    delete [] mesh.pntbl;
    delete [] mesh.pltbl;
    delete model.lodData;
    useUV = 0;
    textureBakeEnabled = false;
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
    testEnabledCompressionAlwaysUsesFourBits();
    testGeneratedTextureDimensionLimits();
    testReducedBaseTexturePixels();
    testImages();
    std::cout << "MTL parser and texture tests passed\n";
}