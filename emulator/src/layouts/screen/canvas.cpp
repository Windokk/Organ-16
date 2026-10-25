#include "canvas.hpp"

CanvasWidget::CanvasWidget(QWidget *parent) : QWidget(parent), canvas(SCREEN_WIDTH, SCREEN_HEIGHT, QImage::Format_RGB32)
{
    canvas.fill(Qt::black);
    setMinimumSize(128, 128);
}

void CanvasWidget::present(const std::array<uint16_t, SCREEN_PIXELS>& pixels)
{
    static const auto tables = [] {
        std::array<std::array<uint32_t, 64>, 2> t{};
        for (uint32_t i = 0; i < 32; ++i) t[0][i] = (i * 255) / 31;
        for (uint32_t i = 0; i < 64; ++i) t[1][i] = (i * 255) / 63;
        return t;
    }();

    for (size_t y = 0; y < SCREEN_HEIGHT; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(canvas.scanLine(static_cast<int>(y)));
        const uint16_t* source = pixels.data() + y * SCREEN_WIDTH;
        for (size_t x = 0; x < SCREEN_WIDTH; ++x) {
            const uint16_t v = source[x];
            line[x] = 0xFF000000u | (tables[0][v >> 11] << 16) | (tables[1][(v >> 5) & 0x3F] << 8) | tables[0][v & 0x1F];
        }
    }
    update();
}

void CanvasWidget::clear()
{
    canvas.fill(Qt::black);
    update();
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
