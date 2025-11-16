#include "image_metrics.h"

#include <opencv2/imgproc.hpp>
#include <cmath>
#include <limits>

double computePsnr(const cv::Mat& ref, const cv::Mat& test)
{
    CV_Assert(!ref.empty());
    CV_Assert(ref.size() == test.size());
    CV_Assert(ref.type() == test.type());

    cv::Mat diff;
    cv::absdiff(ref, test, diff);
    diff.convertTo(diff, CV_32F);
    diff = diff.mul(diff);

    cv::Scalar s = cv::sum(diff);
    double sse = 0.0;
    for (int i = 0; i < diff.channels(); ++i)
        sse += s[i];

    if (sse <= 1e-10)
        return std::numeric_limits<double>::infinity();

    double mse = sse / (double)(ref.channels() * ref.total());
    double psnr = 10.0 * std::log10((255.0 * 255.0) / mse);
    return psnr;
}

double computeSsim(const cv::Mat& ref, const cv::Mat& test)
{
    CV_Assert(!ref.empty());
    CV_Assert(ref.size() == test.size());
    CV_Assert(ref.type() == test.type());

    cv::Mat I1, I2;
    if (ref.channels() == 3) {
        cv::cvtColor(ref, I1, cv::COLOR_BGR2GRAY);
        cv::cvtColor(test, I2, cv::COLOR_BGR2GRAY);
    } else {
        I1 = ref.clone();
        I2 = test.clone();
    }

    I1.convertTo(I1, CV_32F);
    I2.convertTo(I2, CV_32F);

    const double C1 = 6.5025, C2 = 58.5225;

    cv::Mat mu1, mu2;
    cv::GaussianBlur(I1, mu1, cv::Size(11, 11), 1.5);
    cv::GaussianBlur(I2, mu2, cv::Size(11, 11), 1.5);

    cv::Mat mu1_2   = mu1.mul(mu1);
    cv::Mat mu2_2   = mu2.mul(mu2);
    cv::Mat mu1_mu2 = mu1.mul(mu2);

    cv::Mat sigma1_2, sigma2_2, sigma12;

    cv::Mat I1_2 = I1.mul(I1);
    cv::Mat I2_2 = I2.mul(I2);
    cv::Mat I1_I2 = I1.mul(I2);

    cv::GaussianBlur(I1_2, sigma1_2, cv::Size(11, 11), 1.5);
    sigma1_2 -= mu1_2;

    cv::GaussianBlur(I2_2, sigma2_2, cv::Size(11, 11), 1.5);
    sigma2_2 -= mu2_2;

    cv::GaussianBlur(I1_I2, sigma12, cv::Size(11, 11), 1.5);
    sigma12 -= mu1_mu2;

    cv::Mat t1, t2, t3;
    t1 = 2 * mu1_mu2 + C1;
    t2 = 2 * sigma12 + C2;
    t3 = t1.mul(t2);

    t1 = mu1_2 + mu2_2 + C1;
    t2 = sigma1_2 + sigma2_2 + C2;
    t1 = t1.mul(t2);

    cv::Mat ssimMap;
    cv::divide(t3, t1, ssimMap);

    cv::Scalar mssim = cv::mean(ssimMap);
    return mssim.val[0];
}
