#ifndef IMAGEFORMATS_H
#define IMAGEFORMATS_H

#include <QFileInfo>

enum class ImageFormat {
    Png,
    Jpeg,
    Jp2,
    Bmp,
    Gif,
    Tiff,
    Webp,
    Avif,
    Unknown
};

enum class CompressionNature {
    Lossless,   // always lossless recording (for a given encoder track)
    Lossy,      // always a loss
    Hybrid      // can be lossless or lossy (WebP, AVIF, indexed/truecolor PNG)
};

// The only shared list of extensions – HERE
inline QStringList supportedImageExtensions()
{
    return {
        "png", "jpg", "jpeg", "bmp", "gif", "tif", "tiff", "webp", "avif"
    };
}

inline QString fileDialogFilterAllImages()
{
    // eq. "Images (*.png *.jpg ...)"
    QStringList exts = supportedImageExtensions();
    QStringList patterns;
    for (const QString& e : exts)
        patterns << "*." + e;
    return QString("Images (%1)").arg(patterns.join(' '));
}

inline ImageFormat detectImageFormat(const QString& path)
{
    QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "png")  return ImageFormat::Png;
    if (ext == "jpg" || ext == "jpeg") return ImageFormat::Jpeg;
    if (ext == "jp2") return ImageFormat::Jp2;
    if (ext == "bmp")  return ImageFormat::Bmp;
    if (ext == "gif")  return ImageFormat::Gif;
    if (ext == "tif" || ext == "tiff") return ImageFormat::Tiff;
    if (ext == "webp") return ImageFormat::Webp;
    if (ext == "avif") return ImageFormat::Avif;
    return ImageFormat::Unknown;
}

inline CompressionNature compressionNature(ImageFormat fmt)
{
    switch (fmt) {
    case ImageFormat::Png:
            // PNG is formally lossless,
            // but if you index/quantize it yourself, that part is lossy.
            // At the “format” level, let's assume: lossless format.
            return CompressionNature::Lossless;
    case ImageFormat::Bmp:
            return CompressionNature::Lossless;
    case ImageFormat::Gif:
            // GIF: indexed format; if input image == indexed palette → lossless,
            // if we have to quantize → lossy. We treat it as “Hybrid.”
            return CompressionNature::Hybrid;
    case ImageFormat::Tiff:
            // TIFF can vary (LZW, JPEG), but the typical workflow is: lossless → Hybrid.
            return CompressionNature::Hybrid;
    case ImageFormat::Webp:
            // WebP can be lossless or lossy.
            return CompressionNature::Hybrid;
    case ImageFormat::Avif:
            // AVIF can also be lossless or lossy.
            return CompressionNature::Hybrid;
    case ImageFormat::Jpeg:
            // JPEG is always lossy.
        return CompressionNature::Lossy;
    case ImageFormat::Jp2:
        // JPEG is always lossy.
        return CompressionNature::Lossy;
    default:
        return CompressionNature::Hybrid;
    }
}

inline bool isPureLosslessFormat(ImageFormat fmt)
{
    return compressionNature(fmt) == CompressionNature::Lossless;
}

inline bool isAlwaysLossyFormat(ImageFormat fmt)
{
    return compressionNature(fmt) == CompressionNature::Lossy;
}

#endif // IMAGEFORMATS_H
