#include "introducewindow.h"
#include "ui_introducewindow.h"

IntroduceWindow::IntroduceWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::IntroduceWindow)
{
    ui->setupUi(this);
    // Emit the signal that returns to the main window
    connect(ui->mainbtn,&QPushButton::clicked,[=](){
        emit this->returnmain();
    });
    // Set the window icon
    QIcon icon(":/new/Resources/head.ico");
    this->setWindowIcon(icon);

}

IntroduceWindow::~IntroduceWindow()
{
    delete ui;

}
