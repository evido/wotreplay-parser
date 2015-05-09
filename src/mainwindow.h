#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>

namespace Ui {
class MainWindow;
}

class texture_writer_t;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_showButton_clicked();

    void on_browseInputButton_clicked();

    void on_generateButton_clicked();

    void on_browseOutputButton_clicked();

private:
    Ui::MainWindow *ui;
    std::unique_ptr<uint32_t[]> image_data;
};

#endif // MAINWINDOW_H
