#pragma once

#include <string>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

// Simple API similar to cv::imread, but only for AVIF
// flags: CV_8UC3 (IMREAD_COLOR) lub CV_8UC4 (IMREAD_UNCHANGED) / GRAY
cv::Mat imreadAvif(const std::string& filename, int flags = cv::IMREAD_COLOR);

// save to AVIF (quality 0–100; 0 = lowest quality, 100 = best quality)
bool imwriteAvif(const std::string& filename,
                 const cv::Mat& image,
                 int quality = 90);
