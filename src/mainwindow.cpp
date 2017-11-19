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
