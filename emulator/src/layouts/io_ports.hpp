#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <array>
#include <vector>

class IOPortsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit IOPortsPanel(QWidget* parent = nullptr);

    // Set or get full 16-bit port values (A, B, C)
    void setPortValue(char portName, uint16_t value);
    uint16_t portValue(char portName) const;
    
    char portNameFromIndex(int index) const;

    void reset();

    void bindKey(int portIndex, int bitIndex, int key);
    void clearBinding(int portIndex, int bitIndex);
    int boundKey(int portIndex, int bitIndex) const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void squareClicked(char portName, int bitIndex);

private:
    int portIndexFromName(char portName) const;
    void createPort(int portIndex, char portName);
    void setBit(int portIndex, int bitIndex, bool on);
    void promptBinding(int portIndex, int bitIndex);
    void showBindingMenu(int portIndex, int bitIndex, const QPoint& globalPos);
    void updateLabel(int portIndex, int bitIndex);
    void saveBindings() const;
    void loadBindings();
    bool isBound(int key) const;
    bool pressKey(int key);
    bool releaseKey(int key);
    void releaseAllKeys();

    std::vector<std::vector<QPushButton*>> m_ports;  // ports A,B,C → 3 × 16 LEDs
    std::vector<std::vector<QLabel*>> m_labels;
    uint16_t m_portValues[3] = {0, 0, 0};
    uint16_t m_keyHeld[3] = {0, 0, 0};
    std::array<std::array<int, 16>, 3> m_bindings{};
};
