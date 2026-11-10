#include "mainwindow.h"
#include "ui_mainwindow.h"
#include<QPushButton>
#include<opencv2/opencv.hpp>
#include<QFileDialog>
#include"compresswindow.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->pcompress=new CompressWindow;
    // Switch to the compression window
    connect(ui->compressbtn,&QPushButton::clicked,[=](){
        this->hide();
        this->pcompress->show();
    });
    // Switch back to the main window
    connect(this->pcompress,&CompressWindow::returnmain,[=](){
        this->pcompress->hide();
        this->show();
    });
    // Switch to the decompression window
    connect(this->pcompress,&CompressWindow::touncompress,[=](){
        this->pcompress->hide();
        this->puncompress->show();
    });
    // Switch to the help window
    connect(this->pcompress,&CompressWindow::tohelp,[=](){
        this->pcompress->hide();
        this->phelp->show();
    });
    this->phelp=new HelpWindow;
    // Switch to the help window
    connect(ui->helpbtn,&QPushButton::clicked,[=](){
        this->hide();
        this->phelp->show();
    });
    // Switch back to the main window
    connect(this->phelp,&HelpWindow::returnmain,[=](){
        this->phelp->hide();
        this->show();
    });
    // Switch to the decompression window
    connect(this->phelp,&HelpWindow::touncompress,[=](){
        this->phelp->hide();
        this->puncompress->show();
    });
    // Switch to the compression window
    connect(this->phelp,&HelpWindow::tocompress,[=](){
        this->phelp->hide();
        this->pcompress->show();
    });
    this->pintroduce=new IntroduceWindow;
    // Switch to the introduction window
    connect(ui->introducebtn,&QPushButton::clicked,[=](){
        this->hide();
        this->pintroduce->show();
    });
    // Switch back to the main window
    connect(this->pintroduce,&IntroduceWindow::returnmain,[=](){
        this->pintroduce->hide();
        this->show();
    });
    this->puncompress=new uncompressWindow;
    // Switch to the decompression window
    connect(ui->uncompressbtn,&QPushButton::clicked,[=](){
        this->hide();
        this->puncompress->show();
    });
    // Switch back to the main window
    connect(this->puncompress,&uncompressWindow::returnmain,[=](){
        this->puncompress->hide();
        this->show();
    });
    // Switch to the help window
    connect(this->puncompress,&uncompressWindow::tohelp,[=](){
        this->puncompress->hide();
        this->phelp->show();
    });
    // Switch to the compression window
    connect(this->puncompress,&uncompressWindow::tocompress,[=](){
        this->puncompress->hide();
        this->pcompress->show();
    });
    // Set the window icon
    QIcon icon(":/new/Resources/head.ico");
    this->setWindowIcon(icon);


}

MainWindow::~MainWindow()
{
    // The sub-windows are top-level windows, so they are not owned by
    // this object and have to be destroyed explicitly.
    delete pcompress;
    delete puncompress;
    delete phelp;
    delete pintroduce;
    delete ui;
}
