#include "image_metrics.h"

#include <opencv2/core.hpp>
#include <opencv2/core/hal/interface.h>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <limits>

// element-wise cosine for CV_32F or CV_64F
static cv::Mat matCos(const cv::Mat& src)
{
    CV_Assert(src.type() == CV_32F || src.type() == CV_64F);

    cv::Mat dst(src.size(), src.type());

    int rows = src.rows;
    int cols = src.cols * src.channels();

    if (src.type() == CV_32F)
    {
        for (int y = 0; y < rows; ++y) {
            const float* s = src.ptr<float>(y);
            float* d = dst.ptr<float>(y);
            for (int x = 0; x < cols; ++x)
                d[x] = std::cos(s[x]);
        }
    }
    else // CV_64F
    {
        for (int y = 0; y < rows; ++y) {
            const double* s = src.ptr<double>(y);
            double* d = dst.ptr<double>(y);
            for (int x = 0; x < cols; ++x)
                d[x] = std::cos(s[x]);
        }
    }

    return dst;
}

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

double computeMsSsim(const cv::Mat& ref, const cv::Mat& test, int levels)
{
    CV_Assert(!ref.empty());
    CV_Assert(ref.size() == test.size());
    CV_Assert(ref.type() == test.type());

    if (levels < 1)
        levels = 1;
    if (levels > 5)
        levels = 5;

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

    // Wang et al. (2003) weights for 5 scales
    const double weightsAll[5] = { 0.0448, 0.2856, 0.3001, 0.2363, 0.1333 };

    // normalize weights for the used number of levels
    std::vector<double> weights(levels);
    double wsum = 0.0;
    for (int i = 0; i < levels; ++i) {
        weights[i] = weightsAll[i];
        wsum += weights[i];
    }
    if (wsum > 0.0) {
        for (int i = 0; i < levels; ++i)
            weights[i] /= wsum;
    }

    const double C1 = 6.5025, C2 = 58.5225;

    std::vector<double> mcs(levels, 0.0);
    double mssimL = 0.0;

    cv::Mat current1 = I1.clone();
    cv::Mat current2 = I2.clone();

    for (int scale = 0; scale < levels; ++scale) {
        cv::Mat mu1, mu2;
        cv::GaussianBlur(current1, mu1, cv::Size(11, 11), 1.5);
        cv::GaussianBlur(current2, mu2, cv::Size(11, 11), 1.5);

        cv::Mat mu1_2   = mu1.mul(mu1);
        cv::Mat mu2_2   = mu2.mul(mu2);
        cv::Mat mu1_mu2 = mu1.mul(mu2);

        cv::Mat sigma1_2, sigma2_2, sigma12;

        cv::Mat tmp1 = current1.mul(current1);
        cv::Mat tmp2 = current2.mul(current2);
        cv::Mat tmp12 = current1.mul(current2);

        cv::GaussianBlur(tmp1, sigma1_2, cv::Size(11, 11), 1.5);
        sigma1_2 -= mu1_2;

        cv::GaussianBlur(tmp2, sigma2_2, cv::Size(11, 11), 1.5);
        sigma2_2 -= mu2_2;

        cv::GaussianBlur(tmp12, sigma12, cv::Size(11, 11), 1.5);
        sigma12 -= mu1_mu2;

        cv::Mat t1, t2, t3;

        // luminance term
        t1 = 2 * mu1_mu2 + C1;
        t2 = mu1_2 + mu2_2 + C1;
        cv::Mat lMap;
        cv::divide(t1, t2, lMap);

        // contrast-structure term
        t1 = 2 * sigma12 + C2;
        t2 = sigma1_2 + sigma2_2 + C2;
        cv::Mat csMap;
        cv::divide(t1, t2, csMap);

        cv::Scalar lMean  = cv::mean(lMap);
        cv::Scalar csMean = cv::mean(csMap);

        if (scale < levels - 1) {
            mcs[scale] = csMean[0];
        } else {
            mssimL = lMean[0];
        }

        // prepare next scale
        if (scale < levels - 1) {
            cv::Mat down1, down2;
            cv::pyrDown(current1, down1);
            cv::pyrDown(current2, down2);
            current1 = down1;
            current2 = down2;
        }
    }

    // combine scales: product of cs^(w) for all but last,
    // and l^(w) for last scale
    double msSsim = 1.0;
    for (int i = 0; i < levels - 1; ++i) {
        msSsim *= std::pow(std::max(mcs[i], 0.0), weights[i]);
    }
    msSsim *= std::pow(std::max(mssimL, 0.0), weights[levels - 1]);

    return msSsim;
}

