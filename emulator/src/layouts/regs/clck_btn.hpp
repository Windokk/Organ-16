#pragma once

#include <QPushButton>
#include <QEvent>
#include <QPainter>

#include <string>

class ClockButton : public QPushButton {
    
    QPixmap normalPixmap;
    QPixmap tintedPixmap;

public:
    ClockButton(const QString &imagePath, std::function<void()> onClick, std::function<void()> onUp, std::function<void()> onDown, QWidget *parent = nullptr) : QPushButton(parent) {
        this->onClick = onClick;
        this->onMouseUp = onUp;
        this->onMouseDown = onDown;
        setFlat(true);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        normalPixmap = QPixmap(imagePath);
        tintedPixmap = applyTint(normalPixmap, QColor(0, 0, 0, 170));

        setIcon(QIcon(normalPixmap));
        setIconSize(size());

        setStyleSheet("border: none;");

        setMouseTracking(true);
        installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::Enter) {
            setIcon(QIcon(tintedPixmap));
        } else if (event->type() == QEvent::Leave) {
            setIcon(QIcon(normalPixmap));
        } else if (event->type() == QEvent::Resize) {
            setIconSize(size());
        } else if (event->type() == QEvent::MouseButtonPress){
            onClick();
        } else if (event->type() == QEvent::Wheel) {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);

            if (wheelEvent->angleDelta().y() > 0 && onMouseUp) {
                // Scroll up
                onMouseUp();
            } else if(onMouseDown){
                // Scroll down
                onMouseDown();
            }
        }

        return QPushButton::eventFilter(obj, event);
    }

private:
    QPixmap applyTint(const QPixmap &src, const QColor &tintColor) {
        QPixmap result = src;
        result = result.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

        QPainter painter(&result);
        painter.fillRect(result.rect(), tintColor);
        painter.end();
        return result;
    }

    std::function<void()> onClick;
    std::function<void()> onMouseUp;
    std::function<void()> onMouseDown;
};