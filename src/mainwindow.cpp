#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QFile>
#include <QDirIterator>
#include <QImage>
#include <QPixmap>
#include <QMessageBox>
#include <QPainter>
#include <QPen>

#include "imagetransform.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    currentPosition = 0;
    //timer.setTimerType(Qt::PreciseTimer);
    timer.setSingleShot(false);
    connect(&timer, SIGNAL(timeout()), this, SLOT(slot_timer_fired()));
    loadReferenceImages();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_play_pause_clicked()
{
    if (ui->pushButton_play_pause->isChecked())
        timer.start(1000 / ui->spinBox_fps->value());
    else
        timer.stop();
}

void MainWindow::on_spinBox_fps_valueChanged(int arg1)
{
    timer.setInterval(1000.0 / arg1);
}

void MainWindow::slot_timer_fired()
{
    QString filename = files.at(currentPosition);
    currentPosition++;
    if (currentPosition >= files.count())
        currentPosition = 0;

    ui->label_filename->setText(filename);

    QImage* image = new QImage(filename);
    if ((image == NULL) || image->isNull())
        return;

    processImage(image);
    delete image;
}

void MainWindow::on_actionOpen_folder_triggered()
{
    timer.stop();
    files.clear();
    ui->pushButton_play_pause->setChecked(false);
    QString dirPath = QFileDialog::getExistingDirectory(this, "Select image directory");

    if (dirPath.isEmpty())
        return;

    dir = QDir(dirPath);

    dir.setNameFilters(QStringList() << "*.bmp" << "*.gif" << "*.jpg" << "*.jpeg" << "*.png" << "*.pbm" << "*.pgm" << "*.ppm" << "*.xbm" << "*.xpm");
    dir.setSorting(QDir::Name);
    QStringList fileList = dir.entryList(QDir::Files);

    for (int i = 0; i<fileList.count(); i++) {
        files.append(dir.absolutePath() + "/" + fileList.at(i));
    }

    ui->label_filename->setText(QString().setNum(files.count()) + " Images selected");
}

void MainWindow::processImage(QImage *image)
{
    ui->label_inputStream->setPixmap(QPixmap::fromImage(*image));
    QImage* hpfImage = ImageTransform::highPassFilter(image);
    ui->label_highPassFilter->setPixmap(QPixmap::fromImage(*hpfImage));

    QImage* median = imageTransform.medianFilter(hpfImage);
    ui->label_medianFilter->setPixmap(QPixmap::fromImage(*median));
    QList<QRect*> *rectsOfInterest = imageTransform.rectsOfInterest(median);

    QImage* logoMask = new QImage(image->size(), QImage::Format_ARGB32);
    logoMask->fill(QColor (0, 0, 0, 0));

    QPainter painter(logoMask);
    QPen pen;
    pen.setColor(Qt::red);
    pen.setWidth(3);
    painter.setPen(Qt::red);

    QFont font;
    font.setPointSize(20);
    painter.setFont(font);

    foreach(QRect* rect, *rectsOfInterest) {
        if ((rect->width() < 10) || (rect->height() < 10)) continue;
        rect->adjust(-1, -1, 1, 1);

        QImage imageToTest = median->copy(*rect);

        painter.drawImage(*rect, imageToTest);
        painter.drawRect(*rect);


        QString filename = findImage(median, *rect);
        if (!filename.isEmpty()) {
            QImage* detectedLogo = new QImage(filename);
            filename = QDir(QFileInfo(filename).absolutePath()).dirName();

            ui->label_detectedStation->setPixmap(QPixmap::fromImage(*detectedLogo));
            delete detectedLogo;
        }
        else
            filename = "";

        if (rect->center().x() < (image->size().width() / 2))
            painter.drawText(rect->adjusted(0, 0, 20, 0).bottomRight(), filename);
        else
            painter.drawText(rect->adjusted(-200, 0, 0, 0).bottomLeft(), filename);
    }

    painter.end();

    ui->label_logoMask->setPixmap(QPixmap::fromImage(*logoMask));

    delete logoMask;
    foreach(QRect* rect, *rectsOfInterest) {
        delete rect;
    }
    rectsOfInterest->clear();
    delete rectsOfInterest;
    delete median;
    delete hpfImage;
}

