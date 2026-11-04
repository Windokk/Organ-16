#include <QPainter>

#include "ui/workbench.hpp"
#include "ui/layout.hpp"
#include "ui/paint.hpp"
#include "ui/disasm.hpp"

using namespace ui;
using namespace theme;

void Workbench::paintScreen(QPainter& p, const Layout& L) {
    const Fonts& f = fontSet();
    fill(p, QRectF(L.midX, GRID_TOP, L.midW - 1, SECTION_BAR), bar);
    putText(p, f.silk, barInk, L.midX + PAD, GRID_TOP + PAD, "SCREEN");
    putText(p, f.silk, mix(bar, barInk, 0.8), L.midX + L.midW - 1 - PAD, GRID_TOP + PAD, "128 X 128 RGB565", Qt::AlignRight);

    fill(p, L.bezel, rule);
    fill(p, L.canvas, black);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(snap(L.canvas), screen_);

    putText(p, f.mono12, inkFaint, L.cap.left(), L.cap.center().y(), "0x8000 - 0xBFFF");
    putText(p, f.mono12, inkFaint, L.cap.right(), L.cap.center().y(), QString::number(lit_) + " lit", Qt::AlignRight);
}
