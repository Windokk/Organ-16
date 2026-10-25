#include "io_ports.hpp"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QApplication>
#include <QLineEdit>
#include <QKeyEvent>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMenu>
#include <QSettings>

IOPortsPanel::IOPortsPanel(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_ports.resize(3);
    m_labels.resize(3);

    const char portNames[3] = { 'A', 'B', 'C' };

    for (int p = 0; p < 3; ++p) {
        createPort(p, portNames[p]);
        m_labels[p].resize(16);

        QGroupBox* groupBox = new QGroupBox(QString("Port %1").arg(portNames[p]), this);
        QGridLayout* grid = new QGridLayout(groupBox);

        // Add LEDs + labels
        for (int i = 0; i < 16; ++i) {
            QWidget* cell = new QWidget();
            QVBoxLayout* vbox = new QVBoxLayout(cell);
            vbox->setSpacing(2);
            vbox->setAlignment(Qt::AlignHCenter);

            QPushButton* led = m_ports[p][i];

            QLabel* label = new QLabel(QString::number(i));
            label->setAlignment(Qt::AlignCenter);
            m_labels[p][i] = label;

            vbox->addWidget(led);
            vbox->addWidget(label);

            grid->addWidget(cell, i / 8, i % 8);
        }
        mainLayout->addWidget(groupBox);
    }

    setLayout(mainLayout);

    loadBindings();
    qApp->installEventFilter(this);
}

void IOPortsPanel::createPort(int portIndex, char portName)
{
    m_ports[portIndex].resize(16);

    for (int i = 0; i < 16; ++i) {
        QPushButton* led = new QPushButton;
        led->setCheckable(true);
        led->setFixedSize(28, 28);
        led->setContextMenuPolicy(Qt::CustomContextMenu);
        led->setToolTip("Click to toggle - right-click to bind a key");

        // Round LED look
        led->setStyleSheet(
            "QPushButton {"
            "   border-radius: 14px;"
            "   background-color: #555;"
            "   border: 2px solid #333;"
            "}"
            "QPushButton:checked {"
            "   background-color: #4477ff;"
            "   border: 4px solid #1c3068ff;"
            "}"
        );

        m_ports[portIndex][i] = led;

        connect(led, &QPushButton::clicked,
                this, [this, portIndex, i]() {
                    setBit(portIndex, i, m_ports[portIndex][i]->isChecked());
                });

        connect(led, &QPushButton::customContextMenuRequested,
                this, [this, portIndex, i, led](const QPoint& pos) {
                    showBindingMenu(portIndex, i, led->mapToGlobal(pos));
                });
    }
}

void IOPortsPanel::setBit(int portIndex, int bitIndex, bool on)
{
    if (on)
        m_portValues[portIndex] |= (1u << bitIndex);
    else
        m_portValues[portIndex] &= ~(1u << bitIndex);

    m_ports[portIndex][bitIndex]->setChecked(on);
    emit squareClicked(portNameFromIndex(portIndex), bitIndex);
}

int IOPortsPanel::portIndexFromName(char portName) const
{
    switch (portName) {
        case 'A': return 0;
        case 'B': return 1;
        case 'C': return 2;
    }
    return 0; // fallback
}

char IOPortsPanel::portNameFromIndex(int index) const{
    char names[3] = {'A', 'B', 'C'};
    return names[index];
}

void IOPortsPanel::setPortValue(char portName, uint16_t value)
{
    int p = portIndexFromName(portName);
    m_portValues[p] = value;

    for (int i = 0; i < 16; ++i) {
        bool bitSet = (value >> i) & 1;
        m_ports[p][i]->setChecked(bitSet);
    }
}

uint16_t IOPortsPanel::portValue(char portName) const
{
    int p = portIndexFromName(portName);
    return m_portValues[p];
}

void IOPortsPanel::reset()
{
    for(int p = 0; p < 3; p++){

        m_keyHeld[p] = 0;

        m_portValues[p] = 0;

        for(int i = 0; i < 16; i++){
            
            m_ports[p][i]->setChecked(false);
        }
    }
}

void IOPortsPanel::bindKey(int portIndex, int bitIndex, int key)
{
    if (key == 0) {
        clearBinding(portIndex, bitIndex);
        return;
    }

    for (int p = 0; p < 3; ++p) {
        for (int i = 0; i < 16; ++i) {
            if (m_bindings[p][i] == key && !(p == portIndex && i == bitIndex))
                clearBinding(p, i);
        }
    }

    clearBinding(portIndex, bitIndex);
    m_bindings[portIndex][bitIndex] = key;
    updateLabel(portIndex, bitIndex);
    saveBindings();
}

void IOPortsPanel::clearBinding(int portIndex, int bitIndex)
{
    const uint16_t mask = 1u << bitIndex;
    if (m_keyHeld[portIndex] & mask) {
        m_keyHeld[portIndex] &= ~mask;
        setBit(portIndex, bitIndex, false);
    }

    m_bindings[portIndex][bitIndex] = 0;
    updateLabel(portIndex, bitIndex);
    saveBindings();
}

