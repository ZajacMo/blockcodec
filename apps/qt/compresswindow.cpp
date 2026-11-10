#include "compresswindow.h"
#include "ui_compresswindow.h"
#include "imagedisplay.h"

#include <QPushButton>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QTemporaryFile>
#include <operatorsignals.h>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <fstream>
#include <stdexcept>

namespace {

// Writes the container to `path`. Throws std::runtime_error on failure.
void write_file(const QString& path, const std::vector<std::uint8_t>& data) {
    std::ofstream out(path.toStdString(), std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("cannot open the file for writing");
    }
    if (!data.empty()) {
        out.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    }
    if (!out) {
        throw std::runtime_error("failed to write the file");
    }
}

}  // namespace

CompressWindow::CompressWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CompressWindow)
{
    ui->setupUi(this);


    // Emit the signal that returns to the main window
    connect(ui->mainbtn,&QPushButton::clicked,[=](){
        emit this->returnmain();
    });
    // Emit the signal that opens the decompression window
    connect(ui->unbtn,&QPushButton::clicked,[=](){
        emit this->touncompress();
    });
    // Emit the signal that opens the help window
    connect(ui->helpbtn,&QPushButton::clicked,[=](){
        emit this->tohelp();
    });
    // Set the window icon
    QIcon icon(":/new/Resources/head.ico");
    this->setWindowIcon(icon);
}

CompressWindow::~CompressWindow()
{
    delete ui;
}
void CompressWindow::resizeEvent(QResizeEvent* event)/* Slot that rescales the preview image */
{
    m_img2show = m_imgSrc.scaled(ui->firstphoto->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);

    m_pix2show=QPixmap::fromImage(m_img2show);

    ui->firstphoto->setPixmap(m_pix2show);
}



void CompressWindow::on_runbtn_clicked()
{
    if(strFileName.isEmpty())
    {
        QMessageBox::warning(this,"Error","Please choose a photo first!");
        return;
    }
    sigma = ui->doubleSpinBox->value();
    // Load the source image. The image is read with its own channel count,
    // so a colour picture keeps its three channels and a greyscale picture
    // keeps one. The old implementation read the channels into the width,
    // which made a colour picture unreadable.
    cv::Mat source = cv::imread(strFileName.toStdString(), cv::IMREAD_UNCHANGED);
    if (source.empty())
    {
        QMessageBox::warning(this,"Error","Cannot read the selected image file.");
        return;
    }
    if (source.depth() != CV_8U)
    {
        QMessageBox::warning(this,"Error","Only 8-bit images are supported.");
        return;
    }
    // The buttons say what they do: the greyscale button drops the colour
    // channels, the colour button keeps them.
    if (!isColorful && source.channels() > 1)
    {
        cv::Mat grey;
        if (source.channels() == 2) {
            // Grey and alpha. The alpha channel is dropped, not averaged.
            cv::extractChannel(source, grey, 0);
        } else {
            cv::cvtColor(source, grey, source.channels() == 4 ? cv::COLOR_BGRA2GRAY
                                                              : cv::COLOR_BGR2GRAY);
        }
        source = grey;
    }

    // The codec is a small value type, so it is used where it is needed
    // rather than held on the heap.
    blockcodec::Codec codec({static_cast<int>(sigma)});
    blockcodec::CompressResult result;
    std::vector<std::uint8_t> container;
    try {
        container = codec.compress(source, &result);
    } catch (const std::exception& e) {
        QMessageBox::warning(this,"Error",QString("Cannot compress this image: %1").arg(e.what()));
        return;
    }

    m_container = std::move(container);
    m_result = result;
    m_source = source;

    // The size of the raw image in bytes, with its dimensions. It is the
    // quantity the compression ratio is measured against, so it is
    // directly comparable with the size of the compressed file.
    const int channels = source.channels();
    const qint64 raw_bytes = static_cast<qint64>(source.rows) * source.cols * channels;
    QString size = QString::number(raw_bytes)+" ("+QString::number(source.rows)+"*";
    size += QString::number(source.cols);
    if(channels > 1)
        size += "*"+QString::number(channels);
    size += ")";
    ui->formerSize->setText(size);
    // Show the compression time
    ui->compressTime->setText(QString::number(result.compress_ms/1000.0,'g',6)+" s");
    // The size of the file that is written. It is the size of the whole
    // container, so it includes the header.
    ui->newSize->setText(QString::number(result.byte_size)+" Bytes");
    // The bits per pixel of the bitstream. This is a different measurement
    // from the size of the file: it counts the bits of the payload only.
    ui->compressRate->setText(QString::number(result.bpp,'g',6)+" bits/pixel");
    // The compression ratio of the bitstream against the raw image.
    ui->compressRatio->setText(QString::number(result.cr,'g',6)+" : 1");
}

void CompressWindow::on_savebtn_clicked()
{
    if(m_container.empty())
    {
        QMessageBox::warning(this,"Error","Please compress first!");
        return;
    }
    savePath = QFileDialog::getSaveFileName(this,tr("Save Image"), "Desktop/result.bcc",
                                            tr("Compressed images (*.bcc)"));
    if (savePath.isEmpty())
        return;
    try {
        write_file(savePath, m_container);
    } catch (const std::exception& e) {
        QMessageBox::warning(this,"Error",QString("Cannot save the compressed file: %1").arg(e.what()));
    }
}


void CompressWindow::on_colorfulphoto_clicked()
{
    isColorful=1;
}


void CompressWindow::on_blackphoto_clicked()
{
    isColorful=0;
}

    // Choose the image to compress
void CompressWindow::on_choosebtn_clicked()
{
    strFileName = QFileDialog::getOpenFileName(this,tr("Open Image"), "lena512.bmp", tr("Image Files (*.png *.jpg *.bmp)"));
    m_imgSrc.load(strFileName);
    m_img2show = m_imgSrc.scaled(ui->firstphoto->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);

    m_pix2show=QPixmap::fromImage(m_img2show);

    ui->firstphoto->setPixmap(m_pix2show);

    ui->secondlphoto->clear();

}

// Decompress and show the result
void CompressWindow::on_uncompressBtn_clicked()
{
    if(strFileName.isEmpty())
    {
        QMessageBox::warning(this,"Error","Please choose a photo first!");
        return;
    }
    if(m_container.empty())
    {
        QMessageBox::warning(this,"Error","Please compress first!");
        return;
    }

    // Round-trip through a temporary file rather than a fixed name in the
    // working directory, which would collide with the user's own files.
    QTemporaryFile tmp(QDir::tempPath() + "/openCV_test_XXXXXX.bcc");
    if (!tmp.open())
    {
        QMessageBox::warning(this,"Error","Cannot create a temporary file.");
        return;
    }
    const QString tmpPath = tmp.fileName();
    tmp.close();

    try {
        write_file(tmpPath, m_container);

        blockcodec::Codec codec;
        blockcodec::CompressResult result;
        const cv::Mat decoded = codec.decompress(tmpPath.toStdString(), &result);

        m_img2show = gui::to_qimage(decoded);
        m_img2show = m_img2show.scaled(ui->secondlphoto->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);

        m_pix2show=QPixmap::fromImage(m_img2show);
        ui->secondlphoto->setPixmap(m_pix2show);

        // The quality of the round trip is measured against the image that
        // was encoded.
        const double quality = blockcodec::psnr(m_source, decoded);
        ui->PSNR->setText("PSNR: "+(QString::number(quality,'g',6)+" dB"));
    } catch (const std::exception& e) {
        QMessageBox::warning(this,"Error",QString("Cannot decompress: %1").arg(e.what()));
    }
}

