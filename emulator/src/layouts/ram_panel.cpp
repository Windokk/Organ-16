#include "ram_panel.hpp"

#include <QString>

#include "../backend/cpu.hpp"

RamPanel::RamPanel(QObject *parent)
    : QAbstractTableModel(parent), ram(RAM::GetInstance()) {}

int RamPanel::rowCount(const QModelIndex &) const {
    return 4096; // 65536 / 16
}

int RamPanel::columnCount(const QModelIndex &) const {
    return 16;
}

void RamPanel::updateRAM() {
    beginResetModel();
    endResetModel();
}

QVariant RamPanel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    int address = index.row() * 16 + index.column();
    if (address >= 65536)
        return QVariant();

    if (role == Qt::DisplayRole) {
        try {
            uint16_t word = ram->Read(static_cast<uint16_t>(address));
            return QString("%1").arg(word, 4, 16, QChar('0')).toUpper();
        } catch (...) {
            return "ERR";
        }
    }
    if (role == Qt::BackgroundRole) {
        QPair<int, int> cell(index.row(), index.column());
        if (bgColors.contains(cell)) {
            return QBrush(bgColors[cell]);
        }
    }

    return QVariant();
}

void RamPanel::setCellBackground(int row, int col, const QColor &color) {
    QPair<int, int> cell(row, col);
    bgColors[cell] = color;

    QModelIndex idx = index(row, col);
    emit dataChanged(idx, idx, {Qt::BackgroundRole});
}

QVariant RamPanel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Vertical) {
        return QString("0x%1").arg(section * 0x10, 4, 16, QChar('0'));
    }

    if (orientation == Qt::Horizontal) {
        return QString("%1").arg(section, 1, 16).toUpper();
    }

    return QVariant();
}

Qt::ItemFlags RamPanel::flags(const QModelIndex &index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

bool RamPanel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (role != Qt::EditRole || !index.isValid())
        return false;

    int address = index.row() * 16 + index.column();
    bool ok;
    uint16_t newVal = value.toString().toUShort(&ok, 16);

    if (!ok || address >= 65536)
        return false;

    try {
        ram->Write(static_cast<uint16_t>(address), newVal, true);
        CPU::GetInstance()->Init();
        emit dataChanged(index, index);
        return true;
    } catch (...) {
        return false;
    }
}