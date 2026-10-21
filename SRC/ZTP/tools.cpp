#include "../COMMON.H"

bool appendAnimationFrameSamples(unsigned int startFrame, unsigned int endFrame,
                                 unsigned int sourceDelta, unsigned int targetDelta,
                                 std::vector<animationFrameSample_t> * samples)
{
    if (!samples || startFrame > endFrame || sourceDelta < 1 || sourceDelta > 60 ||
        targetDelta < 1 || targetDelta > 60)
        return false;
    const uint64_t durationTicks = uint64_t(endFrame - startFrame) * sourceDelta;
    const uint64_t sampleCount = (durationTicks + targetDelta - 1) / targetDelta + 1;
    if (sampleCount > ZTP_MAX_ANIMATION_FRAMES) return false;
    for (uint64_t tick = 0;; tick += targetDelta)
    {
        if (tick >= durationTicks)
        {
            samples->push_back({endFrame, endFrame, 0.0f});
            break;
        }
        const unsigned int wholeFrames = static_cast<unsigned int>(tick / sourceDelta);
        const unsigned int remainder = static_cast<unsigned int>(tick % sourceDelta);
        const unsigned int first = startFrame + wholeFrames;
        samples->push_back({first, MIN(first + 1, endFrame),
            static_cast<float>(remainder) / sourceDelta});
    }
    return true;
}

bool applyAnimationPose(animated_model_t * model, const animationControl_t & control, bool clamp, bool interpolate)
{
    if (!model || !model->keyFrames || !model->nbFrames) return false;
    const unsigned int sourceFrameDelta = MAX(1u, MIN(static_cast<unsigned int>(control.sourceFrameDelta), 60u));
    const unsigned int frameDelta = MAX(1u, MIN(static_cast<unsigned int>(control.frameDelta), 60u));
    unsigned int rangeEnd = MIN(control.endFrm, model->nbFrames);
    if (!rangeEnd) rangeEnd = model->nbFrames;
    const unsigned int startFrame = MIN(static_cast<unsigned int>(control.startFrm), rangeEnd - 1);
    const unsigned int endFrame = MAX(startFrame, rangeEnd - 1);
    const unsigned int durationTicks = (endFrame - startFrame) * sourceFrameDelta;
    const unsigned int sampleCount = (durationTicks + frameDelta - 1) / frameDelta + 1;
    const unsigned int rangeStart = startFrame * sourceFrameDelta;
    const unsigned int elapsed = control.currentFrm >= rangeStart ? control.currentFrm - rangeStart : 0;
    unsigned int outputFrame = elapsed / frameDelta;
    float fraction = static_cast<float>(elapsed % frameDelta) / frameDelta;
    if (outputFrame >= sampleCount)
    {
        outputFrame = sampleCount - 1;
        fraction = 0.0f;
    }
    unsigned int nextOutputFrame = outputFrame + 1;
    if (nextOutputFrame >= sampleCount)
        nextOutputFrame = clamp ? outputFrame : 0;
    const unsigned int leftTick = MIN(outputFrame * frameDelta, durationTicks);
    const unsigned int rightTick = nextOutputFrame == 0 ? 0 :
        MIN(nextOutputFrame * frameDelta, durationTicks);
    const float firstSourcePosition = startFrame +
        static_cast<float>(leftTick) / sourceFrameDelta;
    const float secondSourcePosition = nextOutputFrame == 0 ? static_cast<float>(startFrame) :
        startFrame + static_cast<float>(rightTick) / sourceFrameDelta;
    const unsigned int boneFrame = static_cast<unsigned int>(firstSourcePosition);
    const unsigned int boneNext = MIN(boneFrame + 1, endFrame);
    const float boneFraction = firstSourcePosition - boneFrame;
    if (applyFbxBonePreviewPose(model, boneFrame, boneNext, boneFraction, interpolate)) return true;
    for (unsigned int frame = startFrame; frame <= endFrame; ++frame)
        if (!model->keyFrames[frame].cVert) return false;
    if (!interpolate) fraction = 0.0f;
    const auto samplePoint = [model, startFrame, endFrame](unsigned int offset, unsigned int axis,
                                                          float sourcePosition)
    {
        sourcePosition = MAX(static_cast<float>(startFrame),
            MIN(sourcePosition, static_cast<float>(endFrame)));
        const unsigned int firstFrame = static_cast<unsigned int>(sourcePosition);
        const unsigned int secondFrame = MIN(firstFrame + 1, endFrame);
        const float amount = sourcePosition - firstFrame;
        const float first = model->keyFrames[firstFrame].cVert[offset].point[axis];
        const float second = model->keyFrames[secondFrame].cVert[offset].point[axis];
        return first + (second - first) * amount;
    };
    unsigned int offset = 0;
    for (unsigned int mesh = 0; mesh < model->nbModels; ++mesh)
        for (unsigned int point = 0; point < model->model[mesh].nbPoint; ++point, ++offset)
            for (unsigned int axis = 0; axis < XYZ; ++axis)
            {
                const float first = samplePoint(offset, axis, firstSourcePosition);
                const float second = samplePoint(offset, axis, secondSourcePosition);
                model->model[mesh].pntbl[point].point[axis] = first + (second - first) * fraction;
            }
    return true;
}

