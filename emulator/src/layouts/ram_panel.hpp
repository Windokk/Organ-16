#pragma once

#include <QAbstractTableModel>
#include <QTableView>
#include <QHeaderView>

#include "../backend/memory/ram.hpp"

class RamPanel : public QAbstractTableModel {
    public:
        explicit RamPanel(QObject *parent = nullptr);
        int rowCount(const QModelIndex &) const override;
        int columnCount(const QModelIndex &) const override;
        void updateRAM();
        QVariant data(const QModelIndex &index, int role) const override;
        void setCellBackground(int row, int col, const QColor &color);
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        Qt::ItemFlags flags(const QModelIndex &index) const override;
        bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    private:
        RAM* ram;
        QMap<QPair<int, int>, QColor> bgColors;
};