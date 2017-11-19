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

    delete median;
    delete hpfImage;

    QImage* logoMask = new QImage(image->size(), QImage::Format_ARGB32);
    logoMask->fill(QColor (0, 0, 0, 0));

    QPainter painter(logoMask);
    QPen pen;
    pen.setColor(Qt::red);
    pen.setWidth(3);
    painter.setPen(Qt::red);

    foreach(QRect* rect, *rectsOfInterest) {
        rect->adjust(-3, -3, 3, 3);
        painter.drawImage(*rect, *image, *rect);
        painter.drawRect(*rect);
    }

    painter.end();

    ui->label_logoMask->setPixmap(QPixmap::fromImage(*logoMask));

    delete logoMask;
    foreach(QRect* rect, *rectsOfInterest) {
        delete rect;
    }
    rectsOfInterest->clear();
    delete rectsOfInterest;
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

    ui->treeWidget_trainingImages->resizeColumnToContents(0);
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

QPoint MainWindow::findImage(QImage big, QImage small)
{
    QPoint bestPosition;
    double bestBonus = 0;
    double currentBonus = 0.0;

    int bigSizeX = big.size().width();
    int bigSizeY = big.size().height();
    int smallSizeX = small.size().width();
    int smallSizeY = small.size().height();

    int offsetXmax = bigSizeX - smallSizeX;
    int offsetYmax = bigSizeY - smallSizeY;

    for (int offsetY = 0; offsetY <= offsetYmax; offsetY++) {
        for (int offsetX = 0; offsetX < offsetXmax; offsetX++) {
            currentBonus = 0.0;
            QImage cutout = big.copy(offsetX, offsetY, smallSizeX, smallSizeY);

            for (int x = 0; x < smallSizeX; x++) {
                for (int y = 0; y < smallSizeY; y++) {
                    currentBonus += cutout.pixelColor(x, y).redF() * small.pixelColor(x, y).redF();
                }
            }

            if (currentBonus > bestBonus) {
                bestBonus = currentBonus;
                bestPosition = QPoint(offsetX, offsetY);
            }

        }
    }

    return bestPosition;
}
