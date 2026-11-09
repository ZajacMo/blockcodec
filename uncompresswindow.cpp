#include "uncompresswindow.h"
#include "ui_uncompresswindow.h"
#include <qmessagebox.h>
#include <QDir>

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
    if(ucg)
        delete ucg;
}

// Choose the compressed file to decode
void uncompressWindow::on_choosebtn_clicked()
{
    fileName = QFileDialog::getOpenFileName(this, "Select the file to decode",
                                                    QDir::homePath(),
                                                    tr("Compressed images (*.nam)"));
    if (fileName.isEmpty())
        return;
    ui->lineEdit->setText(fileName);

    if(ucg)
        delete ucg;
    ucg = new Compress_gray();
    try {
        ucg->init_uncompress(fileName.toStdString());
    } catch (const std::exception& e) {
        QMessageBox::warning(this,"Error",QString("Cannot open the compressed file: %1").arg(e.what()));
        delete ucg;
        ucg = nullptr;
        return;
    }
    ui->formerSize->setText(QString::number(ucg->getReadLen()/8)+" Bytes");
}


// Save the decompressed image
void uncompressWindow::on_savebtn_clicked()
{
    if(!ucg || ucg->getReadLen() == 0)
    {
        QMessageBox::warning(this,"Error","Please choose a file first!");
        return;
    }
    QString savePath = QFileDialog::getSaveFileName(this,tr("Save Image"), "Desktop/uncompressedIMG.bmp");
    if (savePath.isEmpty())
        return;
    if (!cv::imwrite(savePath.toStdString(), ucg->result))
        QMessageBox::warning(this,"Error","Cannot write the image file.");
}


void uncompressWindow::on_runbtn_clicked()
{
    if(fileName.isEmpty() || !ucg){
        QMessageBox::warning(this,"Error","Please choose a file first!");
        return;
    }
    cv::Mat res;
    try {
        res = ucg->uncompress();
    } catch (const std::exception& e) {
        QMessageBox::warning(this,"Error",QString("Cannot decompress: %1").arg(e.what()));
        return;
    }
    ui->uncompressTime->setText(QString::number(ucg->getTime(),'g',6)+" s");
    QString size = QString::number(res.rows)+"*"+QString::number(res.cols);
    if(ucg->isColorful)
        size += "*3";
    size += " Bytes";
    ui->newSize->setText(size);

    QImage m_imgSrc = QImage((const unsigned char*)(res.data),res.cols,res.rows,res.cols,QImage::Format_Grayscale8);

    QImage m_img2show = m_imgSrc.scaled(ui->photo->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);

    QPixmap m_pix2show=QPixmap::fromImage(m_img2show);

    ui->photo->setPixmap(m_pix2show);

    ui->rate->setText(QString::number(ucg->getRate(),'g',6));
}

