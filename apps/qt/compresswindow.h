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

#include "blockcodec/codec.hpp"

#include <opencv2/core.hpp>

#include <cstdint>
#include <vector>

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
    // The encoded container. It is kept so that the image does not have to
    // be encoded a second time to be saved or to be previewed.
    std::vector<std::uint8_t> m_container;
    // The measurements of the last encoding.
    blockcodec::CompressResult m_result;
    // The image that was encoded, kept so that the round trip can be scored.
    cv::Mat m_source;
    // Maximum tolerated deviation
    double sigma = 10;
    // 1 = colour, 0 = greyscale
    int isColorful = 0;
};

#endif // COMPRESSWINDOW_H
