#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QDir>

#include "imagetransform.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_pushButton_play_pause_clicked();

    void on_spinBox_fps_valueChanged(int arg1);

    void slot_timer_fired();

    void on_actionOpen_folder_triggered();

private:
    Ui::MainWindow *ui;
    QTimer timer;
    QDir dir;
    QStringList files;
    int currentPosition;
    ImageTransform imageTransform;

    void processImage(QImage *image);
};

#endif // MAINWINDOW_H