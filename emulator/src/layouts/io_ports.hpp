#pragma once
#include <QWidget>
#include <QPushButton>
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

signals:
    void squareClicked(char portName, int bitIndex);

private:
    int portIndexFromName(char portName) const;
    void createPort(int portIndex, char portName);

    std::vector<std::vector<QPushButton*>> m_ports;  // ports A,B,C → 3 × 16 LEDs
    uint16_t m_portValues[3] = {0, 0, 0};
};