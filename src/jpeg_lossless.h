#ifndef JPEG_LOSSLESS_H
#define JPEG_LOSSLESS_H

#include <QString>

// Returns true if lossless JPEG rotation was successful
// steps90: 1 = 90° right, 2 = 180°, 3 = 270°, 0 = nothing
bool rotateJpegLossless(const QString& filePath, int steps90);

#endif // JPEG_LOSSLESS_H
