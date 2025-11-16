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
