#ifndef HELPWINDOW_H
#define HELPWINDOW_H

#include <QMainWindow>


namespace Ui {
class HelpWindow;
}

class HelpWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit HelpWindow(QWidget *parent = nullptr);
    ~HelpWindow();

signals:
    void returnmain();/* Emitted to return to the main window */
    void touncompress();/* Emitted to open the decompression window */
    void tocompress();/* Emitted to open the compression window */


private:
    Ui::HelpWindow *ui;
};

#endif // HELPWINDOW_H
