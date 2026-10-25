#include "ram_panel.hpp"

#include <QBrush>
#include <QString>

RamPanel::RamPanel(Core& core, QObject *parent)
    : QAbstractTableModel(parent), core(core) {}

int RamPanel::rowCount(const QModelIndex &) const {
    return ADDRESS_SPACE / 16;
}

int RamPanel::columnCount(const QModelIndex &) const {
    return 16;
}

void RamPanel::updateRAM() {
    beginResetModel();
    shownTouched = core.touched;
    shownAddress = core.lastAddress;
    core.clearRamDirty();
    endResetModel();
}

void RamPanel::notifyCell(uint16_t address) {
    const QModelIndex idx = index(address / 16, address % 16);
    emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::BackgroundRole});
}

void RamPanel::refresh() {
    if (core.ramDirty) {
        emit dataChanged(index(core.ramDirtyLo / 16, 0), index(core.ramDirtyHi / 16, 15), {Qt::DisplayRole});
        core.clearRamDirty();
    }

    if (shownAddress != core.lastAddress) {
        const uint16_t previous = shownAddress;
        shownAddress = core.lastAddress;
        notifyCell(previous);
        notifyCell(shownAddress);
    }

    for (size_t word = 0; word < core.touched.size(); ++word) {
        if (core.touched[word] == shownTouched[word])
            continue;
        shownTouched[word] = core.touched[word];
        emit dataChanged(index(static_cast<int>(word * 4), 0), index(static_cast<int>(word * 4 + 3), 15), {Qt::BackgroundRole});
    }
}

QVariant RamPanel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    const uint16_t address = static_cast<uint16_t>(index.row() * 16 + index.column());

    if (role == Qt::DisplayRole)
        return QString("%1").arg(core.ram[address], 4, 16, QChar('0')).toUpper();

    if (role == Qt::BackgroundRole) {
        if (address == shownAddress)
            return QBrush(QColor("#4477ff"));
        if ((core.touched[address >> 6] >> (address & 63)) & 1)
            return QBrush(QColor("#5e5e5e"));
    }

    return QVariant();
}

QVariant RamPanel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Vertical)
        return QString("0x%1").arg(section * 0x10, 4, 16, QChar('0'));

    if (orientation == Qt::Horizontal)
        return QString("%1").arg(section, 1, 16).toUpper();

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

    bool ok;
    const uint16_t newVal = value.toString().toUShort(&ok, 16);
    if (!ok)
        return false;

    const uint16_t address = static_cast<uint16_t>(index.row() * 16 + index.column());
    core.poke(address, newVal);
    if (core.tickCount == 0)
        core.init();
    emit dataChanged(index, index);
    return true;
}
