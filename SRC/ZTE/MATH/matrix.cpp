#include "../../COMMON.H"

/**
WORK IN PROGRESS!
NOT TESTED/COMPLETED!
**/

MATRIX mtx[20];
int currentMatrix;

void unitMatrix(float * m)
{
     m[M00]=m[M11]=m[M22]=1.0f;
     m[M01]=m[M02]=m[M03]=
     m[M10]=m[M12]=m[M13]=
     m[M20]=m[M21]=m[M23]=
     0.0;
}

void ztUnitMatrix(unsigned int depth)
{
    currentMatrix= (int)depth;
    float * m = &mtx[currentMatrix][0];
    unitMatrix(m);
}


void ztPushMatrix(void)
{
    if (++currentMatrix > 19) currentMatrix=19;
    float * src = &mtx[currentMatrix-1][0];
    float * dst = &mtx[currentMatrix][0];
    for (unsigned int i=0; i<12; i++)
        dst[i]=src[i];
}

void ztPopMatrix(void)
{
    if (--currentMatrix < 0) currentMatrix=0;
}

void ztRotX(float angX, float * m)
{
    float cosV=std::cos(angX*conv);
    float sinV=std::sin(angX*conv);
    float m01 = m[M01];
    float m02 = m[M02];
    float m11 = m[M11];
    float m12 = m[M12];
    float m21 = m[M21];
    float m22 = m[M22];

    m[M01] = m01*cosV + m02*sinV;
    m[M02] = -(m01*sinV) + m02*cosV;
    m[M11] = m11*cosV+m12*sinV;
    m[M12] = -(m11*sinV)+m12*cosV;
    m[M21] = m21*cosV+m22*sinV;
    m[M22] = -(m21*sinV)+m22*cosV;
}

void ztRotY(float angY, float * m)
{
    float cosV=std::cos(angY*conv);
    float sinV=std::sin(angY*conv);
    float 	m00 = m[M00];
    float 	m02 = m[M02];
    float   m10 = m[M10];
    float   m12 = m[M12];
    float   m20 = m[M20];
    float   m22 = m[M22];

    m[M00]= m00 * cosV - m02 * sinV;
    m[M02]= m00 * sinV + m02 * cosV;
    m[M10]= m10 * cosV - m12 * sinV;
    m[M12]= m10 * sinV + m12 * cosV;
    m[M20]= m20 * cosV - m22 * sinV;
    m[M22]= m20 * sinV + m22 * cosV;

}


/**To be fixed...*/
void ztRotZ(float angZ, float * m)
{
    float cosV=std::cos(angZ*conv);
    float sinV=std::sin(angZ*conv);

	float m00 = m[M00];
	float m01 = m[M01];
	float m10 = m[M10];
	float m11 = m[M11];
	float m20 = m[M20];
	float m21 = m[M21];
	m[M00] = (m00 * cosV) + (m01 * sinV);
	m[M01] = -(m00 * sinV) + (m01 * cosV);
	m[M10] = (m10 * cosV) + (m11 * sinV);
	m[M11] = -(m10 * sinV) + (m11 * cosV);
	m[M20] = (m20 * cosV) + (m21 * sinV);
	m[M21] = -(m20 * sinV) + (m21 * cosV);
}

void ztTranslate(float tx, float ty, float tz, float * m)
{
    float * ptr = m;

    m[M03] += *ptr++ * tx;
    m[M03] += *ptr++ * ty;
    m[M03] += *ptr++ * tz;
    ptr++;

    m[M13] += *ptr++ * tx;
    m[M13] += *ptr++ * ty;
    m[M13] += *ptr++ * tz;
    ptr++;

    m[M23] += *ptr++ * tx;
    m[M23] += *ptr++ * ty;
    m[M23] += *ptr++ * tz;
    ptr++;
}

void ztScale(float sx, float sy, float sz, float * m)
{
    m[M00] *= sx;
    m[M11] *= sy;
    m[M22] *= sz;
}

void ztCalc3dPoint(float * pt, float * ans, float * m)
{
    ans[X] = m[0] * pt[X] + m[1] * pt[Y] + m[ 2] * pt[Z] + m[ 3];
    ans[Y] = m[4] * pt[X] + m[5] * pt[Y] + m[ 6] * pt[Z] + m[ 7];
    ans[Z] = m[8] * pt[X] + m[9] * pt[Y] + m[10] * pt[Z] + m[11];
}



void ztPrintMatrix(void)
{
   /// cout << "\nPrint matrix stack no." << currentMatrix << "\n";
    float * m = &mtx[currentMatrix][0];
    for (unsigned int i=0; i<4; i++)
    {
   ///     cout << (m[0]) << " " << (m[1]) << " " << (m[2]) << " " << m[3] << "\n";
        m+=4;
    }

}