int IOPortsPanel::boundKey(int portIndex, int bitIndex) const
{
    return m_bindings[portIndex][bitIndex];
}

void IOPortsPanel::updateLabel(int portIndex, int bitIndex)
{
    const int key = m_bindings[portIndex][bitIndex];
    QString text = QString::number(bitIndex);
    if (key != 0)
        text += "\n" + QKeySequence(key).toString(QKeySequence::NativeText);
    m_labels[portIndex][bitIndex]->setText(text);
}

void IOPortsPanel::showBindingMenu(int portIndex, int bitIndex, const QPoint& globalPos)
{
    QMenu menu(this);
    QAction* bind = menu.addAction("Bind key...");
    QAction* clear = menu.addAction("Clear binding");
    clear->setEnabled(m_bindings[portIndex][bitIndex] != 0);

    QAction* chosen = menu.exec(globalPos);
    if (chosen == bind)
        promptBinding(portIndex, bitIndex);
    else if (chosen == clear)
        clearBinding(portIndex, bitIndex);
}

void IOPortsPanel::promptBinding(int portIndex, int bitIndex)
{
    QDialog dialog(this);
    dialog.setWindowTitle("Bind key");

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(
        QString("Press the key to bind to port %1, bit %2")
            .arg(portNameFromIndex(portIndex))
            .arg(bitIndex)));

    QKeySequenceEdit* edit = new QKeySequenceEdit(&dialog);
    edit->setMaximumSequenceLength(1);
    layout->addWidget(edit);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    edit->setFocus();
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QKeySequence sequence = edit->keySequence();
    if (sequence.isEmpty())
        return;

    bindKey(portIndex, bitIndex, static_cast<int>(sequence[0].key()));
}

void IOPortsPanel::saveBindings() const
{
    QSettings settings("Organ16", "Emulator");
    settings.beginGroup("ioKeyBindings");
    for (int p = 0; p < 3; ++p) {
        for (int i = 0; i < 16; ++i) {
            const QString name = QString("%1_%2").arg(portNameFromIndex(p)).arg(i);
            if (m_bindings[p][i] != 0)
                settings.setValue(name, m_bindings[p][i]);
            else
                settings.remove(name);
        }
    }
}

void IOPortsPanel::loadBindings()
{
    QSettings settings("Organ16", "Emulator");
    settings.beginGroup("ioKeyBindings");
    for (int p = 0; p < 3; ++p) {
        for (int i = 0; i < 16; ++i) {
            const QString name = QString("%1_%2").arg(portNameFromIndex(p)).arg(i);
            m_bindings[p][i] = settings.value(name, 0).toInt();
            updateLabel(p, i);
        }
    }
}

bool IOPortsPanel::isBound(int key) const
{
    for (const auto& port : m_bindings)
        for (int bound : port)
            if (bound == key)
                return true;
    return false;
}

bool IOPortsPanel::pressKey(int key)
{
    bool handled = false;
    for (int p = 0; p < 3; ++p) {
        for (int i = 0; i < 16; ++i) {
            if (m_bindings[p][i] != key)
                continue;
            handled = true;
            const uint16_t mask = 1u << i;
            if (m_keyHeld[p] & mask)
                continue;
            m_keyHeld[p] |= mask;
            setBit(p, i, true);
        }
    }
    return handled;
}

bool IOPortsPanel::releaseKey(int key)
{
    bool handled = false;
    for (int p = 0; p < 3; ++p) {
        for (int i = 0; i < 16; ++i) {
            if (m_bindings[p][i] != key)
                continue;
            handled = true;
            const uint16_t mask = 1u << i;
            if (!(m_keyHeld[p] & mask))
                continue;
            m_keyHeld[p] &= ~mask;
            setBit(p, i, false);
        }
    }
    return handled;
}

void IOPortsPanel::releaseAllKeys()
{
    for (int p = 0; p < 3; ++p) {
        for (int i = 0; i < 16; ++i) {
            const uint16_t mask = 1u << i;
            if (m_keyHeld[p] & mask) {
                m_keyHeld[p] &= ~mask;
                setBit(p, i, false);
            }
        }
    }
}

bool IOPortsPanel::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
        case QEvent::KeyPress: {
            if (QApplication::activeModalWidget() || qobject_cast<QLineEdit*>(QApplication::focusWidget()))
                return false;
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            return pressKey(keyEvent->key());
        }
        case QEvent::KeyRelease: {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->isAutoRepeat())
                return isBound(keyEvent->key());
            return releaseKey(keyEvent->key());
        }
        case QEvent::ApplicationDeactivate:
            releaseAllKeys();
            return false;
        default:
            return QWidget::eventFilter(watched, event);
    }
}
