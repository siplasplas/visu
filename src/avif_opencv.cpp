#include "avif_opencv.h"

#include <avif/avif.h>
#include <opencv2/imgproc.hpp>
#include <iostream>

// auxiliary: conversion from avifRGBImage to cv::Mat (CV_8UC4)
static cv::Mat avifRGBToMat(const avifRGBImage& rgb)
{
    if (!rgb.pixels || rgb.rowBytes == 0)
        return {};

    int width  = rgb.width;
    int height = rgb.height;

    // Let's assume 8-bit RGBA
    cv::Mat mat(height, width, CV_8UC4, rgb.pixels, rgb.rowBytes);
    return mat.clone(); // own copy – we do not depend on the libavif buffer
}

cv::Mat imreadAvif(const std::string& filename, int flags)
{
    cv::Mat result;

    avifDecoder* decoder = avifDecoderCreate();
    if (!decoder) {
        std::cerr << "imreadAvif: avifDecoderCreate() failed\n";
        return result;
    }

    // You can set the decoder parameters, e.g., threads:
    decoder->maxThreads = 4;

    avifResult r = avifDecoderSetIOFile(decoder, filename.c_str());
    if (r != AVIF_RESULT_OK) {
        std::cerr << "imreadAvif: avifDecoderSetIOFile failed: "
                  << avifResultToString(r) << "\n";
        avifDecoderDestroy(decoder);
        return result;
    }

    r = avifDecoderParse(decoder);
    if (r != AVIF_RESULT_OK) {
        std::cerr << "imreadAvif: avifDecoderParse failed: "
                  << avifResultToString(r) << "\n";
        avifDecoderDestroy(decoder);
        return result;
    }

    // We decode the first frame (for static AVIF, this is the only image)
    r = avifDecoderNextImage(decoder);
    if (r != AVIF_RESULT_OK) {
        std::cerr << "imreadAvif: avifDecoderNextImage failed: "
                  << avifResultToString(r) << "\n";
        avifDecoderDestroy(decoder);
        return result;
    }

    avifImage* image = decoder->image;
    if (!image) {
        std::cerr << "imreadAvif: decoder->image is null\n";
        avifDecoderDestroy(decoder);
        return result;
    }

    // We prepare the RGB(A) buffer
    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, image);
    rgb.depth  = 8;
    rgb.chromaUpsampling = AVIF_CHROMA_UPSAMPLING_AUTOMATIC;
    rgb.format = AVIF_RGB_FORMAT_RGBA; // always RGBA, then we convert it in OpenCV

    avifRGBImageAllocatePixels(&rgb);

    r = avifImageYUVToRGB(image, &rgb);
    if (r != AVIF_RESULT_OK) {
        std::cerr << "imreadAvif: avifImageYUVToRGB failed: "
                  << avifResultToString(r) << "\n";
        avifRGBImageFreePixels(&rgb);
        avifDecoderDestroy(decoder);
        return result;
    }

    // rgb.pixels -> cv::Mat (CV_8UC4)
    cv::Mat rgba = avifRGBToMat(rgb);

    // Sprzątanie libavif
    avifRGBImageFreePixels(&rgb);
    avifDecoderDestroy(decoder);

    if (rgba.empty())
        return result;

    // Dopasowanie do flag OpenCV
    switch (flags) {
    case cv::IMREAD_GRAYSCALE: {
        cv::Mat gray;
        cv::cvtColor(rgba, gray, cv::COLOR_RGBA2GRAY);
        result = gray;
        break;
    }
    case cv::IMREAD_UNCHANGED: {
        // Return RGBA (CV_8UC4)
        result = rgba;
        break;
    }
    case cv::IMREAD_COLOR:
    default: {
        cv::Mat bgr;
        cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);
        result = bgr;
        break;
    }
    }

    return result;
}
