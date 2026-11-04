#include <QPainter>

#include "ui/workbench.hpp"
#include "ui/layout.hpp"
#include "ui/paint.hpp"
#include "ui/disasm.hpp"

using namespace ui;
using namespace theme;

void Workbench::paintPorts(QPainter& p, const Layout& L) {
    const Fonts& f = fontSet();
    fill(p, QRectF(L.ix, L.portsSh, LEFT_W - 1, SECTION_BAR), bar);
    putText(p, f.silk, barInk, L.ix + PAD, L.portsSh + PAD, "IO PORTS");

    const double px = L.ix + PAD, pw = LEFT_W - 1 - 2 * PAD;
    for (int port = 0; port < PORT_COUNT; ++port) {
        const double top = L.portsTop + port * PORT_STRIDE;
        putText(p, f.silk, ink, px, top + 10, "PORT " + QString::number(port));
        putText(p, f.mono13b, ink, px + pw, top + 10, hex(io_[port]), Qt::AlignRight);
        hLine(p, px, top + 19, pw, rule);
        for (int col = 0; col < PORT_BITS; ++col) {
            const int bit = PORT_BITS - 1 - col;  // bit 15 on the left
            const QRectF dip = dipRect(L, port, col);
            const double cx = px + pw / PORT_BITS * col + pw / (2 * PORT_BITS);
            const double by = dip.top();
            fill(p, dip, well);
            outline(p, dip, 1, rule);
            const bool on = (io_[port] >> bit) & 1;
            if (hover_ == HOVER_DIP + port * PORT_BITS + bit)
                outline(p, dip.adjusted(-1, -1, 1, 1), 1, hi);
            fill(p, QRectF(dip.left() + 3, dip.top() + (on ? 3 : 17), 16, 14), on ? bitOn : inkFaint);
            putText(p, f.mono11, inkSoft, cx, by + 36 + 7, QString::number(bit), Qt::AlignHCenter);
            const QString label = keyLabel(port, bit);
            if (!label.isEmpty()) {
                const bool binding = binding_.first == port && binding_.second == bit;
                const double lw = std::max(12.0, textWidth(f.mono10b, label));
                const QRectF kr(cx - lw / 2, by + 52, lw, 12);
                if (binding)
                    fill(p, kr, hi);
                putText(p, f.mono10b, binding ? black : inkFaint, cx, by + 58, label, Qt::AlignHCenter);
            }
        }
    }
}

QRectF Workbench::dipRect(const Layout& L, int port, int col) const {
    const double px = L.ix + PAD, pw = LEFT_W - 1 - 2 * PAD;
    const double cx = px + pw / PORT_BITS * col + pw / (2 * PORT_BITS);
    return QRectF(std::round(cx - DIP_W / 2), L.portsTop + port * PORT_STRIDE + DIP_Y, DIP_W, DIP_H);
}

// returns port * 16 + bit, or -1
int Workbench::hitDip(const Layout& L, const QPointF& pt) const {
    for (int port = 0; port < PORT_COUNT; ++port)
        for (int col = 0; col < PORT_BITS; ++col)
            if (dipRect(L, port, col).contains(pt))
                return port * PORT_BITS + (PORT_BITS - 1 - col);
    return -1;
}
