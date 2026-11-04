#include <QPainter>

#include "ui/workbench.hpp"
#include "ui/layout.hpp"
#include "ui/paint.hpp"
#include "ui/disasm.hpp"

using namespace ui;
using namespace theme;

uint16_t Workbench::sweepStart(uint16_t pc, int back) const {
    for (int k = back * 2; k >= 1; --k) {
        uint16_t addr = static_cast<uint16_t>(pc - k);
        // the unsigned subtraction wraps when addr steps past pc, ending the loop
        while (static_cast<uint16_t>(pc - addr) > 0 && static_cast<uint16_t>(pc - addr) <= 2 * back)
            addr = static_cast<uint16_t>(addr + disassemble(ram_.data(), addr).words);
        if (addr == pc)
            return static_cast<uint16_t>(pc - k);
    }
    return pc;
}

void Workbench::paintListing(QPainter& p, const Layout& L) {
    const Fonts& f = fontSet();
    fill(p, QRectF(L.rightX, L.listSh, RIGHT_W, SECTION_BAR), bar);
    putText(p, f.silk, barInk, L.rightX + PAD, L.listSh + PAD, "LISTING");

    const QRectF cb(L.follow.left() + 4, L.follow.top() + 3, 13, 13);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    if (follow_) {
        p.setBrush(hi);
        p.drawRoundedRect(cb, 2, 2);
        QPen pen(hiInk, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.drawPolyline(QPolygonF({QPointF(cb.left() + 3, cb.top() + 7), QPointF(cb.left() + 5.5, cb.top() + 9.5), QPointF(cb.left() + 10, cb.top() + 4)}));
    }
    else {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(barInk, 1));
        p.drawRoundedRect(cb.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
    }
    p.setRenderHint(QPainter::Antialiasing, false);
    putText(p, f.mono11, barInk, L.follow.left() + 26, L.follow.center().y(), "follow PC");

    const uint16_t pc = regs_[REG_PC];
    if (follow_)
        listBase_ = sweepStart(pc, LIST_LOOKBEHIND);
    uint16_t a = listBase_;
    for (int i = 0; i < L.listRows; ++i) {
        const double y = L.listTop + i * LIST_ROW_H;
        const double cy = y + LIST_ROW_H / 2 - 0.5;
        const Insn insn = disassemble(ram_.data(), a);
        const bool cur = a == pc;
        const bool over = hover_ == HOVER_LIST + i;
        const bool hasBp = breakpoints_[a];
        if (cur)
            fill(p, QRectF(L.rightX, y, RIGHT_W, LIST_ROW_H), hi);
        else if (over)
            fill(p, QRectF(L.rightX, y, RIGHT_W, LIST_ROW_H), well);
        hLine(p, L.rightX, y + LIST_ROW_H - 1, RIGHT_W, ruleSoft);

        const QRectF bp(L.rightX + 6, std::round(y + 6.5), 10, 10);
        if (hasBp) {
            fill(p, bp, lampHalt);
        }
        else
            outline(p, bp, 1, over ? ink : ruleSoft);

        const QColor soft = cur ? mix(hi, hiInk, 0.75) : inkSoft;
        const QColor faint = cur ? mix(hi, hiInk, 0.75) : inkFaint;
        putText(p, f.mono13, soft, L.rightX + LIST_GUTTER, cy, hex(a));
        QString words = hex(insn.w).mid(2);
        if (insn.words == 2)
            words += " " + hex(insn.imm).mid(2);
        putText(p, f.mono13, faint, L.rightX + 80, cy, words);

        double x = L.rightX + 162;
        const double opW = std::max(60.0, textWidth(f.mono13b, insn.name));
        putText(p, f.mono13b, cur ? hiInk : insn.w == 0 ? inkFaint : ink, x, cy, insn.name);
        x += opW;
        for (const OperandPart& part : operands(insn)) {
            const double w = putText(p, f.mono13, cur ? hiInk : ink, x, cy, part.text);
            if (part.address)
                hLine(p, x, std::round(cy + 7), w, cur ? hiInk : ink);
            x += w;
        }
        if (a == sel_ && !cur)
            outline(p, QRectF(L.rightX, y, RIGHT_W, LIST_ROW_H), 2, focus);
        a = static_cast<uint16_t>(a + insn.words);
    }
}
