#include <QPainter>

#include "ui/workbench.hpp"
#include "ui/layout.hpp"
#include "ui/paint.hpp"
#include "ui/disasm.hpp"

using namespace ui;
using namespace theme;

// only the hex value cell is editable, so only that column counts as a hit
int Workbench::hitRegisterRow(const Layout&, const QPointF& pt) const {
    const double x = pt.x() - REG_TABLE_X, y = pt.y() - REG_TABLE_Y;
    if (x < REG_TAG_W || x > REG_TAG_W + REG_HEX_W || y < 0 || y >= REG_ROWS * REG_ROW_H)
        return -1;
    return static_cast<int>(y / REG_ROW_H);
}

void Workbench::paintRegisters(QPainter& p, const Layout& L) {
    const Fonts& f = fontSet();
    const double sx = L.ix;
    fill(p, QRectF(sx, GRID_TOP, LEFT_W - 1, SECTION_BAR), bar);
    putText(p, f.silk, barInk, sx + PAD, GRID_TOP + PAD, "REGISTERS");

    const double tx = REG_TABLE_X, ty = REG_TABLE_Y;
    const double tagW = REG_TAG_W, hxW = REG_HEX_W, dcW = REG_DEC_W, tableW = LEFT_W - 1 - 2 * PAD;
    const qint64 now = clock_.elapsed();

    for (int i = 0; i < REG_ROWS; ++i) {
        const double y = ty + i * REG_ROW_H;
        const double cy = y + REG_ROW_H / 2;
        const uint16_t v = regs_[i];
        const QString name = REG_NAMES[i];
        const bool general = i < REG_SP, pointer = i >= REG_SP && i < REG_IR0;
        const QColor tag = general ? regGeneral : pointer ? regPointer : regInstruction;

        const bool flash = flashUntil_[i] > now;
        const QColor fg = flash ? hiInk : QColor();

        if (flash)
            fill(p, QRectF(tx + tagW, y, tableW - tagW, REG_ROW_H), hi);
        fill(p, QRectF(tx, y, tagW, REG_ROW_H), tag);
        hLine(p, tx + tagW, y + REG_ROW_H - 1, tableW - tagW, ruleSoft);
        if (i == REG_SP || i == REG_IR0)
            hLine(p, tx, y, tableW, rule);
        vLine(p, tx + tagW - 1, y, REG_ROW_H, rule);
        putText(p, f.mono13b, regInk, tx + 8, cy, name);

        const QString hx = i == REG_FLAGS ? "0b" + QString::number(v & 0xF, 2).rightJustified(4, '0') : hex(v);
        if (!(editKind_ == EditRegister && editIndex_ == i))
            putText(p, f.mono13b, flash ? fg : ink, tx + tagW + 8, cy, hx);

        const double dx = tx + tagW + hxW;
        if (i == REG_IR0) {
            const uint16_t words[2] = {regs_[REG_IR0], regs_[REG_IR1]};
            putText(p, f.mono13, flash ? fg : ink, dx + 8, cy, formatInsn(disassemble(words, 0)));
        }
        else if (i == REG_IR1) {
            putText(p, f.mono13, flash ? fg : inkSoft, dx + 8, cy, "ext");
        }
        else {
            if (i != REG_FLAGS)
                putText(p, f.mono13, flash ? fg : inkSoft, dx + dcW - 8, cy, decimal(v), Qt::AlignRight);
            const double bx = dx + dcW + 8;
            if (i == REG_FLAGS) {
                double fx = bx;
                const char* flagNames[4] = {"Z", "N", "C", "O"};
                for (int k = 0; k < 4; ++k) {
                    const bool on = (v >> k) & 1;
                    fx += putText(p, on ? f.mono12b : f.mono12, flash ? fg : on ? ink : ruleSoft, fx, cy, flagNames[k]);
                    fx += textWidth(f.mono12, " ");
                }
            }
            else
                putText(p, f.mono12, flash ? fg : inkSoft, bx, cy, binary16(v));
        }
    }
}
