#pragma once

#include <QPushButton>
#include <QEvent>
#include <QPainter>

#include <string>

class ClockButton : public QPushButton {
    
    QPixmap normalPixmap;
    QPixmap tintedPixmap;

public:
    ClockButton(const QString &imagePath, std::function<void()> onClick, QWidget *parent = nullptr) : QPushButton(parent) {
        this->onClick = onClick;
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
};