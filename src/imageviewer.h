#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QImage>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram);
QT_FORWARD_DECLARE_CLASS(QOpenGLTexture);

class ImageViewer : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

  public:
    explicit ImageViewer(QWidget *parent = 0);
    void set_image(QImage image);
    ~ImageViewer();

  protected:
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;

  private:
    QImage image;
    bool has_image;
};

#endif // IMAGEVIEWER_H
