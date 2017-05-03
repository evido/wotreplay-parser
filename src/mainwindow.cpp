#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "parser.h"
#include "image_writer.h"
#include "heatmap_writer.h"
#include "class_heatmap_writer.h"
#include <fstream>
#include <QFileDialog>
#include <QImage>
using namespace wotreplay;


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

	QStringList list = (QStringList() << "png" << "heatmap" << "team-heatmap" << "class-heatmap");
	ui->typeComboBox->addItems(list);
}

MainWindow::~MainWindow()
{
    delete ui;
}

std::unique_ptr<image_writer_t> MainWindow::create_writer(const std::string &path) const {
	std::ifstream in(path, std::ios::binary);
	parser_t parser;

	game_t game;
	parser.parse(in, game);

	std::string type = ui->typeComboBox->currentText().toUtf8().constData();

	std::unique_ptr<image_writer_t> writer;

	if (type == "png") {
		writer.reset(new image_writer_t());
		writer->set_show_self(true);
	}
	else if (type == "heatmap" || type == "team-heatmap") {
		writer.reset(new heatmap_writer_t());
		heatmap_writer_t *_specific = dynamic_cast<heatmap_writer_t*>(writer.get());
		_specific->mode = type == "team-heatmap" ? heatmap_mode_t::team : heatmap_mode_t::combined;
		float lowerBound = ui->lowerBoundSlider->value() / (ui->lowerBoundSlider->maximum() * 1.f);
		float upperBound = ui->upperBoundSlider->value() / (ui->upperBoundSlider->maximum() * 1.f);
		_specific->bounds = std::make_tuple(lowerBound, upperBound);
	}
	else if (type == "class-heatmap") {
		writer.reset(new class_heatmap_writer_t());
		class_heatmap_writer_t *_specific = dynamic_cast<class_heatmap_writer_t*>(writer.get());
		float lowerBound = ui->lowerBoundSlider->value() / (ui->lowerBoundSlider->maximum() * 1.f);
		float upperBound = ui->upperBoundSlider->value() / (ui->upperBoundSlider->maximum() * 1.f);
		_specific->bounds = std::make_tuple(lowerBound, upperBound);
		std::vector<draw_rule_t> rules = parse_draw_rules(ui->rulesTextEdit->toPlainText().toUtf8().constData());
		_specific->set_draw_rules(rules);
	}
	
	writer->set_no_basemap(ui->overlayCheckBox->isChecked());
	writer->set_image_height(512);
	writer->set_image_width(512);
	writer->init(game.get_arena(), game.get_game_mode());
	writer->update(game);
	writer->finish();

	return std::move(writer);
}

void MainWindow::on_showButton_clicked()
{
	std::string input(this->ui->inputTextBox->text().toUtf8().constData());
	std::unique_ptr<image_writer_t> writer(create_writer(input));

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
	QStringList filters;
	filters 
		<< tr("World of Tanks Replays (*.wotreplay)")
		<< tr("World of Warships Replays (*.wowsreplay)")
		<< tr("Any files (*.*)");
	dialog.setNameFilters(filters);
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
	std::unique_ptr<image_writer_t> writer(create_writer(input));
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

void MainWindow::on_typeComboBox_currentTextChanged(const QString &arg1)
{
	std::string type = arg1.toUtf8().constData();

	if (type == "png") {
		ui->lowerBoundSlider->setEnabled(false);
		ui->upperBoundSlider->setEnabled(false);
		ui->rulesTextEdit->setEnabled(false);
	}
	
	if (type == "heatmap" || type == "team-heatmap") {
		ui->lowerBoundSlider->setEnabled(true);
		ui->upperBoundSlider->setEnabled(true);
		ui->rulesTextEdit->setEnabled(false);
	}

	if (type == "class-heatmap") {
		ui->lowerBoundSlider->setEnabled(true);
		ui->upperBoundSlider->setEnabled(true);
		ui->rulesTextEdit->setEnabled(true);

		if (ui->rulesTextEdit->toPlainText().isEmpty()) {
			std::string defaultRules = "#ff0000 := team = '1'; #00ff00 := team = '0'";
			ui->rulesTextEdit->setPlainText(QString::fromUtf8(defaultRules.c_str()));
		}
	}
}
