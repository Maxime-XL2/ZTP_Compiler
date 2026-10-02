extern MATRIX mtx[];

enum
{
    M00, M01, M02, M03,
    M10, M11, M12, M13,
    M20, M21, M22, M23,
   // M30, M31, M32, M33
};

void ztUnitMatrix(unsigned int depth);
void ztPushMatrix(void);
void ztPopMatrix(void);
void ztRotX(float angX, float * m);
void ztRotY(float angY, float * m);
void ztRotZ(float angZ, float * m);
void ztTranslate(float tx, float ty, float tz, float * m);
void ztScale(float sx, float sy, float sz, float * m);
void ztCalc3dPoint(float * pt, float * ans, float * m);
void ztPrintMatrix(void);
