#include "../COMMON.H"
#include <new>
#include <algorithm>
#include <cctype>
#include <limits>
#include <vector>
    
uint32_t maxGeneratedTextureWidth = 512;
uint32_t maxGeneratedTextureHeight = 256;
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
struct PaletteSample
{
    uint16_t color;
    uint64_t weight;
};

struct PaletteBox
{
    std::vector<size_t> samples;
};

struct PaletteCentroid
{
    double channel[3];
};

static unsigned int paletteChannel(uint16_t color, unsigned int channel)
{
    return (color >> (channel * 5)) & 0x1F;
}

static uint16_t encodePaletteColor(const PaletteCentroid & centroid)
{
    const unsigned int red = static_cast<unsigned int>(MAX(0, MIN(31, static_cast<int>(centroid.channel[0] + 0.5))));
    const unsigned int green = static_cast<unsigned int>(MAX(0, MIN(31, static_cast<int>(centroid.channel[1] + 0.5))));
    const unsigned int blue = static_cast<unsigned int>(MAX(0, MIN(31, static_cast<int>(centroid.channel[2] + 0.5))));
    return static_cast<uint16_t>(0x8000 | red | (green << 5) | (blue << 10));
}

static PaletteCentroid paletteBoxCentroid(const PaletteBox & box, const std::vector<PaletteSample> & samples)
{
    PaletteCentroid centroid = {};
    uint64_t totalWeight = 0;
    for (size_t sampleIndex : box.samples)
    {
        const PaletteSample & sample = samples[sampleIndex];
        totalWeight += sample.weight;
        for (unsigned int channel = 0; channel < 3; ++channel)
            centroid.channel[channel] += paletteChannel(sample.color, channel) * static_cast<double>(sample.weight);
    }
    if (totalWeight)
        for (double & channel : centroid.channel) channel /= totalWeight;
    return centroid;
}

static std::vector<PaletteCentroid> buildPalette(const std::vector<PaletteSample> & samples, unsigned int colorLimit)
{
    std::vector<PaletteBox> boxes(1);
    boxes[0].samples.resize(samples.size());
    for (size_t sample = 0; sample < samples.size(); ++sample) boxes[0].samples[sample] = sample;

    while (boxes.size() < colorLimit)
    {
        size_t selectedBox = boxes.size();
        unsigned int selectedChannel = 0;
        double selectedScore = -1.0;
        for (size_t boxIndex = 0; boxIndex < boxes.size(); ++boxIndex)
        {
            const PaletteBox & box = boxes[boxIndex];
            if (box.samples.size() < 2) continue;
            uint64_t totalWeight = 0;
            for (size_t sampleIndex : box.samples) totalWeight += samples[sampleIndex].weight;
            for (unsigned int channel = 0; channel < 3; ++channel)
            {
                unsigned int minimum = 31, maximum = 0;
                for (size_t sampleIndex : box.samples)
                {
                    const unsigned int value = paletteChannel(samples[sampleIndex].color, channel);
                    minimum = MIN(minimum, value);
                    maximum = MAX(maximum, value);
                }
                const double score = (maximum - minimum) * std::sqrt(static_cast<double>(totalWeight));
                if (score > selectedScore)
                {
                    selectedScore = score;
                    selectedBox = boxIndex;
                    selectedChannel = channel;
                }
            }
        }
        if (selectedBox == boxes.size()) break;

        PaletteBox & box = boxes[selectedBox];
        std::sort(box.samples.begin(), box.samples.end(), [&](size_t left, size_t right)
        {
            const unsigned int leftValue = paletteChannel(samples[left].color, selectedChannel);
            const unsigned int rightValue = paletteChannel(samples[right].color, selectedChannel);
            return leftValue != rightValue ? leftValue < rightValue : samples[left].color < samples[right].color;
        });
        uint64_t totalWeight = 0;
        for (size_t sampleIndex : box.samples) totalWeight += samples[sampleIndex].weight;
        uint64_t accumulatedWeight = 0;
        size_t split = 1;
        for (; split < box.samples.size(); ++split)
        {
            accumulatedWeight += samples[box.samples[split - 1]].weight;
            if (accumulatedWeight * 2 >= totalWeight) break;
        }
        split = MIN(split, box.samples.size() - 1);
        PaletteBox secondBox;
        secondBox.samples.assign(box.samples.begin() + split, box.samples.end());
        box.samples.erase(box.samples.begin() + split, box.samples.end());
        boxes.push_back(std::move(secondBox));
    }

    std::vector<PaletteCentroid> centroids;
    for (const PaletteBox & box : boxes) centroids.push_back(paletteBoxCentroid(box, samples));

    for (unsigned int iteration = 0; iteration < 8; ++iteration)
    {
        std::vector<PaletteCentroid> sums(centroids.size());
        std::vector<uint64_t> weights(centroids.size(), 0);
        for (const PaletteSample & sample : samples)
        {
            size_t best = 0;
            double bestDistance = std::numeric_limits<double>::max();
            for (size_t candidate = 0; candidate < centroids.size(); ++candidate)
            {
                double distance = 0.0;
                for (unsigned int channel = 0; channel < 3; ++channel)
                {
                    const double difference = paletteChannel(sample.color, channel) - centroids[candidate].channel[channel];
                    distance += difference * difference;
                }
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = candidate;
                }
            }
            weights[best] += sample.weight;
            for (unsigned int channel = 0; channel < 3; ++channel)
                sums[best].channel[channel] += paletteChannel(sample.color, channel) * static_cast<double>(sample.weight);
        }
        for (size_t candidate = 0; candidate < centroids.size(); ++candidate)
            if (weights[candidate])
                for (unsigned int channel = 0; channel < 3; ++channel)
                    centroids[candidate].channel[channel] = sums[candidate].channel[channel] / weights[candidate];
    }
    return centroids;
}

