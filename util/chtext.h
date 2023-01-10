#ifndef _CVXTEXT_H_
#define _CVXTEXT_H_

#include <ft2build.h>
#include FT_FREETYPE_H
#include <wchar.h>
#include "opencv/cv.h"

#include "opencv/highgui.h"
#include "opencv2/opencv.hpp"

#include <opencv2/imgproc/imgproc.hpp>



using namespace cv;

class CvxText  
{

   CvxText& operator=(const CvxText&);


public:


   CvxText(const char *freeType);
   virtual ~CvxText();


   void getFont(int *type,
      CvScalar *size=NULL, bool *underline=NULL, float *diaphaneity=NULL);


   void setFont(int *type,
      CvScalar *size=NULL, bool *underline=NULL, float *diaphaneity=NULL);


   void restoreFont();


#if 0
   int putText(IplImage *img, const char    *text, CvPoint pos);


   int putText(IplImage *img, const wchar_t *text, CvPoint pos);


   int putText(IplImage *img, const char    *text, CvPoint pos, CvScalar color);

   int putText(IplImage *img, const wchar_t *text, CvPoint pos, CvScalar color);


private:


   void putWChar(IplImage *img, wchar_t wc, CvPoint &pos, CvScalar color);
#endif
#if 0
   int putText(cv::Mat &frame, const char    *text, CvPoint pos);


   int putText(cv::Mat &frame, const wchar_t *text, CvPoint pos);


   int putText(cv::Mat &frame, const char    *text, CvPoint pos, CvScalar color);

   int putText(cv::Mat &frame, const wchar_t *text, CvPoint pos, CvScalar color);


private:


   void putWChar(cv::Mat &frame, wchar_t wc, CvPoint& pos, CvPoint color);
#endif

	int putText(cv::Mat& img, char* text, cv::Point pos);


	int putText(cv::Mat& img, const wchar_t* text, cv::Point pos);


	int putText(cv::Mat& img, const char* text, cv::Point pos, cv::Scalar color);


	int putText(cv::Mat& img, const wchar_t* text, cv::Point pos, cv::Scalar color);

	void putWChar(cv::Mat& img, wchar_t wc, cv::Point& pos, cv::Scalar color);

private:

   FT_Library   m_library;   
   FT_Face      m_face;     


   int         m_fontType;
   CvScalar   m_fontSize;
   bool      m_fontUnderline;
   float      m_fontDiaphaneity;

};

#endif 

