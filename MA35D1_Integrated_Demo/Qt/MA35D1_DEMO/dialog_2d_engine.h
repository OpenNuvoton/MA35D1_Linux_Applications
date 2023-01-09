#ifndef DIALOG_2D_ENGINE_H
#define DIALOG_2D_ENGINE_H

#include <QDialog>
#include <QMouseEvent>
#include <QProcess>

namespace Ui {
class Dialog_2d_engine;
}

class Dialog_2d_engine : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog_2d_engine(QWidget *parent = nullptr);
    ~Dialog_2d_engine();

private:
    Ui::Dialog_2d_engine *ui;
    QProcess *p_alphablending;

private slots:
    void mousePressEvent(QMouseEvent *event);
};

#endif // DIALOG_2D_ENGINE_H
