
#pragma once

#include <opencv2/opencv.hpp>
#include <iostream>

using namespace cv;
using namespace std;


int mergePicwithHorizontally(string imgpath1, string imgpath2, string outputPath) {
    cout << "merget output path: " << outputPath << endl;
    Mat img1 = imread(imgpath1);
    Mat img2 = imread(imgpath2);

    if (!img1.data || !img2.data) {
        cout << "Could not load input image file" << endl;
        return -1;
    }

    resize(img1, img1, Size(img1.cols, img1.rows));
    resize(img2, img2, Size(img2.cols, img2.rows));

    cout << "img1 size: " << img1.size() << ", img2 size: " << img2.size() << endl;

    // int rows = max(img1.rows, img2.rows);
    // int cols = img1.cols + img2.cols;

    // // create result image
    // Mat3b res(rows, cols, Vec3b(0, 0, 0));

    // img1.copyTo(res(cv::Rect(0, 0, img1.cols, img1.rows)));
    // img2.copyTo(res(cv::Rect(img1.cols, 0, img2.cols, img2.rows)));

    Mat result;
    hconcat(img1, img2, result);

    vector<int> compress_param;
    compress_param.push_back(CV_IMWRITE_JPEG_QUALITY);
    compress_param.push_back(50);

    cout << "merge size:  " << result.size() << endl;
    if (result.size().width > 0 && result.size().height > 0) {
        imwrite(outputPath, result, compress_param);
    }
    
    return 0;
}