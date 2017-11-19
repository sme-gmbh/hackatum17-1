#include "imagetransform.h"

// Copyright Peter Diener
// Contributor: Jonas Weigand

ImageTransform::ImageTransform(QObject *parent) :
    QObject(parent)
{
    medianShadow = NULL;
}

// Liefert den durchschnittlichen Farbwert des Randes eines Bildes zurück
QColor ImageTransform::colorOfSurroundingRect(QImage image)
{
    quint64 meanRed = 0;
    quint64 meanGreen = 0;
    quint64 meanBlue = 0;

    qint32 countPixel = 0;

    qint32 imageHeight = image.height();
    qint32 imageWidth = image.width();

    QColor tmp;

    // Left and right edges
    for(qint32 y = 1; y < imageHeight - 1; y++)
    {
        // Left
        tmp = image.pixel(0,y);
        meanRed += tmp.red();
        meanBlue += tmp.blue();
        meanGreen += tmp.green();

        // Right
        tmp = image.pixel(imageWidth - 1,y);
        meanRed += tmp.red();
        meanBlue += tmp.blue();
        meanGreen += tmp.green();
        countPixel += 2;
    }

    // Top and bottom edges
    for(qint32 x = 0; x < imageWidth; x++)
    {
        // Top
        tmp = image.pixel(x, 0);
        meanRed += tmp.red();
        meanBlue += tmp.blue();
        meanGreen += tmp.green();

        // Bottom
        tmp = image.pixel(x,imageHeight - 1);
        meanRed += tmp.red();
        meanBlue += tmp.blue();
        meanGreen += tmp.green();
        countPixel += 2;
    }

    // Normalize accumulators
    meanRed /= countPixel;
    meanGreen /= countPixel;
    meanBlue /= countPixel;

    QColor meanColor(meanRed,meanGreen,meanBlue, 255);

    return meanColor;
}


// Liefert den numerischen Abstand (trigonometrisches Maß) zweier Farbwerte
quint32 ImageTransform::colorDistance(QColor colorOne, QColor colorTwo)
{
    quint32 distance;

    distance = sqrt(
                pow(colorOne.red()   - colorTwo.red()  , 2) +
                pow(colorOne.green() - colorTwo.green(), 2) +
                pow(colorOne.blue()  - colorTwo.blue() , 2)
                );

    return distance;
}

QImage ImageTransform::difference(QImage &img_1, QImage &img_2)
{
    if (img_1.size() != img_2.size())
        return QImage();

    int width = img_1.width();
    int height = img_1.height();

    QImage output(width, height, img_1.format());

    for (int x = 0; x < width; x++)
    {
        for (int y = 0; y < height; y++)
        {
            int red = QColor(img_1.pixel(x, y)).red() - QColor(img_2.pixel(x, y)).red() + 127;
            int green = QColor(img_1.pixel(x, y)).green() - QColor(img_2.pixel(x, y)).green() + 127;
            int blue = QColor(img_1.pixel(x, y)).blue() - QColor(img_2.pixel(x, y)).blue() + 127;

            if (red < 0)        red = 0;
            if (red > 255)      red = 255;
            if (green < 0)      green = 0;
            if (green > 255)    green = 255;
            if (blue < 0)       blue = 0;
            if (blue > 255)     blue = 255;

            output.setPixel(x, y, QColor(red, green, blue).rgba());
        }
    }

    return output;
}


// Streckt den Inhalt eines Originalbildes, der durch ein 4-eckiges Polygon begrenzt wird.
// Rückgeliefert wird ein Bild das in der Größe dem Original entspricht.
QImage ImageTransform::extractImageRegion(QImage originalImage, QPolygonF polygonRegion)
{
    if (polygonRegion.isEmpty())
        return originalImage;
    QPolygonF polygonTarget;

    polygonTarget.begin();
    polygonTarget.append(QPointF(0, 0));
    polygonTarget.append(QPointF(originalImage.width(), 0));
    polygonTarget.append(QPointF(originalImage.width(), originalImage.height()));
    polygonTarget.append(QPointF(0, originalImage.height()));

    polygonTarget.end();

    QTransform transform;
    bool check = QTransform::quadToQuad(polygonRegion, polygonTarget, transform);
    if(check == false) return QImage();

    QTransform trueMatrix = originalImage.trueMatrix(transform, originalImage.width(), originalImage.height());

    QPolygonF polygonDestination = trueMatrix.map(polygonRegion);

    QImage image_transformed = originalImage.transformed(transform,Qt::SmoothTransformation).copy(
                polygonDestination.boundingRect().x(),
                polygonDestination.boundingRect().y(),
                polygonDestination.boundingRect().width(),
                polygonDestination.boundingRect().height());

    image_transformed = image_transformed.scaled(originalImage.width(),
                                                 originalImage.height(),
                                                 Qt::IgnoreAspectRatio,
                                                 Qt::SmoothTransformation);

    return image_transformed;
}

