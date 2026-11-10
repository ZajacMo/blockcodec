#ifndef SAVE_H
#define SAVE_H

#include <QMainWindow>

namespace Ui {
class save;
}

class save : public QMainWindow
{
    Q_OBJECT

public:
    explicit save(QWidget *parent = nullptr);
    ~save();

private:
    Ui::save *ui;
};

#endif // SAVE_H
