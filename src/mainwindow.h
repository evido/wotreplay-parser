#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include <string>

namespace Ui {
class MainWindow;
}

namespace wotreplay {
	class image_writer_t;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
	std::unique_ptr<wotreplay::image_writer_t> create_writer(const std::string &path) const;

private slots:
    void on_showButton_clicked();

    void on_browseInputButton_clicked();

    void on_generateButton_clicked();

    void on_browseOutputButton_clicked();

    void on_typeComboBox_currentTextChanged(const QString &arg1);

private:
    Ui::MainWindow *ui;
    std::unique_ptr<uint32_t[]> image_data;
	std::unique_ptr<wotreplay::image_writer_t> writer;
};

#endif // MAINWINDOW_H