QImage* ImageTransform::highPassFilter(QImage* source)
{
    QImage* hpfImage = new QImage(source->width(), source->height(), QImage::Format_ARGB32_Premultiplied);
    hpfImage->fill(Qt::black);

    for (qint32 y = 1; y < (hpfImage->height() - 1); y++)
        for (qint32 x = 1; x < (hpfImage->width() -1 ); x++)
        {
            quint32 distance = 0;
            distance += colorDistance(source->pixel(x, y), source->pixel(x-1, y));   // linkes Pixel
            //distance += colorDistance(source->pixel(x, y), source->pixel(x-1, y-1));   // linkes oberes Pixel
            distance += colorDistance(source->pixel(x, y), source->pixel(x, y-1));   // oberes Pixel
            //distance += colorDistance(source->pixel(x, y), source->pixel(x+1, y-1));   // oberes rechtes Pixel
            distance += colorDistance(source->pixel(x, y), source->pixel(x+1, y));   // rechtes Pixel
            //distance += colorDistance(source->pixel(x, y), source->pixel(x+1, y+1));   // rechtes unteres Pixel
            distance += colorDistance(source->pixel(x, y), source->pixel(x, y+1));   // unteres Pixel
            //distance += colorDistance(source->pixel(x, y), source->pixel(x-1, y+1));   // unteres linkes Pixel
            distance *= 2;
            //distance *= 2;        // war eingeschaltet
            if (distance > 255) distance = 255;
//            if (distance < 180) distance = 0;   // war 180
            hpfImage->setPixel(x, y, QColor(distance, distance, distance).rgba());
        }

    return hpfImage;
}


