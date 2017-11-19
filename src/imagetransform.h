#ifndef IMAGETRANSFORM_H
#define IMAGETRANSFORM_H

// Copyright Peter Diener

#include <QObject>
#include <QColor>
#include <QRgb>
#include <QImage>
#include <math.h>
#include <QVector3D>
#include <QList>
#include <QRect>

class ImageTransform : public QObject
{
    Q_OBJECT
public:
    explicit ImageTransform(QObject *parent = 0);

signals:

public:
    QColor colorOfSurroundingRect(QImage image);
    static quint32 colorDistance(QColor colorOne, QColor colorTwo);
    QImage difference(QImage &img_1, QImage &img_2);
    QImage extractImageRegion(QImage originalImage, QPolygonF polygonRegion);
    static QImage *highPassFilter(QImage *source);
    QPolygonF findVerticesOfImage(QImage image, quint32 threshold, qint32 hitsNeeded);
    QImage* rotateImage(char direction, QImage *image);
    QImage *medianFilter(QImage* inputImage);
    QList<QRect *> *rectsOfInterest(QImage* image);

private:
    QImage* medianShadow;
    QRect* nextNeighbourRect(QList<QRect *> *rects, QPoint pos);
};

#endif // IMAGETRANSFORM_H