double computeFsim(const cv::Mat& ref, const cv::Mat& test)
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

    // Sobel gradients
    cv::Mat gx1, gy1, gx2, gy2;
    cv::Sobel(I1, gx1, CV_32F, 1, 0, 3);
    cv::Sobel(I1, gy1, CV_32F, 0, 1, 3);
    cv::Sobel(I2, gx2, CV_32F, 1, 0, 3);
    cv::Sobel(I2, gy2, CV_32F, 0, 1, 3);

    cv::Mat gm1, gm2, phase1, phase2;
    cv::magnitude(gx1, gy1, gm1);
    cv::magnitude(gx2, gy2, gm2);

    cv::phase(gx1, gy1, phase1); // radians
    cv::phase(gx2, gy2, phase2);

    // Feature importance: max gradient magnitude
    cv::Mat gmMax;
    cv::max(gm1, gm2, gmMax);

    // Gradient magnitude similarity (GMS-like)
    const float T1 = 0.002f;
    cv::Mat gmsNum  = 2.0f * gm1.mul(gm2) + T1;
    cv::Mat gmsDen  = gm1.mul(gm1) + gm2.mul(gm2) + T1;
    cv::Mat gmsMap;
    cv::divide(gmsNum, gmsDen, gmsMap);

    // Phase similarity (coarse approximation)
    cv::Mat dphi = phase1 - phase2;
    cv::Mat cosMap = matCos(dphi);  // in [-1,1]

    // Normalize phase similarity to [0,1]
    const float T2 = 0.001f;
    cv::Mat phsNum = 2.0f * cosMap + T2;

    cv::Mat phsDen(phsNum.size(), CV_32F, cv::Scalar(2.0f + T2));

    cv::Mat phsMap;
    cv::divide(phsNum, phsDen, phsMap); // ~[0,1]

    // Combined similarity map
    cv::Mat simMap = gmsMap.mul(phsMap);

    // Weighted pooling by feature strength (gmMax)
    cv::Mat weight = gmMax;
    cv::Mat weighted;
    simMap.convertTo(simMap, CV_32F);
    weight.convertTo(weight, CV_32F);
    weighted = simMap.mul(weight);

    cv::Scalar num = cv::sum(weighted);
    cv::Scalar den = cv::sum(weight);

    float numVal = static_cast<float>(num[0]);
    float denVal = static_cast<float>(den[0]);
    if (denVal <= 1e-6f)
        return 1.0; // brak wyróżnionych cech → traktujemy jako identyczne

    float fsim = numVal / denVal;
    // zabezpieczenie przed numerycznymi śmieciami
    if (fsim < 0.0f) fsim = 0.0f;
    if (fsim > 1.0f) fsim = 1.0f;
    return fsim;
}