// Liefert das Poygon zurück, das die Kanten eines abfotografierten Bildes darstellt
// threshold ist der numerische Farbabstand, ab dem eine Ecke des Bildes als Ecke erkannt wird.
QPolygonF ImageTransform::findVerticesOfImage(QImage image, quint32 threshold, qint32 hitsNeeded)
{
    //QColor backgroundColor = colorOfSurroundingRect(image);
    QColor backgroundColor = Qt::white;
    bool done;
    qint32 hitCounter;
    QColor lastPixel;

    // Anmerkung: Eigentlich darf man nicht vertikal und horizontal durchscannen, sondern muss von den
    // Ecken des Originalbildes ausgehend mit 45 Grad geneigten Scanlines arbeiten, damit eine Ecke mit
    // ungünstigem Kantenwinkel nicht doppelt erfasst wird.

    qint32 endOfTest = 0;

    if (image.height() > endOfTest) endOfTest = image.height();
    if (image.width()  > endOfTest) endOfTest = image.width();

    // Scan for top-left vertex
    QPointF topLeft_touchpoint;
    done = false;
    hitCounter = 0;
    lastPixel = backgroundColor;

    for (qint32 i = 0; i < endOfTest; i++)
    {
        if (done) break;
        for (qint32 j = 0; j <= i; j++)
        {
            quint32 x = j;
            quint32 y = i - j;

            if (!image.valid(x, y)) continue;

            if (done) break;
            QColor color(image.pixel(x, y));
            quint32 distance = colorDistance(color, backgroundColor);
            //quint32 distance = colorDistance(lastPixel, color);
            //lastPixel = color;

            if (distance > threshold)
            {
                hitCounter++;
                if (hitCounter >= hitsNeeded)
                {
                    topLeft_touchpoint.setX(x);
                    topLeft_touchpoint.setY(y);
                    done = true;
                }
            }
            else
            {
                hitCounter = 0;
            }
        }
    }

    // Scan for top-right vertex
    QPointF topRight_touchpoint;
    done = false;
    hitCounter = 0;
    lastPixel = backgroundColor;

    for (qint32 i = 0; i < endOfTest; i++)
    {
        if (done) break;
        for (qint32 j = 0; j <= i; j++)
        {
            quint32 x = image.width() - 1 - j;
            quint32 y = i - j;

            if (!image.valid(x, y)) continue;

            if (done) break;
            QColor color(image.pixel(x, y));
            quint32 distance = colorDistance(color, backgroundColor);
            //quint32 distance = colorDistance(lastPixel, color);
            //lastPixel = color;

            if (distance > threshold)
            {
                hitCounter++;
                if (hitCounter >= hitsNeeded)
                {
                    topRight_touchpoint.setX(x);
                    topRight_touchpoint.setY(y);
                    done = true;
                }
            }
            else
            {
                hitCounter = 0;
            }
        }
    }

    // Scan for bottom-right vertex
    QPointF bottomRight_touchpoint;
    done = false;
    hitCounter = 0;
    lastPixel = backgroundColor;

    for (qint32 i = 0; i < endOfTest; i++)
    {
        if (done) break;
        for (qint32 j = 0; j <= i; j++)
        {
            quint32 x = image.width() - 1 - j;
            quint32 y = image.height() - 1 - i + j;

            if (!image.valid(x, y)) continue;

            if (done) break;
            QColor color(image.pixel(x, y));
            quint32 distance = colorDistance(color, backgroundColor);
            //quint32 distance = colorDistance(lastPixel, color);
            //lastPixel = color;

            if (distance > threshold)
            {
                hitCounter++;
                if (hitCounter >= hitsNeeded)
                {
                    bottomRight_touchpoint.setX(x);
                    bottomRight_touchpoint.setY(y);
                    done = true;
                }
            }
            else
            {
                hitCounter = 0;
            }
        }
    }

    // Scan for bottom-left vertex
    QPointF bottomLeft_touchpoint;
    done = false;
    hitCounter = 0;
    lastPixel = backgroundColor;

    for (qint32 i = 0; i < endOfTest; i++)
    {
        if (done) break;
        for (qint32 j = 0; j <= i; j++)
        {
            quint32 x = j;
            quint32 y = image.height() - 1 - i + j;

            if (!image.valid(x, y)) continue;

            if (done) break;
            QColor color(image.pixel(x, y));
            quint32 distance = colorDistance(color, backgroundColor);
            //quint32 distance = colorDistance(lastPixel, color);
            //lastPixel = color;

            if (distance > threshold)
            {
                hitCounter++;
                if (hitCounter >= hitsNeeded)
                {
                    bottomLeft_touchpoint.setX(x);
                    bottomLeft_touchpoint.setY(y);
                    done = true;
                }
            }
            else
            {
                hitCounter = 0;
            }
        }
    }

    // Create polygon of touchpoints
    QPolygonF polygon;

    // Check if polygon is valid
    bool error = false;
    if (topLeft_touchpoint.x() >= topRight_touchpoint.x())          error = true;
    if (topLeft_touchpoint.x() >= bottomRight_touchpoint.x())       error = true;

    if (bottomLeft_touchpoint.x() >= topRight_touchpoint.x())       error = true;
    if (bottomLeft_touchpoint.x() >= bottomRight_touchpoint.x())    error = true;

    if (topLeft_touchpoint.y() >= bottomLeft_touchpoint.y())        error = true;
    if (topLeft_touchpoint.y() >= bottomRight_touchpoint.y())       error = true;

    if (topRight_touchpoint.y() >= bottomLeft_touchpoint.y())       error = true;
    if (topRight_touchpoint.y() >= bottomRight_touchpoint.y())      error = true;

    // Eckenwinkleauswertung: 180° Eckenwinkle sind nicht ok, und Ecken, die nach innen knicken, sind auch nicht ok
    QVector3D edge_top;
    QVector3D edge_right;
    QVector3D edge_bottom;
    QVector3D edge_left;

    edge_top.setX(topLeft_touchpoint.x() - topRight_touchpoint.x());
    edge_top.setY(topLeft_touchpoint.y() - topRight_touchpoint.y());
    edge_top.setZ(0.0);

    edge_right.setX(topRight_touchpoint.x() - bottomRight_touchpoint.x());
    edge_right.setY(topRight_touchpoint.y() - bottomRight_touchpoint.y());
    edge_right.setZ(0.0);

    edge_bottom.setX(bottomRight_touchpoint.x() - bottomLeft_touchpoint.x());
    edge_bottom.setY(bottomRight_touchpoint.y() - bottomLeft_touchpoint.y());
    edge_bottom.setZ(0.0);

    edge_left.setX(bottomLeft_touchpoint.x() - topLeft_touchpoint.x());
    edge_left.setY(bottomLeft_touchpoint.y() - topLeft_touchpoint.y());
    edge_left.setZ(0.0);

    QVector3D normalVector_topLeft_touchpoint;
    QVector3D normalVector_topRight_touchpoint;
    QVector3D normalVector_bottomRight_touchpoint;
    QVector3D normalVector_bottomLeft_touchpoint;

    normalVector_topLeft_touchpoint = QVector3D::crossProduct(edge_top, edge_left);
    normalVector_topRight_touchpoint = QVector3D::crossProduct(edge_right, edge_top);
    normalVector_bottomRight_touchpoint = QVector3D::crossProduct(edge_bottom, edge_right);
    normalVector_bottomLeft_touchpoint = QVector3D::crossProduct(edge_left, edge_bottom);

    // Bildflächenüberwachung > 500 Pixel²
    if (normalVector_topLeft_touchpoint.z() > -500.0)                 error = true;
    if (normalVector_topRight_touchpoint.z() > -500.0)                error = true;
    if (normalVector_bottomRight_touchpoint.z() > -500.0)             error = true;
    if (normalVector_bottomLeft_touchpoint.z() > -500.0)              error = true;

    // Kantenlängenüberwachung auf größer 100 Pixel
    if (edge_top.length() < 100)     error = true;
    if (edge_right.length() < 100)   error = true;
    if (edge_bottom.length() < 100)  error = true;
    if (edge_left.length() < 100)    error = true;

    // Eckenwinkelbegrenzung; Winkel kleiner 88° führen zu einem Fehler
    qreal angle_threshold = (qreal) sin(88.0 / 360.0 * 2.0 * 3.1415);
    if (qAbs(normalVector_topLeft_touchpoint.z()) / edge_top.length() / edge_left.length() < angle_threshold)         error = true;
    if (qAbs(normalVector_topRight_touchpoint.z()) / edge_right.length() / edge_top.length() < angle_threshold)       error = true;
    if (qAbs(normalVector_bottomRight_touchpoint.z()) / edge_bottom.length() / edge_right.length() < angle_threshold) error = true;
    if (qAbs(normalVector_bottomLeft_touchpoint.z()) / edge_left.length() / edge_bottom.length() < angle_threshold)   error = true;

    if (error) return polygon;  // empty polygon in case of error

    polygon.begin();
    polygon.append(topLeft_touchpoint);
    polygon.append(topRight_touchpoint);
    polygon.append(bottomRight_touchpoint);
    polygon.append(bottomLeft_touchpoint);
    polygon.end();

    return polygon;
}

