//
// Image Metrics Module
//
// This module provides two objective image quality metrics commonly used
// in image compression research and evaluation:
//
// - PSNR (Peak Signal-to-Noise Ratio):
//     A simple, classical pixel-wise metric based on the mean squared error.
//     Easy to compute, widely used for baseline comparisons.
//     High PSNR generally indicates low distortion, but the metric does not
//     correlate well with human visual perception.
//
// - SSIM (Structural Similarity Index):
//     A significantly more perceptually meaningful metric that compares
//     luminance, contrast and structural information between images.
//     SSIM typically correlates better with human perception than PSNR and is
//     considered a stronger indicator of visual quality.
//
// Both metrics require the reference image and test image to have identical
// dimensions and pixel formats. They operate on cv::Mat and do not depend
// on Qt, making the module suitable for GUI applications and standalone
// console tools.
//

#ifndef IMAGE_METRICS_H
#define IMAGE_METRICS_H

#include <opencv2/core.hpp>

// PSNR (Peak Signal-to-Noise Ratio), w dB.
// Zwraca +inf, jeśli obrazy są identyczne.
double computePsnr(const cv::Mat& ref, const cv::Mat& test);

// SSIM (Structural Similarity Index), jednoskalowe, 0..1.
// Obrazy mogą być szare albo BGR; jeśli BGR, metryka liczona jest na konwersji do gray.
double computeSsim(const cv::Mat& ref, const cv::Mat& test);

#endif // IMAGE_METRICS_H
