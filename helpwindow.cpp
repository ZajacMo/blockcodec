#include "helpwindow.h"
#include "ui_helpwindow.h"

HelpWindow::HelpWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::HelpWindow)
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
    // Emit the signal that opens the compression window
    connect(ui->compressbtn,&QPushButton::clicked,[=](){
        emit this->tocompress();
    });
    // Set the window icon
    QIcon icon(":/new/Resources/head.ico");
    this->setWindowIcon(icon);

}

HelpWindow::~HelpWindow()
{
    delete ui;
}