double computeGmsd(const cv::Mat& ref, const cv::Mat& test)
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

    // Sobel gradient magnitude
    cv::Mat gx1, gy1, gx2, gy2;
    cv::Sobel(I1, gx1, CV_32F, 1, 0, 3);
    cv::Sobel(I1, gy1, CV_32F, 0, 1, 3);
    cv::Sobel(I2, gx2, CV_32F, 1, 0, 3);
    cv::Sobel(I2, gy2, CV_32F, 0, 1, 3);

    cv::Mat gm1, gm2;
    cv::magnitude(gx1, gy1, gm1);
    cv::magnitude(gx2, gy2, gm2);

    // GMS = (2 * m1 * m2 + c) / (m1^2 + m2^2 + c)
    const float c = 170.0f;  // stabilizing constant from the paper

    cv::Mat numerator  = 2.0f * gm1.mul(gm2) + c;
    cv::Mat denominator = gm1.mul(gm1) + gm2.mul(gm2) + c;

    cv::Mat gms;
    cv::divide(numerator, denominator, gms);  // gms in ~[0,1]

    // GMSD = standard deviation of similarity map
    cv::Scalar mean, stddev;
    cv::meanStdDev(gms, mean, stddev);

    return static_cast<double>(stddev[0]);
}

double computeVif(const cv::Mat& ref, const cv::Mat& test)
{
    CV_Assert(!ref.empty());
    CV_Assert(ref.size() == test.size());
    CV_Assert(ref.type() == test.type());

    cv::Mat X, Y;
    if (ref.channels() == 3) {
        cv::cvtColor(ref, X, cv::COLOR_BGR2GRAY);
        cv::cvtColor(test, Y, cv::COLOR_BGR2GRAY);
    } else {
        X = ref.clone();
        Y = test.clone();
    }

    // Work in float, 0..255 range.
    X.convertTo(X, CV_32F);
    Y.convertTo(Y, CV_32F);

    // Sheikh & Bovik: recommended noise variance for 8-bit images.
    const float sigma_nsq = 2.0f;
    const float eps = 1e-10f;

    // Multi-scale VIF: use 4 scales on a Gaussian pyramid.
    const int numScales = 4;

    cv::Mat curX = X;
    cv::Mat curY = Y;

    double num = 0.0;
    double den = 0.0;

    for (int s = 0; s < numScales; ++s) {
        // Local means
        cv::Mat muX, muY;
        cv::GaussianBlur(curX, muX, cv::Size(5, 5), 1.0);
        cv::GaussianBlur(curY, muY, cv::Size(5, 5), 1.0);

        // Local second-order statistics
        cv::Mat X2 = curX.mul(curX);
        cv::Mat Y2 = curY.mul(curY);
        cv::Mat XY = curX.mul(curY);

        cv::Mat sigmaX2, sigmaY2, sigmaXY;
        cv::GaussianBlur(X2, sigmaX2, cv::Size(5, 5), 1.0);
        cv::GaussianBlur(Y2, sigmaY2, cv::Size(5, 5), 1.0);
        cv::GaussianBlur(XY, sigmaXY, cv::Size(5, 5), 1.0);

        sigmaX2 -= muX.mul(muX);
        sigmaY2 -= muY.mul(muY);
        sigmaXY -= muX.mul(muY);

        // Ensure non-negative variances (numerical safety)
        cv::max(sigmaX2, 0.0f, sigmaX2);
        cv::max(sigmaY2, 0.0f, sigmaY2);

        double numScale = 0.0;
        double denScale = 0.0;

        const int rows = curX.rows;
        const int cols = curX.cols;

        for (int y = 0; y < rows; ++y) {
            const float* sx2p = sigmaX2.ptr<float>(y);
            const float* sy2p = sigmaY2.ptr<float>(y);
            const float* sxyp = sigmaXY.ptr<float>(y);

            for (int x = 0; x < cols; ++x) {
                float sx2 = sx2p[x];
                float sy2 = sy2p[x];
                float sxy = sxyp[x];

                // Ignore very flat patches
                if (sx2 < eps)
                    continue;

                // Linear gain between reference and test patch
                double g = sxy / (sx2 + eps);

                // Noise variance in the distorted patch
                double sv2 = sy2 - g * sxy;
                if (sv2 < eps)
                    sv2 = eps;

                // Clamp gain and noise as in typical implementations
                if (sx2 < eps) {
                    g = 0.0;
                    sv2 = sy2;
                }
                if (sy2 < eps) {
                    g = 0.0;
                    sv2 = 0.0;
                }

                // Numerator: log(1 + g^2 * sigma_x^2 / (sigma_v^2 + sigma_n^2))
                double termNum = std::log(1.0 + (g * g * sx2) / (sv2 + sigma_nsq));
                // Denominator: log(1 + sigma_x^2 / sigma_n^2)
                double termDen = std::log(1.0 + sx2 / sigma_nsq);

                numScale += termNum;
                denScale += termDen;
            }
        }

        num += numScale;
        den += denScale;

        // Prepare next scale
        if (s < numScales - 1) {
            cv::Mat downX, downY;
            cv::pyrDown(curX, downX);
            cv::pyrDown(curY, downY);
            curX = downX;
            curY = downY;
        }
    }

    if (den <= 0.0)
        return 1.0; // if there is no variance at all, treat as identical

    double vif = num / den;
    // Clamp to [0,1] for robustness
    if (vif < 0.0) vif = 0.0;
    if (vif > 1.0) vif = 1.0;
    return vif;
}

