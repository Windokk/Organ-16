#pragma once

#include <QWidget>
#include <QPainter>
#include <QColor>
#include <QMouseEvent>
#include <QImage>

#include <array>

#include "core/core.hpp"

class CanvasWidget : public QWidget {
public:
    CanvasWidget(QWidget *parent = nullptr);

    void present(const std::array<uint16_t, SCREEN_PIXELS>& pixels);

    void clear();

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