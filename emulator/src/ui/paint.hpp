#pragma once

#include <QFont>
#include <QFontMetricsF>
#include <QKeySequence>
#include <QPainter>
#include <QRectF>
#include <QString>

#include <cmath>

#include "ui/theme.hpp"

namespace ui {

constexpr int MARGIN = 14;
constexpr int PAD = 12;
constexpr int HEADER_TOP = 14;
constexpr int TRANSPORT_TOP = 56;
constexpr int GRID_TOP = 102;
constexpr int BAR_H = 40;
constexpr int SECTION_BAR = 24;
constexpr int FOOTER_H = 26;
constexpr int LEFT_W = 412;
constexpr int RIGHT_W = 412;

constexpr int REG_ROWS = 14;
// row order of Workbench::regs_ (R0..R7 come first)
constexpr int REG_SP = 8;
constexpr int REG_PC = 9;
constexpr int REG_FLAGS = 10;
constexpr int REG_ADDR = 11;
constexpr int REG_IR0 = 12;
constexpr int REG_IR1 = 13;
constexpr int REG_ROW_H = 24;
constexpr double REG_TAG_W = 58.8;
constexpr double REG_HEX_W = 67.2;
constexpr double REG_DEC_W = 58.3;
// the registers table starts below the section bar, inset by PAD
constexpr double REG_TABLE_X = MARGIN + PAD;
constexpr double REG_TABLE_Y = GRID_TOP + SECTION_BAR + PAD;

constexpr int MEM_COLS = 8;
constexpr int MEM_ROWS = 10;
constexpr int MEM_ROW_H = 23;
constexpr int MEM_PAGE = MEM_COLS * MEM_ROWS;
constexpr double MEM_HEAD_H = 22.5;
constexpr double MEM_ADDR_W = 67.2;

constexpr int PORT_COUNT = 3;
constexpr int PORT_BITS = 16;
constexpr int PORT_STRIDE = 106;
// each bit is a little DIP switch, DIP_Y below the top of its port block
constexpr int DIP_W = 22;
constexpr int DIP_H = 34;
constexpr int DIP_Y = 28;

constexpr int LIST_ROW_H = 24;
constexpr int LIST_GUTTER = 22;
constexpr int LIST_LOOKBEHIND = 3;

constexpr double LOGO_H = 24.0;
constexpr double LOGO_ASPECT = 624.1 / 579.09;

constexpr double HZ_MAX = 20e6;
constexpr double HZ_MIN = 1;

constexpr qint64 FLASH_MS = 280;
constexpr qint64 TOAST_MS = 2600;
constexpr qint64 MEASURE_MS = 500;

// ids for Workbench::hover_, one per clickable thing
constexpr int HOVER_NONE = -1;
constexpr int HOVER_LOAD = 1;
constexpr int HOVER_SAMPLES = 2;
constexpr int HOVER_RUN = 3;
constexpr int HOVER_STEP = 4;
constexpr int HOVER_RESET = 5;
constexpr int HOVER_RATE = 10;
constexpr int HOVER_PG_UP = 20;
constexpr int HOVER_PG_DN = 21;
constexpr int HOVER_MEM = 1000;
constexpr int HOVER_DIP = 2000;
constexpr int HOVER_LIST = 3000;

// the clock slider's track is inset this far from both ends of its cell
constexpr double SLIDER_INSET = 18;

constexpr double RATE_VALUES[4] = {1e3, 1e5, 1e6, 1e7};

const char* const REG_NAMES[REG_ROWS] = {"R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "SP", "PC", "FLAGS", "ADDR", "IR_0", "IR_1"};

struct Fonts {
    QFont silk = theme::sans(11, true, 0.66);
    QFont body = theme::sans(14);
    QFont brand = theme::sans(20, true);
    QFont button = theme::sans(15, true);
    QFont display = theme::sans(28, true);
    QFont mono10b = theme::mono(10, true);
    QFont mono11 = theme::mono(11);
    QFont mono12 = theme::mono(12);
    QFont mono12b = theme::mono(12, true);
    QFont mono13 = theme::mono(13);
    QFont mono13b = theme::mono(13, true);
    QFont mono15b = theme::mono(15, true);
};

inline const Fonts& fontSet() {
    static const Fonts fonts;
    return fonts;
}

inline double textWidth(const QFont& font, const QString& text) {
    return QFontMetricsF(font).horizontalAdvance(text);
}

// round to whole pixels so 1px rules stay crisp
inline QRectF snap(const QRectF& r) {
    const double x0 = std::round(r.left()), y0 = std::round(r.top());
    const double x1 = std::round(r.right()), y1 = std::round(r.bottom());
    return QRectF(x0, y0, x1 - x0, y1 - y0);
}

inline void fill(QPainter& p, const QRectF& r, const QColor& c) {
    p.fillRect(snap(r), c);
}

inline void hLine(QPainter& p, double x, double y, double w, const QColor& c) {
    fill(p, QRectF(x, y, w, 1), c);
}

inline void vLine(QPainter& p, double x, double y, double h, const QColor& c) {
    fill(p, QRectF(x, y, 1, h), c);
}

inline void outline(QPainter& p, const QRectF& r, double t, const QColor& c) {
    fill(p, QRectF(r.left(), r.top(), r.width(), t), c);
    fill(p, QRectF(r.left(), r.bottom() - t, r.width(), t), c);
    fill(p, QRectF(r.left(), r.top(), t, r.height()), c);
    fill(p, QRectF(r.right() - t, r.top(), t, r.height()), c);
}

// draws s vertically centred on cy and returns its width
inline double putText(QPainter& p, const QFont& f, const QColor& c, double x, double cy, const QString& s, Qt::Alignment align = Qt::AlignLeft) {
    p.setFont(f);
    p.setPen(c);
    const QFontMetricsF fm(f);
    const double w = fm.horizontalAdvance(s);
    if (align & Qt::AlignRight)
        x -= w;
    else if (align & Qt::AlignHCenter)
        x -= w / 2;
    const double baseline = std::round(cy + (fm.ascent() - fm.descent()) / 2) + 1;
    p.drawText(QPointF(std::round(x), baseline), s);
    return w;
}

inline QString decimal(unsigned v) { return QString::number(v); }

// 16 bits in groups of four, e.g. "0000 0001 1000 0110"
inline QString binary16(uint16_t v) {
    QString s = QString::number(v, 2).rightJustified(16, '0');
    for (int i = 12; i > 0; i -= 4)
        s.insert(i, ' ');
    return s;
}

inline QString keyName(int key) {
    return QKeySequence(key).toString(QKeySequence::PortableText);
}

}