QImage* ImageTransform::rotateImage(char direction, QImage *image)
{
// By FKaiser
    QImage* temp;

    qint32 buf_x=0, buf_y=0, flag_x=1, flag_y=1;

    if (direction == 'r')
    {
        temp = new QImage(image->height(),image->width(), QImage::Format_ARGB32_Premultiplied);
        buf_y = image->height()-1;
        flag_x=-1;
    }
    else if (direction == 'l')
    {
        temp= new QImage(image->height(),image->width(), QImage::Format_ARGB32_Premultiplied);
        buf_x = image->width()-1;
        flag_y=-1;
    }
    else if (direction == 'o')
    {
        temp= new QImage(image->width(),image->height(), QImage::Format_ARGB32_Premultiplied);
        for(qint32 x = 0; x < image->width(); x++)
            for(qint32 y = 0; y < image->height(); y++)
                temp->setPixel(image->width() -1 - x, image->height() -1 - y, image->pixel(x, y));

        //delete image;
        return temp;
    }
    else return (new QImage());

    for(qint32 x = 0; x < image->width(); x++)
        for(qint32 y = 0; y < image->height(); y++)
            temp->setPixel( (buf_y - y)*flag_y,(buf_x - x)*flag_x, image->pixel(x, y));

    //delete image;
    return temp;

}

