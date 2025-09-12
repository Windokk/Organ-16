#include <QSplitterHandle>
#include <QPainter>

class FullHeightHandle : public QSplitterHandle {
public:
    FullHeightHandle(Qt::Orientation orientation, QSplitter *parent)
        : QSplitterHandle(orientation, parent) {}

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);
    }

    QSize sizeHint() const override {
    constexpr int thickness = 2;

    if (orientation() == Qt::Horizontal)
        return QSize(thickness, 0);
    else
        return QSize(0, thickness);
}
};

class FullSplitter : public QSplitter {
public:
    FullSplitter(Qt::Orientation orientation, QWidget *parent = nullptr)
        : QSplitter(orientation, parent) {}

protected:
    QSplitterHandle *createHandle() override {
        return new FullHeightHandle(orientation(), this);
    }
};