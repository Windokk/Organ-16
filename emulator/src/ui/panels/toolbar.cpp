#include <QFileInfo>
#include <QPainter>

#include <algorithm>
#include <cmath>

#include "ui/workbench.hpp"
#include "ui/layout.hpp"
#include "ui/paint.hpp"
#include "ui/disasm.hpp"

using namespace ui;
using namespace theme;

void Workbench::paintHeader(QPainter& p, const Layout& L) {
    const Fonts& f = fontSet();
    const double cy = HEADER_TOP + BAR_H / 2;

    auto cell = [&](const QRectF& r, const QString& label, int id, const QFont& font) {
        const bool over = hover_ == id;
        if (over)
            fill(p, QRectF(r.left(), r.top(), r.width() - 1, r.height()), hi);
        putText(p, font, over ? hiInk : ink, r.left() + MARGIN, cy, label);
        vLine(p, r.right() - 1, r.top(), r.height(), rule);
    };

    p.drawPixmap(snap(L.logo).toRect(), logo_.scaled(snap(L.logo).size().toSize() * 4, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    putText(p, f.brand, ink, L.brand.left() + MARGIN + L.logo.width() + 10, cy + 1, "Organ 16");
    vLine(p, L.brand.right() - 1, L.brand.top(), BAR_H, rule);

    cell(L.load, "Load .bin", HOVER_LOAD, f.body);
    cell(L.samples, "Samples", HOVER_SAMPLES, f.body);

    const char* names[3] = {"RUN", "BRK", "HALT"};
    const bool on[3] = {running_, hit_, halted_};
    const QColor colors[3] = {lampRun, lampWait, lampHalt};
    p.setRenderHint(QPainter::Antialiasing, true);
    for (int i = 0; i < 3; ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(rule);
        p.drawEllipse(L.lamp[i]);
        p.setBrush(on[i] ? colors[i] : lampOff);
        p.drawEllipse(L.lamp[i].adjusted(1, 1, -1, -1));
        putText(p, f.silk, ink, L.lampText[i], cy, names[i]);
    }
    p.setRenderHint(QPainter::Antialiasing, false);

    fill(p, QRectF(L.ix, TRANSPORT_TOP - 2, L.iw, 2), rule);
}

void Workbench::paintTransport(QPainter& p, const Layout& L) {
    const Fonts& f = fontSet();

    auto button = [&](const QRectF& r, const QString& label, int id, const QColor& color) {
        const bool over = hover_ == id;
        QColor bg = color;
        QColor fg = black;
        if (over) {
            bg = hi;
            fg = hiInk;
        }
        fill(p, QRectF(r.left(), r.top(), r.width() - 1, r.height()), bg);
        vLine(p, r.right() - 1, r.top(), r.height(), rule);
        putText(p, f.button, fg, r.left() + 18, r.top() + BAR_H / 2, label);
    };
    button(L.run, running_ ? "Pause" : "Run", HOVER_RUN, lampRun);
    button(L.stepBtn, "Step", HOVER_STEP, stepBtn);
    button(L.reset, "Reset", HOVER_RESET, resetBtn);

    const double cy = TRANSPORT_TOP + 22;
    vLine(p, L.hz.right() - 1, L.hz.top(), BAR_H, rule);

    fill(p, L.unit, well);
    const int unit = hzField_->property("unit").toInt();
    putText(p, f.mono15b, hi, L.unit.left() + 8, TRANSPORT_TOP + BAR_H / 2, unit == 2 ? "MHz" : unit == 1 ? "kHz" : "Hz");
    vLine(p, L.unit.right() - 1, L.unit.top(), BAR_H, rule);

    fill(p, L.range, panel);
    vLine(p, L.range.right() - 1, L.range.top(), BAR_H, rule);
    const double frac = std::clamp(std::log10(std::max(hz_, 1.0)) / std::log10(HZ_MAX), 0.0, 1.0);
    const double tx0 = L.range.left() + SLIDER_INSET, tx1 = L.range.right() - 1 - SLIDER_INSET;
    const double tcx = tx0 + (tx1 - tx0) * frac, tcy = L.range.top() + BAR_H / 2;
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(ruleSoft);
    p.drawRoundedRect(QRectF(tx0, tcy - 2, tx1 - tx0, 4), 2, 2);
    p.setBrush(hi);
    p.drawRoundedRect(QRectF(tx0, tcy - 2, tcx - tx0, 4), 2, 2);
    p.drawEllipse(QPointF(tcx, tcy), 8, 8);
    p.setRenderHint(QPainter::Antialiasing, false);

    const char* rateLabels[4] = {"1k", "100k", "1M", "10M"};
    for (int i = 0; i < 4; ++i) {
        const bool selected = std::abs(hz_ - RATE_VALUES[i]) < 0.5;
        const bool over = hover_ == HOVER_RATE + i;
        const bool lit = selected || over;
        fill(p, QRectF(L.rates[i].left(), L.rates[i].top(), L.rates[i].width() - 1, L.rates[i].height()), lit ? hi : panel);
        vLine(p, L.rates[i].right() - 1, L.rates[i].top(), BAR_H, rule);
        putText(p, f.mono12, lit ? hiInk : ink, L.rates[i].left() + 12, L.rates[i].top() + BAR_H / 2, rateLabels[i]);
    }

    if (realHz_ > 0) {
        const QString v = realHz_ >= 1e6 ? QString::number(realHz_ / 1e6, 'g', 4) + " MHz" : realHz_ >= 1e3 ? QString::number(realHz_ / 1e3, 'g', 4) + " kHz" : QString::number(realHz_, 'g', 4) + " Hz";
        const double right = L.real.right() - 1 - 12;
        const double vw = textWidth(f.mono12, v);
        putText(p, f.mono12, lampRun, right, cy, v, Qt::AlignRight);
        putText(p, f.mono12, inkSoft, right - vw - 4, cy, "real", Qt::AlignRight);
    }
    vLine(p, L.real.right() - 1, L.real.top(), 44, rule);

    const double sx = L.stat.left() + MARGIN;
    const double w = putText(p, f.mono13, ink, sx, cy, "Executed");
    putText(p, f.mono13b, ink, sx + w + 8, cy, QLocale().toString(static_cast<qulonglong>(ticks_)));

    fill(p, QRectF(L.ix, GRID_TOP - 2, L.iw, 2), rule);
}

void Workbench::paintFooter(QPainter& p, const Layout& L) {
    const Fonts& f = fontSet();
    fill(p, QRectF(L.ix, L.footTop, L.iw, 2), rule);
    const double cy = L.footTop + 14;
    double x = L.ix + PAD;
    auto item = [&](const QString& label, const QString& value) {
        x += putText(p, f.mono12, inkSoft, x, cy, label + " ");
        x += putText(p, f.mono12b, ink, x, cy, value);
        x += 24;
    };
    item("PC", hex(regs_[REG_PC]));
    item("SP", hex(regs_[REG_SP]));
    item("Program", program_);
}
