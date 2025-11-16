//
// Image Metrics Module
//
// This module provides implementations of the two most common classical
// image quality metrics used in compression evaluation:
//
// - PSNR (Peak Signal-to-Noise Ratio):
//     A simple error-based metric derived from MSE.
//     Fast to compute and widely used as a baseline.
//     Its correlation with human perception is limited.
//
// - SSIM (Structural Similarity Index):
//     A perceptually-aware metric comparing structure, contrast,
//     and luminance. Much more aligned with human visual perception.
//
//
// Advanced metrics (not yet implemented here) that are typically slower
// but significantly more accurate for modern codecs such as AVIF, WebP,
// JPEG XL, and HEIC:
//
// - MS-SSIM (Multi-Scale SSIM):
//     An improved multi-resolution variant of SSIM.
//     Much stronger predictor of perceived visual quality than PSNR/SSIM.
//     Commonly used in academic papers and in AVIF/JXL evaluations.
//
// - VIF (Visual Information Fidelity):
//     A statistically grounded metric based on information theory.
//     Provides excellent correlation with subjective tests,
//     but is computationally heavier.
//
// - FSIM (Feature Similarity Index):
//     Relies on phase congruency and gradient magnitude.
//     Highly sensitive to edges and structural detail—ideal for
//     comparing sharpening/compression pipelines.
//
// - LPIPS (Learned Perceptual Image Patch Similarity):
//     A modern deep-learning-based metric using VGG/AlexNet features.
//     Very strong agreement with human perception.
//     Computationally expensive; GPU acceleration recommended.
//
// All metrics require reference and test images of the same size and type.
//
// This module is Qt-independent and can be used in GUI applications
// or standalone console tools.
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
