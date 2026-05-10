#include "imageviewer.h"
#include <QPaintEvent>
#include <QPainter>
ImageViewer::ImageViewer(QWidget *parent) : QOpenGLWidget(parent) {}

ImageViewer::~ImageViewer() {}

void ImageViewer::set_image(QImage image) {
  this->has_image = true;
  this->image = image;
  this->update();
}

void ImageViewer::paintEvent(QPaintEvent *event) {
  QPainter painter;
  painter.begin(this);
  painter.fillRect(event->rect(), Qt::black);
  painter.setRenderHint(QPainter::Antialiasing);
  if (has_image) {
    painter.drawImage(event->rect(), image);
  }
  painter.end();
}