QImage* ImageTransform::medianFilter(QImage *inputImage)
{
    int width = inputImage->width();
    int height = inputImage->height();

    if (medianShadow == NULL || (medianShadow->size() != inputImage->size())) {
        if (medianShadow != NULL)
            delete medianShadow;
        medianShadow = new QImage(inputImage->size(), QImage::Format_RGB32);
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int r, g, b;
            QColor px1 = medianShadow->pixelColor(x, y);
            r = px1.red();
            g = px1.green();
            b = px1.blue();

            int dt = 30; // decayTime

            r = r * (dt-1) / dt;
            g = g * (dt-1) / dt;
            b = b * (dt-1) / dt;

            QColor px2 = inputImage->pixelColor(x, y);
            int ri = px2.red();
            int gi = px2.green();
            int bi = px2.blue();

            if (ri >= r - 30)  r += ri / dt;
            else          r /= 4;
            if (gi >= g - 30)  g += gi / dt;
            else          g /= 4;
            if (bi >= b - 30)  b += bi / dt;
            else          b /= 4;

            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;

            medianShadow->setPixelColor(x, y, QColor(r, g, b));
        }
    }

    QImage* out = new QImage(inputImage->size(), QImage::Format_RGB32);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int r = medianShadow->pixelColor(x, y).red();
            int g = medianShadow->pixelColor(x, y).green();
            int b = medianShadow->pixelColor(x, y).blue();

            if ((x < 8) || (y < 8) || (x > width - 8) || (y > height - 8)) {
                r = 0;
                g = 0;
                b = 0;
            }

            r = (r*2 - 150) * 2;
            g = (g*2 - 150) * 2;
            b = (b*2 - 150) * 2;

            if (r < 0) r = 0;
            if (g < 0) g = 0;
            if (b < 0) b = 0;

            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;

            out->setPixelColor(x, y, QColor(r, g, b));
        }
    }

    return out;
}

QList<QRect*>* ImageTransform::rectsOfInterest(QImage *image)
{
    int width = image->width();
    int height = image->height();

    QImage* poi = new QImage(image->size(), QImage::Format_ARGB32);
    poi->fill(Qt::black);

    for (int y = 6; y < height - 6; y++) {
        for (int x = 6; x < width - 6; x++) {

            int px =  image->pixelColor(x, y).red();

            if (px > 128) {
                int interestingPixels = 0;
                int totalPixels = 0;
                for (int dy = -5; dy <= 5; dy++) {
                    for (int dx = -5; dx <= 5; dx++) {
                        int px1 = image->pixelColor(x + dx, y + dy).red();
                        if (px1 > 128)
                            interestingPixels++;
                        totalPixels++;
                    }
                }

                if (interestingPixels > (totalPixels / 8)) {    // more than 1/4 of neighbour pixels are interesting
                    poi->setPixelColor(x, y, Qt::white);
                }
            }

        }
    }

    QList<QRect*>* rects = new QList<QRect*>();

    for (int y = 6; y < height - 6; y++) {
        for (int x = 6; x < width - 6; x++) {
            int px =  poi->pixelColor(x, y).red();
            if (px > 128) {
                QRect* neigbourRect = nextNeighbourRect(rects, QPoint(x, y));

                if (neigbourRect == NULL) { // We need to create a new rect
                    QRect* rect = new QRect(x, y, 1, 1);
                    bool exchangeRect = false;
                    foreach (QRect* r, *rects) {
                        if (r->intersects(*rect)) {
                            QRect united = r->united(*rect);
                            *r = united;
                            exchangeRect = true;
                        }
                    }
                    if (!exchangeRect)
                        rects->append(rect);
                } else {    // we need to make an existing rect bigger
                    QRect united = neigbourRect->united(QRect(x, y, 1, 1));
                    *neigbourRect = united;
                }
            }
        }
    }

    int i = 0;
    int j = 0;

    while (i < rects->length()) {
        j = 0;
        while (j < rects->length()) {
            if (j <= i) {
                j++;
                continue;
            }
            if (rects->at(i)->intersects(*rects->at(j))) {
                QRect united = rects->at(i)->united(*rects->at(j));
                *rects->at(i) = united;
                QRect* toDelete = rects->at(j);
                rects->removeAt(j);
                delete toDelete;
                j--;
            }
            j++;
        }
        i++;
    }

    delete poi;
    return rects;
}

QRect *ImageTransform::nextNeighbourRect(QList<QRect*> *rects, QPoint pos)
{
    foreach (QRect* rect, *rects) {
        if ((rect->center() - pos).manhattanLength() < 25)
            return rect;
    }
    return NULL;
}
