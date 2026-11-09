#ifndef COMPRESSWINDOW_H
#define COMPRESSWINDOW_H

#include <QMainWindow>
#include<QMenuBar>
#include<QMenu>
#include<QAction>
#include<QFileDialog>
#include<QMessageBox>
#include<QString>
#include<QTextEdit>
#include<QFile>
#include<QTextStream>
#include<QFileDialog>
#include <QAbstractButton>
#include "Compress_gray.h"


namespace Ui {
class CompressWindow;
}

class CompressWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CompressWindow(QWidget *parent = nullptr);
    virtual void resizeEvent(QResizeEvent* event);/* Slot that rescales the preview image */
    ~CompressWindow();

signals:
    void returnmain();/* Emitted to return to the main window */
    void touncompress();/* Emitted to open the decompression window */
    void tohelp();/* Emitted to open the help window */

private slots:
    void on_runbtn_clicked();

    void on_savebtn_clicked();

    void on_colorfulphoto_clicked();

    void on_blackphoto_clicked();

    void on_choosebtn_clicked();

    void on_uncompressBtn_clicked();

private:
    Ui::CompressWindow *ui;
    QImage m_imgSrc;
    QImage m_img2show;
    QPixmap m_pix2show;
    QString strFileName;
    QString savePath;
    Compress_gray* cg = nullptr;
    // Maximum tolerated deviation
    double sigma = 10;
    // 1 = colour, 0 = greyscale
    int isColorful = 0;
};

#endif // COMPRESSWINDOW_H
