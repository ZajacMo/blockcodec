#ifndef INTRODUCEWINDOW_H
#define INTRODUCEWINDOW_H

#include <QMainWindow>


namespace Ui {
class IntroduceWindow;
}

class IntroduceWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit IntroduceWindow(QWidget *parent = nullptr);
    ~IntroduceWindow();

signals:
    void returnmain();/* Emitted to return to the main window */

private:
    Ui::IntroduceWindow *ui;
};

#endif // INTRODUCEWINDOW_H
