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

// auxiliary: mapping quality 0–100 → quantizer 0–63 (lower = better quality)
static int qualityToQuantizer(int quality)
{
    if (quality < 0)   quality = 0;
    if (quality > 100) quality = 100;
    // quality: 0   → q ~= 63  (worst quality)
    // quality: 100 → q = 0    (best quality)
    int q = 63 - (quality * 63 + 50) / 100;
    if (q < 0)  q = 0;
    if (q > 63) q = 63;
    return q;
}

bool imwriteAvif(const std::string& filename,
                 const cv::Mat& image,
                 int quantizer)
{
    if (quantizer < 0)  quantizer = 0;
    if (quantizer > 63) quantizer = 63;
    if (image.empty()) {
        std::cerr << "imwriteAvif: empty image\n";
        return false;
    }

    // We must have 8-bit RGB(A). If CV_8UC3, we treat it as BGR.
    cv::Mat src8;
    if (image.depth() != CV_8U) {
        image.convertTo(src8, CV_8U, 255.0);
    } else {
        src8 = image;
    }

    cv::Mat rgb;
    if (src8.channels() == 3) {
        cv::cvtColor(src8, rgb, cv::COLOR_BGR2RGB);
    } else if (src8.channels() == 4) {
        cv::cvtColor(src8, rgb, cv::COLOR_BGRA2RGB);
    } else if (src8.channels() == 1) {
        // obraz 1-kanałowy – powielamy do RGB
        cv::cvtColor(src8, rgb, cv::COLOR_GRAY2RGB);
    } else {
        std::cerr << "imwriteAvif: unsupported channel count: "
                  << src8.channels() << "\n";
        return false;
    }

    int width  = rgb.cols;
    int height = rgb.rows;

    avifImage* imageAvif = avifImageCreate(width, height, 8, AVIF_PIXEL_FORMAT_YUV444);
    if (!imageAvif) {
        std::cerr << "imwriteAvif: avifImageCreate failed\n";
        return false;
    }

    // Przygotuj RGB wrapper
    avifRGBImage rgbAvif;
    avifRGBImageSetDefaults(&rgbAvif, imageAvif);
    rgbAvif.depth  = 8;
    rgbAvif.format = AVIF_RGB_FORMAT_RGB;
    rgbAvif.chromaUpsampling = AVIF_CHROMA_UPSAMPLING_AUTOMATIC;

    avifRGBImageAllocatePixels(&rgbAvif);

    // Copy data from cv::Mat to rgbAvif.pixels
    // Assumption: rgb is contiguous (if not, make a copy)
    cv::Mat rgbCont;
    if (rgb.isContinuous()) {
        rgbCont = rgb;
    } else {
        rgbCont = rgb.clone();
    }

    const int srcStride = static_cast<int>(rgbCont.step[0]);
    const uint8_t* srcData = rgbCont.ptr<uint8_t>(0);

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = srcData + y * srcStride;
        uint8_t* dstRow = rgbAvif.pixels + y * rgbAvif.rowBytes;
        std::memcpy(dstRow, srcRow, width * 3);
    }

    avifResult r = avifImageRGBToYUV(imageAvif, &rgbAvif);
    if (r != AVIF_RESULT_OK) {
        std::cerr << "imwriteAvif: avifImageRGBToYUV failed: "
                  << avifResultToString(r) << "\n";
        avifRGBImageFreePixels(&rgbAvif);
        avifImageDestroy(imageAvif);
        return false;
    }

    avifEncoder* encoder = avifEncoderCreate();
    if (!encoder) {
        std::cerr << "imwriteAvif: avifEncoderCreate failed\n";
        avifRGBImageFreePixels(&rgbAvif);
        avifImageDestroy(imageAvif);
        return false;
    }

    encoder->minQuantizer = encoder->maxQuantizer = quantizer;
    // You can select a speed between 0 and 10; 0 = slowest, best quality
    encoder->speed = 6; // default balance

    avifRWData output = AVIF_DATA_EMPTY;
    r = avifEncoderWrite(encoder, imageAvif, &output);
    if (r != AVIF_RESULT_OK) {
        std::cerr << "imwriteAvif: avifEncoderWrite failed: "
                  << avifResultToString(r) << "\n";
        avifRWDataFree(&output);
        avifEncoderDestroy(encoder);
        avifRGBImageFreePixels(&rgbAvif);
        avifImageDestroy(imageAvif);
        return false;
    }

    // Save to file
    FILE* f = std::fopen(filename.c_str(), "wb");
    if (!f) {
        std::cerr << "imwriteAvif: cannot open file for write: "
                  << filename << "\n";
        avifRWDataFree(&output);
        avifEncoderDestroy(encoder);
        avifRGBImageFreePixels(&rgbAvif);
        avifImageDestroy(imageAvif);
        return false;
    }

    size_t written = std::fwrite(output.data, 1, output.size, f);
    std::fclose(f);

    bool ok = (written == output.size);
    if (!ok) {
        std::cerr << "imwriteAvif: fwrite incomplete\n";
    }

    avifRWDataFree(&output);
    avifEncoderDestroy(encoder);
    avifRGBImageFreePixels(&rgbAvif);
    avifImageDestroy(imageAvif);

    return ok;
}
