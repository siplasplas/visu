#include "jpeg_lossless.h"

#include <turbojpeg.h>
#include <QString>
#include <QByteArray>
#include <cstdio>
#include <cstdlib>

bool rotateJpegLossless(const QString& filePath, int steps90)
{
    steps90 %= 4;
    if (steps90 < 0)
        steps90 += 4;
    if (steps90 == 0)
        return true; // nic do zrobienia

    tjtransform xform;
    std::memset(&xform, 0, sizeof(xform));

    switch (steps90) {
        case 1: xform.op = TJXOP_ROT270; break;
        case 2: xform.op = TJXOP_ROT180; break;
        case 3: xform.op = TJXOP_ROT90;  break;
        default: xform.op = TJXOP_NONE;  break;
    }

    // You can enable TRIM to cut off mismatched 8x8 blocks
    // (but I still check that the dimensions are multiples of 8).
    xform.options = 0; // lub TJXOPT_TRIM

    QByteArray pathBytes = filePath.toLocal8Bit();

    // 1. Load the entire JPEG into memory
    FILE* infile = std::fopen(pathBytes.constData(), "rb");
    if (!infile) {
        std::fprintf(stderr, "rotateJpegLossless: cannot open input: %s\n",
                     pathBytes.constData());
        return false;
    }

    if (std::fseek(infile, 0, SEEK_END) != 0) {
        std::fclose(infile);
        return false;
    }
    long fileSize = std::ftell(infile);
    if (fileSize <= 0) {
        std::fclose(infile);
        return false;
    }
    std::rewind(infile);

    unsigned char* jpegBuf = (unsigned char*)std::malloc(fileSize);
    if (!jpegBuf) {
        std::fclose(infile);
        return false;
    }

    size_t readBytes = std::fread(jpegBuf, 1, fileSize, infile);
    std::fclose(infile);
    if (readBytes != (size_t)fileSize) {
        std::free(jpegBuf);
        return false;
    }

    unsigned char* dstBuf = nullptr;
    unsigned long  dstSize = 0;

    tjhandle handle = tjInitTransform();
    if (!handle) {
        std::fprintf(stderr, "rotateJpegLossless: tjInitTransform failed: %s\n",
                     tjGetErrorStr());
        std::free(jpegBuf);
        return false;
    }

    int res = tjTransform(handle,
                          jpegBuf, (unsigned long)fileSize,
                          1,
                          &dstBuf, &dstSize,
                          &xform,
                          0);
    if (res != 0) {
        std::fprintf(stderr, "rotateJpegLossless: tjTransform failed: %s\n",
                     tjGetErrorStr());
        tjDestroy(handle);
        std::free(jpegBuf);
        if (dstBuf) tjFree(dstBuf);
        return false;
    }

    // 3. Save the result to a temporary file
    QString tmpPath = filePath + ".tmp_rot.jpg";
    QByteArray tmpBytes = tmpPath.toLocal8Bit();
    FILE* outfile = std::fopen(tmpBytes.constData(), "wb");
    if (!outfile) {
        std::fprintf(stderr, "rotateJpegLossless: cannot open temp: %s\n",
                     tmpBytes.constData());
        tjDestroy(handle);
        std::free(jpegBuf);
        tjFree(dstBuf);
        return false;
    }

    size_t written = std::fwrite(dstBuf, 1, dstSize, outfile);
    std::fclose(outfile);

    tjDestroy(handle);
    std::free(jpegBuf);
    tjFree(dstBuf);

    if (written != dstSize) {
        std::remove(tmpBytes.constData());
        return false;
    }

    // 4. Replace the file
    if (std::remove(pathBytes.constData()) != 0) {
        std::remove(tmpBytes.constData());
        return false;
    }
    if (std::rename(tmpBytes.constData(), pathBytes.constData()) != 0) {
        std::remove(tmpBytes.constData());
        return false;
    }

    return true;
}
