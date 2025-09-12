#include "canvas.hpp"

CanvasWidget::CanvasWidget(QWidget *parent) : QWidget(parent), canvas(320, 240, QImage::Format_RGB32)
{
    canvas = QImage(128, 128, QImage::Format_RGB32);
    canvas.fill(Qt::yellow);
    setMinimumSize(128, 128);
}

void CanvasWidget::setPixel(int x, int y, QColor color)
{
    if (x >= 0 && x < canvas.width() && y >= 0 && y < canvas.height()) {
        canvas.setPixelColor(x, y, color);
        update();
    }
}

void CanvasWidget::clear()
{
    canvas.fill(Qt::black);
    update();
}

void CanvasWidget::updateFrame(const QImage &newImage)
{
    if (newImage.size() == canvas.size() && newImage.format() == canvas.format()) {
        canvas = newImage.copy();
        update();
    }
}

void CanvasWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    QSize imgSize = canvas.size();
    QSize widgetSize = size();

    QSize scaledSize = imgSize.scaled(widgetSize, Qt::KeepAspectRatio);

    QPoint center((widgetSize.width() - scaledSize.width())/2,
                    (widgetSize.height() - scaledSize.height())/2);

    QRect targetRect(center, scaledSize);
    painter.drawImage(targetRect, canvas);
}
