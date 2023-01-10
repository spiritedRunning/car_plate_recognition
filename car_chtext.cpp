#include <wchar.h>
#include <assert.h>
#include <locale.h>
#include <ctype.h>


#include "car_chtext.h"



CvxText::CvxText(const char *freeType)
{
	assert(freeType != NULL);


	if(FT_Init_FreeType(&m_library)) throw;
	if(FT_New_Face(m_library, freeType, 0, &m_face)) throw;


	restoreFont();

	setlocale(LC_ALL, "");
}


CvxText::~CvxText()
{
	FT_Done_Face    (m_face);
	FT_Done_FreeType(m_library);
}

void CvxText::getFont(int *type, CvScalar *size, bool *underline, float *diaphaneity)
{
	if(type) *type = m_fontType;
	if(size) *size = m_fontSize;
	if(underline) *underline = m_fontUnderline;
	if(diaphaneity) *diaphaneity = m_fontDiaphaneity;
}

void CvxText::setFont(int *type, CvScalar *size, bool *underline, float *diaphaneity)
{

	if(type)
	{
		if(type >= 0) m_fontType = *type;
	}
	if(size)
	{
		m_fontSize.val[0] = fabs(size->val[0]);
		m_fontSize.val[1] = fabs(size->val[1]);
		m_fontSize.val[2] = fabs(size->val[2]);
		m_fontSize.val[3] = fabs(size->val[3]);
	}
	if(underline)
	{
		m_fontUnderline   = *underline;
	}
	if(diaphaneity)
	{
		m_fontDiaphaneity = *diaphaneity;
	}

	FT_Set_Pixel_Sizes(m_face, 0, m_fontSize.val[0]);
}


void CvxText::restoreFont()
{
	m_fontType = 0;           

	m_fontSize.val[0] = 24;     
	m_fontSize.val[1] = 0.1;   
	m_fontSize.val[2] = 0.1;  
	m_fontSize.val[3] = 0;      
	m_fontUnderline   = false;   

	m_fontDiaphaneity = 1.0;  


	FT_Set_Pixel_Sizes(m_face, 0, m_fontSize.val[0]);
}

#if 0
int CvxText::putText(cv::Mat &frame, const char    *text, CvPoint pos)
{
	return putText(frame, text, pos, CV_RGB(255,255,255));
}
int CvxText::putText(cv::Mat &frame, const wchar_t *text, CvPoint pos)
{
	return putText(frame, text, pos, CV_RGB(255,255,255));
}

//

int CvxText::putText(cv::Mat &frame, const char *text, CvPoint pos, CvScalar color)
{
	if(frame.empty() == NULL) return -1;
	if(text == NULL) return -1;

	//

	int i;
	 for (i = 0; text[i] != '\0';)
	{
		int wc = text[i];


	/*	if(!isascii(wc)) mbtowc(&wc, &text[i++], 2);*/

		  if (!isascii(wc)) {
				mbtowc(reinterpret_cast<wchar_t *>(&wc), &text[i], 3);
				i += 3;
			} else {
				i++;
			}


		putWChar(frame, wc, pos, color);
	}
	return i;
}
int CvxText::putText(cv::Mat &frame, const wchar_t *text, CvPoint pos, CvScalar color)
{
	if(frame.empty() == NULL) return -1;
	if(text == NULL) return -1;

	//

	int i;
	for(i = 0; text[i] != '\0'; ++i)
	{

		putWChar(frame, text[i], pos, color);
	}
	return i;
}

void CvxText::putWChar(cv::Mat &frame, wchar_t wc, CvPoint &pos, CvScalar color)
{
	IplImage* img=NULL;
	img = &(IplImage)frame;
	//IplImage img = &IplImage(frame);
	//IplImage* img = (IplImage*)&frame;
	//IplImage img = IplImage(frame);
	
	FT_UInt glyph_index = FT_Get_Char_Index(m_face, wc);
	FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT);
	FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_MONO);


	FT_GlyphSlot slot = m_face->glyph;


	int rows = slot->bitmap.rows;
	int cols = slot->bitmap.width;

	//

	for(int i = 0; i < rows; ++i)
	{
		for(int j = 0; j < cols; ++j)
		{
			int off  = ((img->origin==0)? i: (rows-1-i))  \
				* slot->bitmap.pitch + j/8;

			if(slot->bitmap.buffer[off] & (0xC0 >> (j%8)))
			{
				int r = (img->origin==0)? pos.y - (rows-1-i): pos.y + i;;
				int c = pos.x + j;

				if(r >= 0 && r < img->height
					&& c >= 0 && c < img->width)
				{
					CvScalar scalar = cvGet2D(img, r, c);


					float p = m_fontDiaphaneity;
					for(int k = 0; k < 4; ++k)
					{
						scalar.val[k] = scalar.val[k]*(1-p) + color.val[k]*p;
					}

					cvSet2D(img, r, c, scalar);
				}
			}
		} // end for
	} // end for


	double space = m_fontSize.val[0]*m_fontSize.val[1];
	double sep   = m_fontSize.val[0]*m_fontSize.val[2];

	pos.x += (int)((cols? cols: space) + sep);
}
#endif

#if 1
int CvxText::putText(cv::Mat& img, char* text, cv::Point pos)
{
    return putText(img, text, pos, CV_RGB(255, 255, 255));
}

int CvxText::putText(cv::Mat& img, const wchar_t* text, cv::Point pos)
{
    return putText(img, text, pos, CV_RGB(255,255,255));
}

