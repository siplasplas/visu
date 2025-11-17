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

#include <string>
#include <vector>
#include <opencv2/core.hpp>

enum class MetricType {
    None,
    PSNR,
    SSIM,
    MS_SSIM,
    FSIM,
};

struct MetricDef {
    MetricType    type;
    const char*   id;         // stable id, e.g. "psnr"
    const char*   label;      // short UI label, e.g. "PSNR"
    const char*   description;// short human-readable description
};

// List of all metrics supported by this module (except None).
// Order defines default UI order.
const std::vector<MetricDef>& getAvailableMetrics();

// Core metric implementations:
// PSNR (Peak Signal-to-Noise Ratio), in dB.
// Returns +inf if the images are identical.
double computePsnr(const cv::Mat& ref, const cv::Mat& test);

// SSIM (Structural Similarity Index), single-scale, 0..1.
// Images can be grayscale or BGR; if BGR, the metric is calculated on conversion to grayscale.
double computeSsim(const cv::Mat& ref, const cv::Mat& test);

// MS-SSIM (Multi-Scale Structural Similarity Index), 0..1.
// Uses a classical 5-scale formulation with Wang et al. weights by default.
// 'levels' should be in [1,5]; values >5 are clamped to 5.
double computeMsSsim(const cv::Mat& ref, const cv::Mat& test, int levels = 5);

// FSIM-like feature similarity index, 0..1.
// This is a lightweight approximation of the original FSIM metric:
// it uses gradient magnitude and local phase (from Sobel derivatives)
// instead of full phase congruency maps. It is less expensive than a
// full FSIM implementation but still more discriminative than pure SSIM
// for structural and edge differences.
double computeFsim(const cv::Mat& ref, const cv::Mat& test);

// Generic metric compute function for UI:
// - returns NaN if metric is not defined or inputs invalid
double computeMetric(MetricType type,
                     const cv::Mat& ref,
                     const cv::Mat& test);

// Utility to get user-friendly label for result,
// without Qt (std::string → w GUI zrobisz QString::fromStdString()).
std::string formatMetricResult(MetricType type, double value);

#endif // IMAGE_METRICS_H
