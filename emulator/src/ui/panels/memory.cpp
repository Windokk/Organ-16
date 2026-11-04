#include <QPainter>

#include "ui/workbench.hpp"
#include "ui/layout.hpp"
#include "ui/paint.hpp"
#include "ui/disasm.hpp"

using namespace ui;
using namespace theme;

int Workbench::hitMemoryCell(const Layout& L, const QPointF& pt) const {
    const double x = pt.x() - L.midX, y = pt.y() - (L.memTop + MEM_HEAD_H);
    if (x < MEM_ADDR_W || x >= MEM_ADDR_W + MEM_COLS * L.memColW || y < 0 || y >= MEM_ROWS * MEM_ROW_H)
        return -1;
    const int col = static_cast<int>((x - MEM_ADDR_W) / L.memColW);
    const int row = static_cast<int>(y / MEM_ROW_H);
    return (base_ + row * MEM_COLS + col) & 0xFFFF;
}

void Workbench::paintMemory(QPainter& p, const Layout& L) {
    const Fonts& f = fontSet();
    const double mx = L.midX, mw = L.midW - 1;
    fill(p, QRectF(mx, L.memSh, mw, SECTION_BAR), bar);
    putText(p, f.silk, barInk, mx + PAD, L.memSh + PAD, "MEMORY");

    auto tool = [&](const QRectF& r, const QString& label, int id) {
        const bool over = hover_ == id;
        if (over)
            fill(p, r, barInk);
        outline(p, r, 1, barInk);
        putText(p, f.mono11, over ? bar : barInk, r.center().x(), r.center().y(), label, Qt::AlignHCenter);
    };
    tool(L.pgUp, "PG-", HOVER_PG_UP);
    tool(L.pgDn, "PG+", HOVER_PG_DN);

    const double top = L.memTop;
    fill(p, QRectF(mx, top, mw, MEM_HEAD_H), well);
    hLine(p, mx, top + MEM_HEAD_H - 1.5, mw, rule);
    putText(p, f.mono13, inkFaint, mx + MEM_ADDR_W / 2, top + 11, "Addr", Qt::AlignHCenter);
    for (int c = 0; c < MEM_COLS; ++c)
        putText(p, f.mono13, inkFaint, mx + MEM_ADDR_W + c * L.memColW + L.memColW / 2, top + 11, "+" + QString::number(c), Qt::AlignHCenter);

    const uint16_t pc = regs_[REG_PC];
    for (int r = 0; r < MEM_ROWS; ++r) {
        const double y = top + MEM_HEAD_H + r * MEM_ROW_H;
        const double cy = y + MEM_ROW_H / 2.0;
        const uint16_t a0 = static_cast<uint16_t>(base_ + r * MEM_COLS);
        fill(p, QRectF(mx, y, MEM_ADDR_W, MEM_ROW_H), well);
        vLine(p, mx + MEM_ADDR_W - 1, y, MEM_ROW_H, rule);
        hLine(p, mx, y + MEM_ROW_H - 1, mw, ruleSoft);
        putText(p, f.mono13, inkSoft, mx + 8, cy, hex(a0));
        for (int c = 0; c < MEM_COLS; ++c) {
            const uint16_t a = static_cast<uint16_t>(a0 + c);
            const double x = mx + MEM_ADDR_W + c * L.memColW;
            const QRectF cell(x, y, L.memColW, MEM_ROW_H);
            const bool touchedCell = (touched_[a >> 6] >> (a & 63)) & 1;
            const bool isPc = a == pc;
            const bool over = hover_ == HOVER_MEM + r * MEM_COLS + c;
            if (isPc)
                fill(p, cell, hi);
            else if (over)
                fill(p, cell, well);
            if (c < MEM_COLS - 1)
                vLine(p, x + L.memColW - 1, y, MEM_ROW_H, ruleSoft);
            hLine(p, x, y + MEM_ROW_H - 1, L.memColW, ruleSoft);
            if (a == sel_)
                outline(p, cell, 2, focus);
            if (editKind_ == EditMemory && editIndex_ == a)
                continue;
            putText(p, touchedCell || isPc ? f.mono13b : f.mono13, isPc ? hiInk : touchedCell ? ink : inkFaint, x + L.memColW / 2, cy, hex(ram_[a]).mid(2), Qt::AlignHCenter);
        }
    }
}