void MainWindow::addReferenceImage(QString filename, QString stationName)
{
    for (int i=0; i< ui->treeWidget_trainingImages->topLevelItemCount(); i++) {
        if (ui->treeWidget_trainingImages->topLevelItem(i)->text(0) == stationName) {
            QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << "" << filename);

            QImage image;
            bool ok = image.load(filename);
            if (!ok) {
                QMessageBox::warning(this, "Failure", "Image could not be loaded: " + filename);
            }

            item->setIcon(0, QIcon(QPixmap::fromImage(image.scaledToHeight(64, Qt::SmoothTransformation))));

            ui->treeWidget_trainingImages->topLevelItem(i)->addChild(item);
            treeItems.append(item);
            addImageToMap(filename, image);
            return;
        }
    }

    // If program flow reaches this point, we do not have that station yet, so lets create it

    QTreeWidgetItem* topLevelItem = new QTreeWidgetItem(QStringList() << stationName);
    topLevelItem->setData(0, 1001, QVariant(QString("Station")));
    ui->treeWidget_trainingImages->addTopLevelItem(topLevelItem);
    QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << "" << filename);

    QImage image;
    bool ok = image.load(filename);
    if (!ok) {
        QMessageBox::warning(this, "Failure", "Image could not be loaded: " + filename);
    }

    item->setIcon(0, QIcon(QPixmap::fromImage(image.scaledToHeight(64, Qt::SmoothTransformation))));
    topLevelItem->addChild(item);
    treeItems.append(item);
    addImageToMap(filename, image);

    ui->treeWidget_trainingImages->resizeColumnToContents(0);
}

void MainWindow::addImageToMap(QString filename, QImage image) {

    QList<QImage*> list;
    for (int s = 10; s < 50; s++) {
        QString filename_s = filename + QString().setNum(s) + ".sjpg";
        QImage* small;
        if (QFile(filename_s).exists()) {
            small = new QImage(filename_s);
        } else {
            QImage* tmp2 = imageTransform.highPassFilter(&image);
            small = new QImage(tmp2->scaledToHeight(s, Qt::SmoothTransformation));
            delete tmp2;
            small->save(filename_s, "JPG", 100);
        }
        list.append(small);
    }

    referenceMap.insert(filename, list);
}

void MainWindow::loadReferenceImages()
{
    QDir dir;
    dir.setCurrent("etc/references");

    QStringList dirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    foreach(QString dirPath, dirs) {


        QDir innerDir = QDir(dirPath);


        if (!innerDir.exists()) {
            QMessageBox::warning(this, "Warning", "Dir does not exist: " + innerDir.absolutePath());
            continue;
        }

        innerDir.setNameFilters(QStringList() << "*.bmp" << "*.gif" << "*.jpg" << "*.jpeg" << "*.png" << "*.pbm" << "*.pgm" << "*.ppm" << "*.xbm" << "*.xpm");
        QStringList filePaths = innerDir.entryList(QDir::Files);

        foreach (QString filePath, filePaths) {
            filePath.prepend(dirPath + "/");
            addReferenceImage(QFileInfo(filePath).absoluteFilePath(), QDir(dirPath).dirName());
        }
    }
}

void MainWindow::on_treeWidget_trainingImages_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    if (current->data(0, 1001).toString() != "Station") {
        referenceImage.load(current->text(1));
        ui->label_trainingImage->setPixmap(QPixmap::fromImage(referenceImage));
    }
}

QString MainWindow::findImage(QImage* big, QRect searchRect)
{
    return QString();
    double bestBonus = 0.0;
    double secondBestBonus = 0.0;
    double currentBonus = 0.0;


    QString foundFilename;

    foreach (QTreeWidgetItem* item, treeItems) {
        QString filename = item->text(1);
//        QImage *tmp = new QImage(filename);


        for (int s = 20; s < 50; s++) {
            QImage* small = referenceMap.value(filename).at(s - 10);

            int smallSizeX = small->size().width();
            int smallSizeY = small->size().height();

            for (int offsetY = -2; offsetY < 3; offsetY++) {
                for (int offsetX = -2; offsetX < 3; offsetX++) {

                    currentBonus = 0.0;
                    //QImage cutout = big.copy(offsetX, offsetY, smallSizeX, smallSizeY);

//ui->label_detectedStation->setPixmap(QPixmap::fromImage(cutout));
//qApp->processEvents();
                    for (int x = 0; x < smallSizeX; x++) {
                        for (int y = 0; y < smallSizeY; y++) {
                            if ((searchRect.topLeft().x() + x >= big->width()) ||
                                (searchRect.topLeft().y() + y >= big->height()))
                                    continue;
                            if ((big->pixelColor(searchRect.topLeft().x() + x + offsetX, searchRect.topLeft().y() + y + offsetY).redF() > 128) &&
                                small->pixelColor(x, y).redF() > 128)
                                currentBonus += 1.0;
//                            currentBonus += pow(big->pixelColor(searchRect.topLeft().x() + x, searchRect.topLeft().y() + y).redF() * small->pixelColor(x, y).redF(), 3);
                        }
                    }
                    currentBonus /= (smallSizeX * smallSizeY);

                    if (currentBonus >= bestBonus) {
                        secondBestBonus = bestBonus;
                        bestBonus = currentBonus;
                        if (bestBonus >= secondBestBonus + 0) {

                        }
                        foundFilename = filename;
                    }

                }
            }
        }
    }


    return foundFilename;;
}
