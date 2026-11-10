#include "uncompresswindow.h"
#include "ui_uncompresswindow.h"
#include "imagedisplay.h"

#include <qmessagebox.h>
#include <QDir>
#include <QFileInfo>

#include <opencv2/imgcodecs.hpp>

#include <fstream>
#include <iterator>
#include <vector>

#include "blockcodec/format.hpp"

namespace {

// Checks that the file at `path` holds a container.
// Throws std::runtime_error on failure.
void check_container_file(const QString& path) {
    std::ifstream in(path.toStdString(), std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open the file for reading");
    }
    const std::vector<std::uint8_t> container((std::istreambuf_iterator<char>(in)),
                                              std::istreambuf_iterator<char>());
    std::vector<std::uint8_t> payload;
    blockcodec::read_container(container, payload);
}

}  // namespace

uncompressWindow::uncompressWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::uncompressWindow)
{
    ui->setupUi(this);
    // Emit the signal that returns to the main window
    connect(ui->mainbtn,&QPushButton::clicked,[=](){
        emit this->returnmain();
    });
    // Emit the signal that opens the help window
    connect(ui->helpbtn,&QPushButton::clicked,[=](){
        emit this->tohelp();
    });
    // Emit the signal that opens the compression window
    connect(ui->compressbtn,&QPushButton::clicked,[=](){
        emit this->tocompress();
    });
    QIcon icon(":/new/Resources/head.ico");
    this->setWindowIcon(icon);
}

uncompressWindow::~uncompressWindow()
{
    delete ui;
}

// Choose the compressed file to decode
void uncompressWindow::on_choosebtn_clicked()
{
    fileName = QFileDialog::getOpenFileName(this, "Select the file to decode",
                                                    QDir::homePath(),
                                                    tr("Compressed images (*.bcc)"));
    if (fileName.isEmpty())
        return;
    ui->lineEdit->setText(fileName);

    // The container is checked here, so that a file that is not one is
    // reported before the user asks for the image.
    try {
        check_container_file(fileName);
    } catch (const std::exception& e) {
        QMessageBox::warning(this,"Error",QString("Cannot open the compressed file: %1").arg(e.what()));
        m_decoded.release();
        ui->formerSize->clear();
        return;
    }
    ui->formerSize->setText(QString::number(QFileInfo(fileName).size())+" Bytes");
}


// Save the decompressed image
void uncompressWindow::on_savebtn_clicked()
{
    if(m_decoded.empty())
    {
        QMessageBox::warning(this,"Error","Please choose a file first!");
        return;
    }
    QString savePath = QFileDialog::getSaveFileName(this,tr("Save Image"), "Desktop/uncompressedIMG.bmp");
    if (savePath.isEmpty())
        return;
    if (!cv::imwrite(savePath.toStdString(), m_decoded))
        QMessageBox::warning(this,"Error","Cannot write the image file.");
}


void uncompressWindow::on_runbtn_clicked()
{
    if(fileName.isEmpty())
    {
        QMessageBox::warning(this,"Error","Please choose a file first!");
        return;
    }
    blockcodec::Codec codec;
    blockcodec::CompressResult result;
    cv::Mat res;
    try {
        res = codec.decompress(fileName.toStdString(), &result);
    } catch (const std::exception& e) {
        QMessageBox::warning(this,"Error",QString("Cannot decompress: %1").arg(e.what()));
        m_decoded.release();
        return;
    }
    m_decoded = res;

    // The size of the file that was decoded. It is shown here as well as
    // when the file is chosen, so that the panel describes the file that
    // was actually read.
    ui->formerSize->setText(QString::number(QFileInfo(fileName).size())+" Bytes");

    ui->uncompressTime->setText(QString::number(result.decompress_ms/1000.0,'g',6)+" s");
    // The size of the decoded image in bytes, with its dimensions. It is
    // directly comparable with the size of the file it was decoded from.
    const int channels = res.channels();
    const qint64 raw_bytes = static_cast<qint64>(res.rows) * res.cols * channels;
    QString size = QString::number(raw_bytes)+" ("+QString::number(res.rows)+"*";
    size += QString::number(res.cols);
    if(channels > 1)
        size += "*"+QString::number(channels);
    size += ")";
    ui->newSize->setText(size);

    QImage m_imgSrc = gui::to_qimage(res);

    QImage m_img2show = m_imgSrc.scaled(ui->photo->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);

    QPixmap m_pix2show=QPixmap::fromImage(m_img2show);

    ui->photo->setPixmap(m_pix2show);

    // The bits per pixel of the bitstream, which is a different measurement
    // from the size of the file.
    ui->rate->setText(QString::number(result.bpp,'g',6)+" bits/pixel");
    ui->compressRatio->setText(QString::number(result.cr,'g',6)+" : 1");
}
