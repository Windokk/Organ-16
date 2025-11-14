#include "io_ports.hpp"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>

IOPortsPanel::IOPortsPanel(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_ports.resize(3);

    const char portNames[3] = { 'A', 'B', 'C' };

    for (int p = 0; p < 3; ++p) {
        createPort(p, portNames[p]);

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

            vbox->addWidget(led);
            vbox->addWidget(label);

            grid->addWidget(cell, i / 8, i % 8);
        }
        mainLayout->addWidget(groupBox);
    }

    setLayout(mainLayout);
}

void IOPortsPanel::createPort(int portIndex, char portName)
{
    m_ports[portIndex].resize(16);

    for (int i = 0; i < 16; ++i) {
        QPushButton* led = new QPushButton;
        led->setCheckable(true);
        led->setFixedSize(28, 28);

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

        // Connect to signal, update internal state
        connect(led, &QPushButton::clicked,
                this, [this, portName, i]() {
                    int p = portIndexFromName(portName);

                    // Toggle internal bit
                    if (m_ports[p][i]->isChecked())
                        m_portValues[p] |= (1u << i);
                    else
                        m_portValues[p] &= ~(1u << i);

                    emit squareClicked(portName, i);
                });
    }
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

        m_portValues[p] = 0;

        for(int i = 0; i < 16; i++){
            
            m_ports[p][i]->setChecked(false);
        }
    }
}
