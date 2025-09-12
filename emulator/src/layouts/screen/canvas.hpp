#pragma once

#include <QWidget>
#include <QPainter>
#include <QColor>
#include <QMouseEvent>

class CanvasWidget : public QWidget {
public:
    CanvasWidget(QWidget *parent = nullptr);

    void setPixel(int x, int y, QColor color);

    void clear();

    void updateFrame(const QImage &newImage);

    QSize sizeHint() const override {
        return QSize(128, 128);
    }

    bool hasHeightForWidth() const override {
        return true;
    }

    int heightForWidth(int w) const override {
        return w;
    }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QImage canvas;
};