int CvxText::putText(cv::Mat& img, const char* text, cv::Point pos, cv::Scalar color)
{
    if (img.data == nullptr) return -1;
    if (text == nullptr) return -1;

    int i;
    for (i = 0; text[i] != '\0'; ++i) {
        wchar_t wc = text[i];

 
        if(!isascii(wc)) mbtowc(&wc, &text[i++], 2);

  
        putWChar(img, wc, pos, color);
    }

    return i;
}

int CvxText::putText(cv::Mat& img, const wchar_t* text, cv::Point pos, cv::Scalar color)
{
    if (img.data == nullptr) return -1;
    if (text == nullptr) return -1;

    int i;
    for(i = 0; text[i] != '\0'; ++i) {
       
        putWChar(img, text[i], pos, color);
    }

    return i;
}

void CvxText::putWChar(cv::Mat& img, wchar_t wc, cv::Point& pos, cv::Scalar color)
{
    
    FT_UInt glyph_index = FT_Get_Char_Index(m_face, wc);
    FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT);
    FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_MONO);

    FT_GlyphSlot slot = m_face->glyph;


    int rows = slot->bitmap.rows;
    int cols = slot->bitmap.width;

    for (int i = 0; i < rows; ++i) {
        for(int j = 0; j < cols; ++j) {
            int off  = i * slot->bitmap.pitch + j/8;

            if (slot->bitmap.buffer[off] & (0xC0 >> (j%8))) {
                int r = pos.y - (rows-1-i);
                int c = pos.x + j;

                if(r >= 0 && r < img.rows && c >= 0 && c < img.cols) {
                    cv::Vec3b pixel = img.at<cv::Vec3b>(cv::Point(c, r));
                    cv::Scalar scalar = cv::Scalar(pixel.val[0], pixel.val[1], pixel.val[2]);

                    float p = m_fontDiaphaneity;
                    for (int k = 0; k < 4; ++k) {
                        scalar.val[k] = scalar.val[k]*(1-p) + color.val[k]*p;
                    }

                    img.at<cv::Vec3b>(cv::Point(c, r))[0] = (unsigned char)(scalar.val[0]);
                    img.at<cv::Vec3b>(cv::Point(c, r))[1] = (unsigned char)(scalar.val[1]);
                    img.at<cv::Vec3b>(cv::Point(c, r))[2] = (unsigned char)(scalar.val[2]);
                }
            }
        }
    }

    // 修改下一个字的输出位置
    double space = m_fontSize.val[0]*m_fontSize.val[1];
    double sep   = m_fontSize.val[0]*m_fontSize.val[2];

    pos.x += (int)((cols? cols: space) + sep);
}

#endif 


#if 0
int CvxText::putText(IplImage *img, const char    *text, CvPoint pos)
{
	return putText(img, text, pos, CV_RGB(255,255,255));
}
int CvxText::putText(IplImage *img, const wchar_t *text, CvPoint pos)
{
	return putText(img, text, pos, CV_RGB(255,255,255));
}

//

int CvxText::putText(IplImage *img, const char *text, CvPoint pos, CvScalar color)
{
	if(img == NULL) return -1;
	if(text == NULL) return -1;

	//

	int i;
	 for (i = 0; text[i] != '\0';)
	{
		int wc = text[i];


	/*	if(!isascii(wc)) mbtowc(&wc, &text[i++], 2);*/

		  if (!isascii(wc)) {
				mbtowc(reinterpret_cast<wchar_t *>(&wc), &text[i], 3);
				i += 3;
			} else {
				i++;
			}


		putWChar(img, wc, pos, color);
	}
	return i;
}
int CvxText::putText(IplImage *img, const wchar_t *text, CvPoint pos, CvScalar color)
{
	if(img == NULL) return -1;
	if(text == NULL) return -1;

	//

	int i;
	for(i = 0; text[i] != '\0'; ++i)
	{

		putWChar(img, text[i], pos, color);
	}
	return i;
}


void CvxText::putWChar(IplImage *img, wchar_t wc, CvPoint &pos, CvScalar color)
{	
	FT_UInt glyph_index = FT_Get_Char_Index(m_face, wc);
	FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT);
	FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_MONO);


	FT_GlyphSlot slot = m_face->glyph;


	int rows = slot->bitmap.rows;
	int cols = slot->bitmap.width;

	//

	for(int i = 0; i < rows; ++i)
	{
		for(int j = 0; j < cols; ++j)
		{
			int off  = ((img->origin==0)? i: (rows-1-i))  \
				* slot->bitmap.pitch + j/8;

			if(slot->bitmap.buffer[off] & (0xC0 >> (j%8)))
			{
				int r = (img->origin==0)? pos.y - (rows-1-i): pos.y + i;;
				int c = pos.x + j;

				if(r >= 0 && r < img->height
					&& c >= 0 && c < img->width)
				{
					CvScalar scalar = cvGet2D(img, r, c);


					float p = m_fontDiaphaneity;
					for(int k = 0; k < 4; ++k)
					{
						scalar.val[k] = scalar.val[k]*(1-p) + color.val[k]*p;
					}

					cvSet2D(img, r, c, scalar);
				}
			}
		} // end for
	} // end for


	double space = m_fontSize.val[0]*m_fontSize.val[1];
	double sep   = m_fontSize.val[0]*m_fontSize.val[2];

	pos.x += (int)((cols? cols: space) + sep);
}
#endif
