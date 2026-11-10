#ifndef UNCOMPRESSWINDOW_H
#define UNCOMPRESSWINDOW_H

#include <QMainWindow>
#include<QFile>
#include<QTextStream>
#include<QFileDialog>

#include "blockcodec/codec.hpp"

#include <opencv2/core.hpp>

namespace Ui {
class uncompressWindow;
}

class uncompressWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit uncompressWindow(QWidget *parent = nullptr);
    void choose();/* Slot that opens the compressed document */
    ~uncompressWindow();

signals:
    void returnmain();/* Emitted to return to the main window */
    void tohelp();/* Emitted to open the help window */
    void tocompress();/* Emitted to open the compression window */

private slots:
    void on_choosebtn_clicked();

    void on_savebtn_clicked();

    void on_runbtn_clicked();

private:
    Ui::uncompressWindow *ui;
    QString fileName;
    // The decoded image, held so that it can be saved without being
    // decoded a second time.
    cv::Mat m_decoded;
};

#endif // UNCOMPRESSWINDOW_H
