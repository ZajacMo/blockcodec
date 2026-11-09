#include "compresswindow.h"
#include "ui_compresswindow.h"
#include <QPushButton>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QTemporaryFile>
#include <operatorsignals.h>

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
    cg = new Compress_gray;

}

CompressWindow::~CompressWindow()
{
    delete ui;
    delete cg;
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
    // Load the source image. cv::imread returns an empty matrix when it
    // fails, so check the result before handing it to the codec.
    cv::Mat source = cv::imread(strFileName.toStdString(), isColorful ? 1 : 0);
    if (source.empty())
    {
        QMessageBox::warning(this,"Error","Cannot read the selected image file.");
        return;
    }
    if(cg)
        delete cg;
    cg = new Compress_gray;
    try {
        cg->init_compress(source,sigma);
    } catch (const std::exception& e) {
        QMessageBox::warning(this,"Error",QString("Cannot compress this image: %1").arg(e.what()));
        delete cg;
        cg = nullptr;
        return;
    }
    // Dimensions of the source image
    QString size = QString::number(cg->img.rows)+"*"+QString::number(cg->img.cols);
    if(isColorful)
        size += "*3";
    size += "Bytes";
    ui->formerSize->setText(size);
    // Run the encoder
    cg->compress();
    // Show the compression ratio
    ui->compressRate->setText(QString::number(cg->getRate()));
    // Show the compression time
    ui->compressTime->setText(QString::number(cg->getTime(),'g',6)+" s");
    // Show the compressed size
    ui->newSize->setText(QString::number(cg->getWriteLen()/8)+" Bytes");
}


void CompressWindow::on_savebtn_clicked()
{
    if(!cg||cg->isNull)
    {
        QMessageBox::warning(this,"Error","Please compress first!");
        return;
    }
    savePath = QFileDialog::getSaveFileName(this,tr("Save Image"), "Desktop/result.nam");
    if (savePath.isEmpty())
        return;
    try {
        cg->save(savePath.toStdString());
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
    if(!cg || cg->isNull)
    {
        QMessageBox::warning(this,"Error","Please compress first!");
        return;
    }

    // Round-trip through a temporary file rather than a fixed name in the
    // working directory, which would collide with the user's own files.
    QTemporaryFile tmp(QDir::tempPath() + "/openCV_test_XXXXXX.nam");
    if (!tmp.open())
    {
        QMessageBox::warning(this,"Error","Cannot create a temporary file.");
        return;
    }
    const QString tmpPath = tmp.fileName();
    tmp.close();

    try {
        cg->save(tmpPath.toStdString());

        Compress_gray ucg;
        ucg.init_uncompress(tmpPath.toStdString());
        cv::Mat result = ucg.uncompress();

        m_img2show = QImage((const unsigned char*)(result.data),result.cols,result.rows,result.cols,QImage::Format_Grayscale8);
        m_img2show = m_img2show.scaled(ui->secondlphoto->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);

        m_pix2show=QPixmap::fromImage(m_img2show);
        ui->secondlphoto->setPixmap(m_pix2show);

        ui->PSNR->setText("PSNR: "+QString::number(Compress_gray::getPSNR(cg->img,result)));
    } catch (const std::exception& e) {
        QMessageBox::warning(this,"Error",QString("Cannot decompress: %1").arg(e.what()));
    }
}