/**This function fills the current image's palette in a 16 colors palette**/
int FillPalette(texture_t * texture, int compression)
{
   /** if (texture->width*texture->height<16){
        texture->colorDepth = COL_32K;
        return 0;
    }*/
    if (!(specialOption&1)) {
        if (!(specialOption&1)) cout << "Texture no. " << texture->textureId << " set to 16 bpp\n";
        texture->colorDepth = COL_32K;
        return 0;
    }
    (void)compression;
    std::fill(texture->clut, texture->clut + 16, 0);
    const size_t pixelCount = static_cast<size_t>(texture->width) * texture->height;
    std::vector<uint64_t> histogram(65536, 0);
    bool hasTransparency = false;
    for (size_t pixel = 0; pixel < pixelCount; ++pixel)
    {
        const uint16_t color = texture->pixel[pixel].rgb;
        if (color == 0) hasTransparency = true;
        else ++histogram[color];
    }

    std::vector<PaletteSample> samples;
    for (size_t color = 1; color < histogram.size(); ++color)
        if (histogram[color]) samples.push_back(PaletteSample{static_cast<uint16_t>(color), histogram[color]});

    const unsigned int firstColorIndex = hasTransparency ? 1 : 0;
    const unsigned int colorLimit = 16 - firstColorIndex;
    std::vector<uint16_t> paletteColors;
    if (samples.size() <= colorLimit)
    {
        for (const PaletteSample & sample : samples) paletteColors.push_back(sample.color);
    }
    else
    {
        const std::vector<PaletteCentroid> centroids = buildPalette(samples, colorLimit);
        for (const PaletteCentroid & centroid : centroids) paletteColors.push_back(encodePaletteColor(centroid));
    }

    if (hasTransparency) texture->clut[0] = 0;
    for (size_t color = 0; color < paletteColors.size(); ++color)
        texture->clut[firstColorIndex + color] = paletteColors[color];

    std::vector<uint8_t> colorPaletteIndex(65536, 0xFF);
    if (hasTransparency) colorPaletteIndex[0] = 0;
    for (const PaletteSample & sample : samples)
    {
        size_t best = 0;
        unsigned int bestDistance = UINT_MAX;
        for (size_t candidate = 0; candidate < paletteColors.size(); ++candidate)
        {
            unsigned int distance = 0;
            for (unsigned int channel = 0; channel < 3; ++channel)
            {
                const int difference = static_cast<int>(paletteChannel(sample.color, channel)) -
                    static_cast<int>(paletteChannel(paletteColors[candidate], channel));
                distance += difference * difference;
            }
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = candidate;
            }
        }
        colorPaletteIndex[sample.color] = static_cast<uint8_t>(firstColorIndex + best);
    }
    for (size_t pixel = 0; pixel < pixelCount; ++pixel)
        texture->pixel[pixel].palette_idx = colorPaletteIndex[texture->pixel[pixel].rgb];

    texture->colorDepth = COL_16;
    cout << "The image contains a total of " << paletteColors.size() + firstColorIndex
         << " colors after " << (samples.size() > colorLimit ? "quantization" : "deduplication") << ".\n";
    return 1;

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
int ReadTGAFile (string folder, texture_t * texture, string sourcePath = "")
{
    const auto fallback = [&]() -> int
    {
        if (!sourcePath.empty())
        {
            cout << "WARNING: Cannot load MTL texture " << sourcePath << "; trying legacy texture " << texture->name << ".TGA\n";
            return ReadTGAFile(folder, texture);
        }
        createFakeTexture(texture);
        return -1;
    };
    if (!sourcePath.empty())
    {
        const std::size_t dot = sourcePath.find_last_of('.');
        string extension = dot == string::npos ? "" : sourcePath.substr(dot);
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        if (extension == ".png")
        {
            std::vector<unsigned char> image;
            unsigned int width = 0, height = 0;
            const unsigned int error = lodepng::decode(image, width, height, sourcePath);
            if (error || width == 0 || height == 0) return fallback();
            texture->width = width;
            texture->height = height;
            texture->pixel = new pixel_t[width * height]();
            for (unsigned int row = 0; row < height; ++row)
                for (unsigned int column = 0; column < width; ++column)
                {
                    const unsigned int sourceRow = useUV ? height - 1 - row : row;
                    const unsigned int source = (sourceRow * width + column) * 4;
                    pixel_t & pixel = texture->pixel[row * width + column];
                    pixel.r = image[source];
                    pixel.g = image[source + 1];
                    pixel.b = image[source + 2];
                    pixel.a = image[source + 3];
                    pixel.rgb = pixel.a < 8 ? 0 : C_RGB(pixel.r / 8, pixel.g / 8, pixel.b / 8);
                }
            FillPalette(texture, -1);
            textureSpecialAttributes(texture);
            cout << "MTL texture " << sourcePath << ", size: " << width << "x" << height << "\n";
            return 1;
        }
        if (extension != ".tga") return fallback();
    }

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
    name = sourcePath.empty() ? folder + texture->name + ".TGA" : sourcePath;
    TGA_HEADER header = {};
    ifstream ibinfile(name.c_str(), ios::in | ios::binary);

    if (!ibinfile.is_open())
        {
            cout << "ERROR : COULDN'T LOAD FILE " << name << "\n";
            return fallback();
        }

    if (!ReadTGAHeader(&ibinfile, &header) || header.width <= 0 || header.height <= 0 ||
        header.colourmaptype != 0 || (header.bitsperpixel != 24 && header.bitsperpixel != 32))
    {
        cout << "ERROR : INVALID OR UNSUPPORTED TGA HEADER " << name << "\n";
        return fallback();
    }
        cout << "Texture " << texture->name << ", size : " << header.width << "x" << header.height << ", pixel depth : " << header.bitsperpixel << ", RLE = " << header.datatypecode << "\n";

    if (header.datatypecode!=2) {cout << "RLE currently not supported\n"; return fallback();}
    texture->width = header.width;
    texture->height = header.height;
    texture->pixel = new pixel_t[header.height*header.width]();

    ibinfile.ignore(header.idlength);
    if (!ReadTGAData(&ibinfile, texture, header.bitsperpixel))
    {
        cout << "ERROR : INCOMPLETE TGA PIXEL DATA " << name << "\n";
        delete [] texture->pixel;
        texture->pixel = NULL;
        return fallback();
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


#define uvTol (1.0f/256.0f)
void rotatePol(polygon_t * pol);
void textureFlip(polygon_t * pol);
static bool reuseFaceTexture(polygon_t * polygon, const polygon_t * reference)
{
    for (unsigned int rotation = 0; rotation < 4; ++rotation)
    {
        for (unsigned int reflected = 0; reflected < 2; ++reflected)
        {
            bool matches = true;
            for (unsigned int corner = 0; corner < 4 && matches; ++corner)
            {
                const unsigned int sourceCorner = (corner + rotation) & 3;
                const unsigned int referenceCorner = corner ^ reflected;
                if (fabs(polygon->vertUV[sourceCorner].U - reference->vertUV[referenceCorner].U) > uvTol ||
                    fabs(polygon->vertUV[sourceCorner].V - reference->vertUV[referenceCorner].V) > uvTol)
                    matches = false;
                for (unsigned int other = 0; other < corner && matches; ++other)
                {
                    if ((polygon->vertIdx[sourceCorner] == polygon->vertIdx[(other + rotation) & 3]) !=
                        (reference->vertIdx[referenceCorner] == reference->vertIdx[other ^ reflected]))
                        matches = false;
                }
            }
            if (!matches) continue;

            const polygon_t original = *polygon;
            for (unsigned int corner = 0; corner < 4; ++corner)
            {
                polygon->vertIdx[corner] = original.vertIdx[(corner + rotation) & 3];
                polygon->vertUV[corner] = original.vertUV[(corner + rotation) & 3];
            }
            polygon->texture = reference->texture;
            polygon->SGL_ATTR = reference->SGL_ATTR;
            polygon->SGL_ATTR.dir ^= reflected << 4;
            return true;
        }
    }
    return false;
}

int checkExists (polygon_t * pol, animated_model_t * aModel, const string & sourceName){
    for (int m=0; m<aModel->nbModels; m++){
       polygon_t * newPol = &aModel->model[m].pltbl[0];
       for (int i=0; i<aModel->model[m].nbPolygon; i++, newPol++){
            if (newPol==pol) return 0;
            if (newPol->texture < 0 || newPol->texture >= static_cast<int>(aModel->nbTextures) ||
                aModel->texture[newPol->texture].name != sourceName) continue;
            if (reuseFaceTexture(pol, newPol)) return 1;

       }
    }

    return 0;
}

void  createTexture( polygon_t * pol, animated_model_t * aModel ) {
    createTexture(pol, aModel, skin);
}

static bool createTextureInternal(polygon_t * pol, animated_model_t * aModel,
                                  const texture_t & sourceTexture, bool reuseExisting,
                                  unsigned int maxDimension)
{
    if (!useUV || !pol || !aModel || !aModel->texture || !sourceTexture.pixel ||
        !sourceTexture.width || !sourceTexture.height || aModel->nbTextures >= 2048) return false;
    if (reuseExisting && checkExists(pol, aModel, sourceTexture.name)) return true;

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


    const unsigned int naturalWidth = MAX(((int)(sourceTexture.width * (bbMax[0] - bbMin[0]) + 7) / 8) << 3, 8);
    const unsigned int naturalHeight = specialOption & (1 << 1) ?
        MAX(((int)(sourceTexture.height * (bbMax[1] - bbMin[1]) + 4) / 8) << 3, 8) :
        MAX(static_cast<unsigned int>(sourceTexture.height * (bbMax[1] - bbMin[1])), 1u);
    unsigned int targetWidth = 0;
    unsigned int targetHeight = 0;
    if (maxDimension)
    {
        const unsigned int naturalMaxDimension = MAX(naturalWidth, naturalHeight);
        const double scale = MIN(1.0, static_cast<double>(maxDimension) / naturalMaxDimension);
        targetWidth = MAX(8u, MIN(naturalWidth,
            (static_cast<unsigned int>(std::lround(naturalWidth * scale)) + 7u) & ~7u));
        targetHeight = specialOption & (1 << 1) ?
            MAX(8u, MIN(naturalHeight,
                (static_cast<unsigned int>(std::lround(naturalHeight * scale)) + 7u) & ~7u)) :
            MAX(1u, MIN(naturalHeight, static_cast<unsigned int>(std::lround(naturalHeight * scale))));
    }
    else
    {
        const unsigned int widthLimit = MAX(8u, (maxGeneratedTextureWidth / 8) * 8);
        targetWidth = MIN(naturalWidth, widthLimit);
        const unsigned int heightLimit = MIN(maxGeneratedTextureHeight, 256u);
        if (specialOption & (1 << 1))
        {
            const unsigned int alignedHeightLimit = MAX(8u, (heightLimit / 8) * 8);
            targetHeight = MIN(naturalHeight, alignedHeightLimit);
        }
        else
            targetHeight = MIN(naturalHeight, MAX(heightLimit, 1u));
    }
    const size_t pixelCount = static_cast<size_t>(targetWidth) * targetHeight;
    pixel_t * resizedPixels = new (std::nothrow) pixel_t[pixelCount]();
    if (!resizedPixels) return false;

    const unsigned int textureIndex = aModel->nbTextures;
    texture_t * text = &aModel->texture[textureIndex];
    pol->texture = static_cast<int>(textureIndex);
    text->textureId = textureIndex;
    text->name = sourceTexture.name;
    text->width = targetWidth;
    text->height = targetHeight;
    text->pixel = resizedPixels;

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
            text->pixel[j].palette_idx = sourcePixel.palette_idx;
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

    if ((specialOption & 1) && sourceTexture.colorDepth == COL_16)
    {
        for (unsigned int color = 0; color < 16; ++color)
            text->clut[color] = sourceTexture.clut[color];
        text->colorDepth = COL_16;
    }
    else
        FillPalette(text, -1);
    textureSpecialAttributes(text);

    cout << "\nTEST..." << aModel->nbTextures << "\n";
    aModel->nbTextures++;

    specialConditions(aModel->nbTextures-1, aModel->nbTextures, aModel);
    pol->SGL_ATTR = aModel->texture[pol->texture].SGL_ATTR;

    ///delete [] bufText->pixel;
    ///delete bufText;
    return true;
}

void createTexture(polygon_t * pol, animated_model_t * aModel, const texture_t & sourceTexture)
{
    createTextureInternal(pol, aModel, sourceTexture, true, 0);
}

bool createTextureAtMaxDimension(polygon_t * polygon, animated_model_t * model,
                                 const texture_t & sourceTexture, unsigned int maxDimension)
{
    if (maxDimension < 8 || maxDimension > 256 || (maxDimension & 7)) return false;
    return createTextureInternal(polygon, model, sourceTexture, false, maxDimension);
}

void destroySkinVariantModel(animated_model_t * model)
{
    if (!model) return;
    if (model->model)
        for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
            delete [] model->model[mesh].pltbl;
    if (model->texture)
        for (unsigned int texture = 0; texture < model->nbTextures; ++texture)
            delete [] model->texture[texture].pixel;
    delete [] model->model;
    delete [] model->texture;
    delete model;
}

animated_model_t * createSkinVariantModel(const animated_model_t * source, const texture_t & atlas,
                                          string * error)
{
    if (!source || !source->model || !source->nbModels || !atlas.pixel ||
        !atlas.width || !atlas.height || !useUV)
    {
        if (error) *error = "Skin variants require a loaded mesh with UVs and a valid atlas.";
        return NULL;
    }

    animated_model_t * variant = new (std::nothrow) animated_model_t();
    if (!variant)
    {
        if (error) *error = "There is not enough memory to create the skin model.";
        return NULL;
    }
    *variant = *source;
    variant->model = NULL;
    variant->texture = NULL;
    variant->nbTextures = 0;
    variant->model = new (std::nothrow) model_t[source->nbModels]();
    variant->texture = new (std::nothrow) texture_t[2048]();
    if (!variant->model || !variant->texture)
    {
        if (error) *error = "There is not enough memory to allocate the skin model.";
        destroySkinVariantModel(variant);
        return NULL;
    }

    for (unsigned int mesh = 0; mesh < source->nbModels; ++mesh)
    {
        variant->model[mesh] = source->model[mesh];
        variant->model[mesh].pltbl = NULL;
        if (source->model[mesh].nbPolygon)
        {
            variant->model[mesh].pltbl =
                new (std::nothrow) polygon_t[source->model[mesh].nbPolygon];
            if (!variant->model[mesh].pltbl)
            {
                if (error) *error = "There is not enough memory to copy the skin model polygons.";
                destroySkinVariantModel(variant);
                return NULL;
            }
            std::copy(source->model[mesh].pltbl,
                      source->model[mesh].pltbl + source->model[mesh].nbPolygon,
                      variant->model[mesh].pltbl);
        }
    }

    for (unsigned int mesh = 0; mesh < variant->nbModels; ++mesh)
        for (unsigned int face = 0; face < variant->model[mesh].nbPolygon; ++face)
        {
            polygon_t * polygon = &variant->model[mesh].pltbl[face];
            const ATTR surfaceAttributes = source->model[mesh].pltbl[face].SGL_ATTR;
            if (!createTextureInternal(polygon, variant, atlas, true, 0))
            {
                if (error) *error = "Could not generate a skin texture for every UV-mapped surface.";
                destroySkinVariantModel(variant);
                return NULL;
            }
            const ATTR textureAttributes = polygon->SGL_ATTR;
            polygon->SGL_ATTR = surfaceAttributes;
            polygon->SGL_ATTR.texno = textureAttributes.texno;
            polygon->SGL_ATTR.colno = textureAttributes.colno;
            polygon->SGL_ATTR.dir = textureAttributes.dir;
        }

    if (!variant->nbTextures)
    {
        if (error) *error = "The model has no textured surfaces to generate for this skin.";
        destroySkinVariantModel(variant);
        return NULL;
    }
    return variant;
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
