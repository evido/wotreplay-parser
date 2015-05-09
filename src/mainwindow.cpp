#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "parser.h"
#include "image_writer.h"
#include <fstream>
#include <QFileDialog>
#include <QImage>
using namespace wotreplay;

class texture_writer_t : public image_writer_t {
public:
    const boost::multi_array<uint8_t, 3> &get_result() const { return result; }
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

static std::unique_ptr<texture_writer_t> create_writer(const std::string &path) {
	std::ifstream in(path, std::ios::binary);
	parser_t parser;

	game_t game;
	parser.parse(in, game);

	std::unique_ptr<texture_writer_t> writer(new texture_writer_t());
	writer->set_image_height(512);
	writer->set_image_width(512);
	writer->init(game.get_arena(), game.get_game_mode());
	writer->set_no_basemap(false);
	writer->set_show_self(true);
	writer->update(game);
	writer->finish();

	return std::move(writer);
}

void MainWindow::on_showButton_clicked()
{
	std::string input(this->ui->inputTextBox->text().toUtf8().constData());
	std::unique_ptr<texture_writer_t> writer(create_writer(input));

    auto data = writer->get_result();
    auto dim = data.shape();
    if (!image_data) image_data.reset(new uint32_t[dim[0]*dim[1]]);
    for (size_t i = 0; i < dim[0]; ++i) {
        for (size_t j = 0; j < dim[1]; ++j) {
            uint32_t color = qRgba(data[i][j][0], data[i][j][1], data[i][j][2], data[i][j][3]);
            image_data[i*dim[1] + j] = color;
        }
    }

    QImage image((const uchar*) image_data.get(), dim[0], dim[1], dim[1] * sizeof(uint32_t), QImage::Format_ARGB32);
    this->ui->openGLWidget->set_image(image);
}

void MainWindow::on_browseInputButton_clicked()
{
    QFileDialog dialog(this);
    dialog.setNameFilter(tr("Replay Files (*.wotreplay)"));
    if (dialog.exec()) {
        if (dialog.selectedFiles().length() > 0) {
            this->ui->inputTextBox->setText(dialog.selectedFiles()[0]);
        }
    }
}

void MainWindow::on_generateButton_clicked()
{
	std::string input(this->ui->inputTextBox->text().toUtf8().constData());
	std::string output(this->ui->outputTextBox->text().toUtf8().constData());
	std::ofstream out(output, std::ios::binary);
	std::unique_ptr<texture_writer_t> writer(create_writer(input));
	writer->write(out);
}

void MainWindow::on_browseOutputButton_clicked()
{
	QFileDialog dialog(this);
	dialog.setNameFilter(tr("PNG (*.png)"));
	dialog.setAcceptMode(QFileDialog::AcceptSave);
	if (dialog.exec()) {
		if (dialog.selectedFiles().length() > 0) {
			this->ui->outputTextBox->setText(dialog.selectedFiles()[0]);
			this->ui->generateButton->setEnabled(true);
		}
	}
}