static const std::vector<MetricDef> kMetricDefs = {
    { MetricType::PSNR,    "psnr",    "PSNR",
      "Peak Signal-to-Noise Ratio (dB)" },
    { MetricType::SSIM,    "ssim",    "SSIM",
      "Structural Similarity Index" },
    { MetricType::MS_SSIM, "ms-ssim", "MS-SSIM",
      "Multi-scale Structural Similarity Index" },
    { MetricType::FSIM,    "fsim",    "FSIM",
      "Feature-based Similarity Index (approx.)" },
    { MetricType::GMSD,   "gmsd",   "GMSD",
      "Gradient Magnitude Similarity Deviation (lower is better)" },
    { MetricType::VIF,     "vif",     "VIF",
      "Visual Information Fidelity (0..1, higher is better)" },
};

const std::vector<MetricDef>& getAvailableMetrics()
{
    return kMetricDefs;
}

double computeMetric(MetricType type,
                     const cv::Mat& ref,
                     const cv::Mat& test)
{
    switch (type) {
        case MetricType::PSNR:
            return computePsnr(ref, test);
        case MetricType::SSIM:
            return computeSsim(ref, test);
        case MetricType::MS_SSIM:
            return computeMsSsim(ref, test);
        case MetricType::FSIM:
            return computeFsim(ref, test);
        case MetricType::GMSD:
            return computeGmsd(ref, test);
        case MetricType::VIF:
            return computeVif(ref, test);
        case MetricType::None:
        default:
            return std::numeric_limits<double>::quiet_NaN();
    }
}

std::string formatMetricResult(MetricType type, double value)
{
    if (std::isnan(value)) {
        return "N/A";
    }

    char buf[128];

    switch (type) {
        case MetricType::PSNR:
            if (std::isinf(value))
                return "PSNR: inf (identical)";
            std::snprintf(buf, sizeof(buf), "PSNR: %.2f dB", value);
            return buf;

        case MetricType::SSIM:
            std::snprintf(buf, sizeof(buf), "SSIM: %.4f", value);
            return buf;

        case MetricType::MS_SSIM:
            std::snprintf(buf, sizeof(buf), "MS-SSIM: %.4f", value);
            return buf;

        case MetricType::FSIM:
            std::snprintf(buf, sizeof(buf), "FSIM: %.4f", value);
            return buf;

        case MetricType::GMSD:
            std::snprintf(buf, sizeof(buf), "GMSD: %.6f (lower is better)", value);
            return buf;

        case MetricType::VIF:
            std::snprintf(buf, sizeof(buf), "VIF: %.4f", value);
            return buf;

        case MetricType::None:
        default:
            return "Metric: None";
    }
}