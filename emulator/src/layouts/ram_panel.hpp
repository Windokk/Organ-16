#pragma once

#include <QAbstractTableModel>
#include <QTableView>
#include <QHeaderView>
#include <QColor>

#include <array>

#include "core/core.hpp"

class RamPanel : public QAbstractTableModel {
    public:
        explicit RamPanel(Core& core, QObject *parent = nullptr);
        int rowCount(const QModelIndex &) const override;
        int columnCount(const QModelIndex &) const override;
        void updateRAM();
        void refresh();
        QVariant data(const QModelIndex &index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        Qt::ItemFlags flags(const QModelIndex &index) const override;
        bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    private:
        void notifyCell(uint16_t address);

        Core& core;
        std::array<uint64_t, ADDRESS_SPACE / 64> shownTouched{};
        uint16_t shownAddress = 0;
};