uint16_t         swap_endian_ushort(uint16_t value)   { return ((((value) >> 8) & 0xff) | (((value) & 0xff) << 8));}
int16_t          swap_endian_sshort( int16_t value)   { return (((value >> 8)&0xFF) | ((value & 0xFF) << 8));}
uint32_t         swap_endian_uint(  uint32_t value)   { return ((((value) & 0xff000000) >> 24) | (((value) & 0x00ff0000) >>  8) | (((value) & 0x0000ff00) <<  8) | (((value) & 0x000000ff) << 24)); }
int32_t          swap_endian_sint(   int32_t value)   { return ((((value) & 0xff000000) >> 24) | (((value) & 0x00ff0000) >>  8) | (((value) & 0x0000ff00) <<  8) | (((value) & 0x000000ff) << 24)); }

float dotProduct(float * vct1, float * vct2){return ((vct1[X]*vct2[X])+(vct1[Y]*vct2[Y])+(vct1[Z]*vct2[Z]));}

/*
int AddPoint(vPDATA * newMesh, POINT CurrentPoint, VECTOR CurNormVect)
{
    for (unsigned int i=0; i< newMesh->nbPoint; i++)
    {
        if ((newMesh->pntbl[i][0] == CurrentPoint[0])&& (newMesh->pntbl[i][1]== CurrentPoint[1]) && (newMesh->pntbl[i][2]== CurrentPoint[2]))
            {
                return i;
            }
    }

    newMesh->pntbl[newMesh->nbPoint][X] = CurrentPoint[0];
    newMesh->pntbl[newMesh->nbPoint][Y] = CurrentPoint[1];
    newMesh->pntbl[newMesh->nbPoint][Z] = CurrentPoint[2];
    newMesh->vntbl[newMesh->nbPoint][X] = CurNormVect[X];
    newMesh->vntbl[newMesh->nbPoint][Y] = CurNormVect[Y];
    newMesh->vntbl[newMesh->nbPoint][Z] = CurNormVect[Z];
    newMesh->nbPoint ++;
    return (newMesh->nbPoint - 1);
}*/


void calculateBoundingBox(float * boxMin, float * boxMax, vertex_t * vertexArray, unsigned int totalVertices)
{
    vertex_t * v = vertexArray;
    for (unsigned int i=0; i<totalVertices; i++)
    {
        for (unsigned int ii = 0; ii<3; ii++)
        {
            if (v->point[ii] < boxMin[ii])
                boxMin[ii]=v->point[ii];
            if (v->point[ii] > boxMax[ii])
                boxMax[ii]=v->point[ii];
        }
        v++;
    }
}

void writeUint8(ofstream * file, uint8_t data) {
    file->write((char*)&data, sizeof(uint8_t));
}

void writeSint8(ofstream * file, int8_t data) {
    file->write((char*)&data, sizeof(int8_t));
}

void writeUint16(ofstream * file, uint16_t data) {
    uint16_t buf = swap_endian_ushort(data);
    file->write((char*)&buf, sizeof(uint16_t));
}

void writeSint16(ofstream * file, int16_t data) {
    int16_t buf = swap_endian_sshort(data);
    file->write((char*)&buf, sizeof(int16_t));
}

void writeUint32(ofstream * file, uint32_t data) {
    uint32_t buf = swap_endian_uint(data);
    file->write((char*)&buf, sizeof(uint32_t));
}

void writeSint32(ofstream * file, int32_t data) {
    int32_t buf = swap_endian_sint(data);
    file->write((char*)&buf, sizeof(int32_t));
}
