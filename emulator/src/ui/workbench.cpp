#include "workbench.hpp"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>
#include <cmath>

#include "theme.hpp"
#include "disasm.hpp"

namespace {

using namespace theme;

constexpr int HEADER_TOP = 14;
constexpr int TRANSPORT_TOP = 56;
constexpr int GRID_TOP = 102;
constexpr int SECTION_BAR = 24;
constexpr int FOOTER_H = 26;
constexpr int LEFT_W = 412;
constexpr int RIGHT_W = 412;
constexpr int REG_ROWS = 14;
constexpr int MEM_COLS = 8;
constexpr int MEM_ROWS = 10;
constexpr int PORT_STRIDE = 106;
constexpr double HZ_MAX = 20e6;
constexpr double HZ_MIN = 1;

const char* const REG_NAMES[REG_ROWS] = {"R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "SP", "PC", "FLAGS", "ADDR", "IR_0", "IR_1"};

struct Fonts {
    QFont silk = sans(11, true, 0.66);
    QFont body = sans(14);
    QFont brand = sans(20, true);
    QFont button = sans(15, true);
    QFont mono11 = mono(11);
    QFont mono10b = mono(10, true);
    QFont mono12 = mono(12);
    QFont mono12b = mono(12, true);
    QFont mono13 = mono(13);
    QFont mono13b = mono(13, true);
    QFont mono15b = mono(15, true);
    QFont mono28 = sans(28, true);
};

const Fonts& F() {
    static const Fonts fonts;
    return fonts;
}

double Width(const QFont& font, const QString& text) {
    return QFontMetricsF(font).horizontalAdvance(text);
}

QRectF Snap(const QRectF& r) {
    const double x0 = std::round(r.left()), y0 = std::round(r.top());
    const double x1 = std::round(r.right()), y1 = std::round(r.bottom());
    return QRectF(x0, y0, x1 - x0, y1 - y0);
}

void Fill(QPainter& p, const QRectF& r, const QColor& c) {
    p.fillRect(Snap(r), c);
}

void HLine(QPainter& p, double x, double y, double w, const QColor& c) {
    Fill(p, QRectF(x, y, w, 1), c);
}

void VLine(QPainter& p, double x, double y, double h, const QColor& c) {
    Fill(p, QRectF(x, y, 1, h), c);
}

void Frame(QPainter& p, const QRectF& r, double t, const QColor& c) {
    Fill(p, QRectF(r.left(), r.top(), r.width(), t), c);
    Fill(p, QRectF(r.left(), r.bottom() - t, r.width(), t), c);
    Fill(p, QRectF(r.left(), r.top(), t, r.height()), c);
    Fill(p, QRectF(r.right() - t, r.top(), t, r.height()), c);
}

double Text(QPainter& p, const QFont& f, const QColor& c, double x, double cy, const QString& s, Qt::Alignment align = Qt::AlignLeft) {
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

QString Dec(unsigned v) { return QString::number(v); }

QString Binary16(uint16_t v) {
    QString s = QString::number(v, 2).rightJustified(16, '0');
    for (int i = 12; i > 0; i -= 4)
        s.insert(i, ' ');
    return s;
}

QString KeyName(int key) {
    return QKeySequence(key).toString(QKeySequence::PortableText);
}

}

struct Workbench::Layout {
    double W = 0, H = 0, ix = 0, iw = 0, bottom = 0;

    QRectF brand, load, samples, lamps;
    QRectF lamp[3];
    double lampText[3]{};
    QRectF run, stepBtn, reset;
    QRectF hz, unit, range, rates[4], real, stat;
    QRectF logo;

    double midX = 0, midW = 0, rightX = 0, footTop = 0;
    QRectF bezel, canvas, cap;
    double memSh = 0, memTop = 0, memAddrW = 0, memColW = 0;
    QRectF pgUp, pgDn, goBox;
    double portsSh = 0, portsTop = 0;
    double listSh = 0, listTop = 0;
    int listRows = 0;
    QRectF follow;
    QRectF hint;
};

Workbench::Workbench(Core& core, SimThread& sim, QWidget* parent)
    : QWidget(parent), core_(core), sim_(sim), ram_(ADDRESS_SPACE, 0), screen_(SCREEN_WIDTH, SCREEN_HEIGHT, QImage::Format_RGB32),
      breakpoints_(ADDRESS_SPACE, 0)
{
    screen_.fill(Qt::black);
    logo_ = QPixmap(":/imgs/imgs/logo.png");
    clock_.start();

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    setAttribute(Qt::WA_OpaquePaintEvent);

    hzField_ = new QLineEdit(this);
    hzField_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hzField_->setFont(F().mono15b);
    hzField_->setStyleSheet("QLineEdit{background:#181b1f;color:#f4f5f6;border:0;padding:0 8px;selection-background-color:#4477ff;selection-color:#ffffff;}"
                            "QLineEdit:focus{background:#2d3239;}");
    connect(hzField_, &QLineEdit::textEdited, this, [this] { readHzField(); });
    connect(hzField_, &QLineEdit::editingFinished, this, [this] { setHz(hz_); setFocus(); });

    goField_ = new QLineEdit(this);
    goField_->setPlaceholderText("go to 0x");
    goField_->setFont(F().mono11);
    goField_->setStyleSheet("QLineEdit{background:transparent;color:#ffffff;border:1px solid #ffffff;padding:0 6px;selection-background-color:#ffffff;selection-color:#24489a;}");
    QPalette pal = goField_->palette();
    QColor placeholder(Qt::white);
    placeholder.setAlphaF(0.7);
    pal.setColor(QPalette::PlaceholderText, placeholder);
    goField_->setPalette(pal);
    connect(goField_, &QLineEdit::returnPressed, this, [this] { goToAddress(); setFocus(); });

    editor_ = new QLineEdit(this);
    editor_->setFont(F().mono13b);
    editor_->hide();
    connect(editor_, &QLineEdit::returnPressed, this, [this] { commitEdit(); });
    editor_->installEventFilter(this);

    QSettings settings("Organ16", "Emulator");
    hz_ = std::clamp(settings.value("clock/hz", 1e6).toDouble(), HZ_MIN, HZ_MAX);
    loadBindings();
    setHz(hz_);
    refresh();
    lastRegs_ = regs_;
}

void Workbench::loadBindings() {
    QSettings settings("Organ16", "Emulator");
    settings.beginGroup("ioKeyBindings");
    const char names[3] = {'A', 'B', 'C'};
    for (int p = 0; p < 3; ++p)
        for (int b = 0; b < 16; ++b) {
            const int key = settings.value(QString("%1_%2").arg(names[p]).arg(b), 0).toInt();
            if (key)
                binds_[key] = {p, b};
        }
    if (binds_.isEmpty()) {
        binds_[Qt::Key_Z] = {0, 0};
        binds_[Qt::Key_X] = {0, 8};
        binds_[Qt::Key_Up] = {1, 0};
        binds_[Qt::Key_Down] = {1, 8};
    }
}

void Workbench::saveBindings() const {
    QSettings settings("Organ16", "Emulator");
    settings.remove("ioKeyBindings");
    settings.beginGroup("ioKeyBindings");
    const char names[3] = {'A', 'B', 'C'};
    for (auto it = binds_.begin(); it != binds_.end(); ++it)
        settings.setValue(QString("%1_%2").arg(names[it.value().first]).arg(it.value().second), it.key());
}

QString Workbench::keyLabel(int port, int bit) const {
    if (binding_.first == port && binding_.second == bit)
        return "key?";
    for (auto it = binds_.begin(); it != binds_.end(); ++it)
        if (it.value().first == port && it.value().second == bit)
            return KeyName(it.key());
    return QString();
}

void Workbench::setSamples(const QStringList& paths) {
    samples_ = paths;
    update();
}

void Workbench::toast(const QString& text) {
    toast_ = text;
    toastUntil_ = clock_.elapsed() + 2600;
    update();
}

void Workbench::refresh() {
    bool screenChanged = false;
    {
        auto guard = core_.lock();
        for (int i = 0; i < 8; ++i)
            regs_[i] = core_.r[i];
        regs_[8] = core_.sp;
        regs_[9] = core_.pc;
        regs_[10] = core_.flags;
        regs_[11] = core_.ramAddr;
        regs_[12] = core_.ir0;
        regs_[13] = core_.ir1;
        io_ = core_.io;
        ticks_ = core_.tickCount;
        halted_ = decodeTable[core_.ir0 >> 9].hlt;
        std::copy(core_.ram.begin(), core_.ram.end(), ram_.begin());
        touched_ = core_.touched;
        if (core_.screenDirty) {
            core_.screenDirty = false;
            screenChanged = true;
            static const auto tables = [] {
                std::array<std::array<uint32_t, 64>, 2> t{};
                for (uint32_t i = 0; i < 32; ++i) t[0][i] = (i * 255) / 31;
                for (uint32_t i = 0; i < 64; ++i) t[1][i] = (i * 255) / 63;
                return t;
            }();
            int lit = 0;
            for (size_t y = 0; y < SCREEN_HEIGHT; ++y) {
                uint32_t* line = reinterpret_cast<uint32_t*>(screen_.scanLine(static_cast<int>(y)));
                const uint16_t* source = core_.screen.data() + y * SCREEN_WIDTH;
                for (size_t x = 0; x < SCREEN_WIDTH; ++x) {
                    const uint16_t v = source[x];
                    lit += v != 0;
                    line[x] = 0xFF000000u | (tables[0][v >> 11] << 16) | (tables[1][(v >> 5) & 0x3F] << 8) | tables[0][v & 0x1F];
                }
            }
            lit_ = lit;
        }
    }
    (void)screenChanged;

    running_ = sim_.automaticEnabled();
    hit_ = sim_.breakHit();

    const qint64 now = clock_.elapsed();
    for (int i = 0; i < REG_ROWS; ++i) {
        if (regs_[i] != lastRegs_[i] && !(running_ && hz_ > 100))
            flashUntil_[i] = now + 280;
        lastRegs_[i] = regs_[i];
    }

    if (!running_) {
        realHz_ = 0;
        measureAt_ = now;
        measureTicks_ = sim_.halfTicksRun();
    }
    else if (now - measureAt_ >= 500) {
        const uint64_t done = sim_.halfTicksRun();
        realHz_ = (done - measureTicks_) / 2.0 / ((now - measureAt_) / 1000.0);
        measureTicks_ = done;
        measureAt_ = now;
    }

    update();
}

void Workbench::togglePlay() {
    if (running_) {
        sim_.setAutomatic(false);
        return;
    }
    if (halted_)
        return;
    sim_.clearBreakHit();
    sim_.setFrequency(hz_);
    sim_.setAutomatic(true);
}

void Workbench::playAt(double hz) {
    setHz(hz);
    togglePlay();
}

void Workbench::step(bool halfTick) {
    sim_.setAutomatic(false);
    sim_.clearBreakHit();
    {
        auto guard = core_.lock();
        if (halfTick) {
            core_.run(1);
        }
        else {
            for (int i = 0; i < 64; ++i) {
                core_.run(1);
                if (i >= 1 && !core_.clockValue && !core_.ffExt && !core_.ffSpChange && !core_.ffRegAddr && !core_.ffJsr)
                    break;
                if (decodeTable[core_.ir0 >> 9].hlt)
                    break;
            }
        }
    }
    refresh();
}

void Workbench::resetCore() {
    sim_.setAutomatic(false);
    sim_.clearBreakHit();
    {
        auto guard = core_.lock();
        core_.reset();
    }
    emit resetPerformed();
    refresh();
    lastRegs_ = regs_;
    flashUntil_.fill(0);
}

bool Workbench::loadProgram(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "File Error", "Could not open the selected file.");
        return false;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    std::vector<uint16_t> words;
    bool text = true;
    for (char c : bytes) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (!(std::isxdigit(u) || u == ' ' || u == '\n' || u == '\r' || u == '\t')) {
            text = false;
            break;
        }
    }
    if (text) {
        const QStringList list = QString::fromLatin1(bytes).split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        words.reserve(list.size());
        for (const QString& token : list) {
            bool ok = false;
            const uint16_t value = token.toUShort(&ok, 16);
            if (!ok) {
                QMessageBox::warning(this, "File Error", "Invalid word in file : " + token);
                return false;
            }
            words.push_back(value);
        }
    }
    else {
        for (qsizetype i = 0; i + 1 < bytes.size(); i += 2)
            words.push_back(static_cast<uint16_t>(static_cast<uint8_t>(bytes[i]) | (static_cast<uint8_t>(bytes[i + 1]) << 8)));
    }

    if (words.size() > ADDRESS_SPACE) {
        QMessageBox::warning(this, "File Error", "Invalid file size : " + QString::number(words.size()));
        return false;
    }
    const size_t given = words.size();
    words.resize(ADDRESS_SPACE, 0);

    sim_.setAutomatic(false);
    sim_.clearBreakHit();
    {
        auto guard = core_.lock();
        core_.loadRam(words);
        core_.reset();
    }
    program_ = QFileInfo(path).fileName();
    base_ = 0;
    sel_ = -1;
    refresh();
    lastRegs_ = regs_;
    flashUntil_.fill(0);
    emit resetPerformed();
    const int nonZero = static_cast<int>(std::count_if(words.begin(), words.end(), [](uint16_t w) { return w != 0; }));
    toast(program_ + ": " + QString::number(nonZero) + " non-zero words" + (given != ADDRESS_SPACE ? QString(" (file has %1 words, expected 65536)").arg(given) : QString()));
    return true;
}

void Workbench::setHz(double hz, bool fromField) {
    hz_ = std::clamp(hz, HZ_MIN, HZ_MAX);
    sim_.setFrequency(hz_);
    if (!fromField) {
        const int unit = hz_ >= 1e6 ? 2 : hz_ >= 1e3 ? 1 : 0;
        QSignalBlocker blocker(hzField_);
        hzField_->setText(QString::number(hz_ / std::pow(1000.0, unit), 'g', 4));
        hzField_->setProperty("unit", unit);
    }
    QSettings("Organ16", "Emulator").setValue("clock/hz", hz_);
    update();
}

void Workbench::readHzField() {
    static const QRegularExpression re("^\\s*([0-9]*\\.?[0-9]+(?:e[+-]?\\d+)?)\\s*([kKmM]?)");
    const QRegularExpressionMatch m = re.match(QString(hzField_->text()).replace(',', '.'));
    if (!m.hasMatch())
        return;
    int unit = hzField_->property("unit").toInt();
    const QString suffix = m.captured(2).toLower();
    if (suffix == "k") unit = 1;
    else if (suffix == "m") unit = 2;
    if (!suffix.isEmpty()) {
        QSignalBlocker blocker(hzField_);
        hzField_->setText(m.captured(1));
    }
    hzField_->setProperty("unit", unit);
    setHz(m.captured(1).toDouble() * std::pow(1000.0, unit), true);
}

void Workbench::applyUnit(int unit) {
    hzField_->setProperty("unit", unit);
    setHz(hzField_->text().replace(',', '.').toDouble() * std::pow(1000.0, unit), true);
}

void Workbench::goToAddress() {
    QString text = goField_->text().trimmed();
    if (text.startsWith("0x", Qt::CaseInsensitive))
        text = text.mid(2);
    bool ok = false;
    const uint value = text.toUInt(&ok, 16);
    if (!ok)
        return;
    base_ = value & 0xFFF8;
    sel_ = value & 0xFFFF;
    update();
}

void Workbench::setBreakpoint(uint16_t address, bool enabled) {
    breakpoints_[address] = enabled;
    sim_.setBreakpoint(address, enabled);
}

void Workbench::setPortBit(int port, int bit, bool on) {
    auto guard = core_.lock();
    if (on)
        core_.io[port] |= 1u << bit;
    else
        core_.io[port] &= ~(1u << bit);
    io_[port] = core_.io[port];
}

uint16_t Workbench::sweepStart(uint16_t pc, int back) const {
    for (int k = back * 2; k >= 1; --k) {
        uint16_t p = static_cast<uint16_t>(pc - k);
        int guard = 0;
        while (static_cast<uint16_t>(pc - p) > 0 && static_cast<uint16_t>(pc - p) < 64 && guard++ < 64)
            p = static_cast<uint16_t>(p + Disassemble(ram_.data(), p).words);
        if (p == pc)
            return static_cast<uint16_t>(pc - k);
    }
    return pc;
}

Workbench::Layout Workbench::layout() const {
    Layout L;
    L.W = width();
    L.H = height();
    L.ix = 14;
    L.iw = L.W - 28;
    L.bottom = L.H - 14;

    const Fonts& f = F();
    double x = L.ix;

    const double logoW = 24.0 * 624.1 / 579.09;
    L.brand = QRectF(x, HEADER_TOP, 14 + logoW + 10 + Width(f.brand, "Organ 16") + 14 + 1, 40);
    L.logo = QRectF(x + 14, HEADER_TOP + 8, logoW, 24);
    x += L.brand.width();
    L.load = QRectF(x, HEADER_TOP, 14 + Width(f.body, "Load .bin") + 14 + 1, 40);
    x += L.load.width();
    double sampleText = Width(f.body, "Samples");
    for (const QString& s : samples_)
        sampleText = std::max(sampleText, Width(f.body, QFileInfo(s).dir().dirName()));
    L.samples = QRectF(x, HEADER_TOP, 14 + sampleText + 14 + 1, 40);

    const char* lampNames[3] = {"RUN", "BRK", "HALT"};
    double lampW = 28 + 32;
    for (int i = 0; i < 3; ++i)
        lampW += 12 + 6 + Width(f.silk, lampNames[i]);
    L.lamps = QRectF(L.ix + L.iw - lampW, HEADER_TOP, lampW, 40);
    double lx = L.lamps.left() + 14;
    for (int i = 0; i < 3; ++i) {
        L.lamp[i] = QRectF(lx, HEADER_TOP + 14, 12, 12);
        L.lampText[i] = lx + 18;
        lx += 18 + Width(f.silk, lampNames[i]) + 16;
    }

    x = L.ix;
    auto button = [&](const QString& label) {
        const double w = 18 + Width(f.button, label) + 18 + 1;
        QRectF r(x, TRANSPORT_TOP, w, 40);
        x += w;
        return r;
    };
    L.run = button(running_ ? "Pause" : "Run");
    L.stepBtn = button("Step");
    L.reset = button("Reset");

    L.hz = QRectF(x, TRANSPORT_TOP, 92, 40);
    x += 92;
    L.unit = QRectF(x, TRANSPORT_TOP, 62, 40);
    x += 62;
    L.range = QRectF(x, TRANSPORT_TOP + 2, 150, 40);
    x += 150;
    const char* rateLabels[4] = {"1k", "100k", "1M", "10M"};
    for (int i = 0; i < 4; ++i) {
        const double w = 12 + Width(f.mono12, rateLabels[i]) + 12 + 1;
        L.rates[i] = QRectF(x, TRANSPORT_TOP + 2, w, 40);
        x += w;
    }
    const QString realText = realHz_ > 0 ? "real " : QString();
    const double realW = std::max(96.0, 12 + Width(f.mono12, realText) + (realHz_ > 0 ? 64 : 0) + 12 + 1);
    L.real = QRectF(x, TRANSPORT_TOP, realW, 44);

    const double statW = 14 + Width(f.mono13, "Executed") + 8 + Width(f.mono13b, QLocale().toString(static_cast<qulonglong>(ticks_))) + 14;
    L.stat = QRectF(L.ix + L.iw - statW, TRANSPORT_TOP, statW, 44);

    L.midX = L.ix + LEFT_W;
    L.rightX = L.ix + L.iw - RIGHT_W;
    L.midW = L.rightX - L.midX;

    const double areaBottom = L.bottom - FOOTER_H;
    const double availH = areaBottom - GRID_TOP;
    const double fixedMid = SECTION_BAR + 12 + 6 + 15 + 12 + SECTION_BAR + 22.5 + MEM_ROWS * 23;
    const double bezel = std::max(128.0, std::min({416.0, L.midW - 1 - 24, availH - fixedMid}));
    L.bezel = QRectF(L.midX + (L.midW - 1 - bezel) / 2, GRID_TOP + SECTION_BAR + 12, bezel, bezel);
    L.canvas = L.bezel.adjusted(6, 6, -6, -6);
    L.cap = QRectF(L.midX + 12, L.bezel.bottom() + 6, L.midW - 1 - 24, 15);
    L.memSh = L.cap.bottom() + 12;
    L.memTop = L.memSh + SECTION_BAR;
    L.memAddrW = 67.2;
    L.memColW = (L.midW - 1 - L.memAddrW) / MEM_COLS;

    const double pgW = Width(f.mono11, "PG-") + 12 + 2;
    const double right = L.midX + L.midW - 1 - 12;
    L.pgDn = QRectF(right - pgW, L.memSh + 3, pgW, 18);
    L.pgUp = QRectF(L.pgDn.left() - pgW, L.memSh + 3, pgW, 18);
    L.goBox = QRectF(L.pgUp.left() - 4 - 70, L.memSh + 3, 70, 18);

    L.portsSh = GRID_TOP + SECTION_BAR + 12 + REG_ROWS * 24 + 12;
    L.portsTop = L.portsSh + SECTION_BAR + 12;
    L.listSh = GRID_TOP;
    L.listTop = L.listSh + SECTION_BAR;
    L.listRows = std::max(5, static_cast<int>((availH - SECTION_BAR - 44) / 24));
    L.hint = QRectF(L.rightX, L.listTop + L.listRows * 24 + 4, RIGHT_W, 44);
    const double followW = 20 + 6 + Width(f.mono11, "follow PC");
    L.follow = QRectF(L.rightX + RIGHT_W - 12 - followW, L.listSh + 2.5, followW, 19);

    L.footTop = std::max({L.bottom - FOOTER_H, L.hint.bottom(), L.portsTop + 3.0 * PORT_STRIDE});
    return L;
}

void Workbench::resizeEvent(QResizeEvent*) {
    placeEditors(layout());
}

void Workbench::placeEditors(const Layout& L) {
    const QRect hz = Snap(L.hz).toRect();
    if (hzField_->geometry() != hz)
        hzField_->setGeometry(hz);
    const QRect go = Snap(L.goBox).toRect();
    if (goField_->geometry() != go)
        goField_->setGeometry(go);
}

void Workbench::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    const Layout L = layout();
    placeEditors(L);

    p.fillRect(rect(), paper);
    Fill(p, QRectF(12, 12, L.W - 24, L.H - 24), panel);
    Frame(p, QRectF(12, 12, L.W - 24, L.H - 24), 2, rule);

    paintHeader(p, L);
    paintTransport(p, L);

    Fill(p, QRectF(L.ix, GRID_TOP - 2, L.iw, 2), rule);
    VLine(p, L.ix + LEFT_W - 1, GRID_TOP, L.footTop - GRID_TOP, rule);
    VLine(p, L.midX + L.midW - 1, GRID_TOP, L.footTop - GRID_TOP, rule);

    paintRegisters(p, L);
    paintScreen(p, L);
    paintMemory(p, L);
    paintPorts(p, L);
    paintListing(p, L);
    paintFooter(p, L);

    if (!toast_.isEmpty() && clock_.elapsed() < toastUntil_) {
        const double w = Width(F().mono13, toast_) + 32;
        const QRectF r((L.W - w) / 2, L.H - 24 - 34, w, 34);
        Fill(p, r, hi);
        Text(p, F().mono13, hiInk, r.center().x(), r.center().y(), toast_, Qt::AlignHCenter);
    }
    if (dropping_) {
        p.setOpacity(0.92);
        p.fillRect(rect(), ink);
        p.setOpacity(1);
        Text(p, F().mono28, paper, L.W / 2, L.H / 2, "Drop a .bin to load it", Qt::AlignHCenter);
    }
}

void Workbench::paintHeader(QPainter& p, const Layout& L) {
    const Fonts& f = F();
    const double cy = HEADER_TOP + 20;

    auto cell = [&](const QRectF& r, const QString& label, int id, const QFont& font) {
        const bool over = hover_ == id;
        if (over)
            Fill(p, QRectF(r.left(), r.top(), r.width() - 1, r.height()), hi);
        Text(p, font, over ? hiInk : ink, r.left() + 14, cy, label);
        VLine(p, r.right() - 1, r.top(), r.height(), rule);
    };

    p.drawPixmap(Snap(L.logo).toRect(), logo_.scaled(Snap(L.logo).size().toSize() * 4, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    Text(p, f.brand, ink, L.brand.left() + 14 + L.logo.width() + 10, cy + 1, "Organ 16");
    VLine(p, L.brand.right() - 1, L.brand.top(), 40, rule);

    cell(L.load, "Load .bin", 1, f.body);
    cell(L.samples, "Samples", 2, f.body);

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
        Text(p, f.silk, ink, L.lampText[i], cy, names[i]);
    }
    p.setRenderHint(QPainter::Antialiasing, false);

    Fill(p, QRectF(L.ix, TRANSPORT_TOP - 2, L.iw, 2), rule);
}

void Workbench::paintTransport(QPainter& p, const Layout& L) {
    const Fonts& f = F();

    auto button = [&](const QRectF& r, const QString& label, int id, bool go) {
        const bool over = hover_ == id;
        QColor bg = go ? lampRun : panel;
        QColor fg = go ? black : ink;
        if (over) {
            bg = hi;
            fg = hiInk;
        }
        Fill(p, QRectF(r.left(), r.top(), r.width() - 1, r.height()), bg);
        VLine(p, r.right() - 1, r.top(), r.height(), rule);
        const double cy = r.top() + 20;
        Text(p, f.button, fg, r.left() + 18, cy, label);
    };
    button(L.run, running_ ? "Pause" : "Run", 3, true);
    button(L.stepBtn, "Step", 4, false);
    button(L.reset, "Reset", 5, false);

    const double cy = TRANSPORT_TOP + 22;
    VLine(p, L.hz.right() - 1, L.hz.top(), 40, rule);

    Fill(p, L.unit, well);
    const int unit = hzField_->property("unit").toInt();
    Text(p, f.mono15b, hi, L.unit.left() + 8, TRANSPORT_TOP + 20, unit == 2 ? "MHz" : unit == 1 ? "kHz" : "Hz");
    VLine(p, L.unit.right() - 1, L.unit.top(), 40, rule);

    Fill(p, L.range, panel);
    VLine(p, L.range.right() - 1, L.range.top(), 40, rule);
    const double frac = std::clamp(std::log10(std::max(hz_, 1.0)) / std::log10(HZ_MAX), 0.0, 1.0);
    const double tx0 = L.range.left() + 10 + 8, tx1 = L.range.right() - 1 - 10 - 8;
    const double tcx = tx0 + (tx1 - tx0) * frac, tcy = L.range.top() + 20;
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#d9d9d9"));
    p.drawRoundedRect(QRectF(tx0, tcy - 2, tx1 - tx0, 4), 2, 2);
    p.setBrush(hi);
    p.drawRoundedRect(QRectF(tx0, tcy - 2, tcx - tx0, 4), 2, 2);
    p.drawEllipse(QPointF(tcx, tcy), 8, 8);
    p.setRenderHint(QPainter::Antialiasing, false);

    const double rateValues[4] = {1e3, 1e5, 1e6, 1e7};
    const char* rateLabels[4] = {"1k", "100k", "1M", "10M"};
    for (int i = 0; i < 4; ++i) {
        const bool selected = std::abs(hz_ - rateValues[i]) < 0.5;
        const bool over = hover_ == 10 + i;
        const bool lit = selected || over;
        Fill(p, QRectF(L.rates[i].left(), L.rates[i].top(), L.rates[i].width() - 1, L.rates[i].height()), lit ? hi : panel);
        VLine(p, L.rates[i].right() - 1, L.rates[i].top(), 40, rule);
        Text(p, f.mono12, lit ? hiInk : ink, L.rates[i].left() + 12, L.rates[i].top() + 20, rateLabels[i]);
    }

    if (realHz_ > 0) {
        const QString v = realHz_ >= 1e6 ? QString::number(realHz_ / 1e6, 'g', 4) + " MHz" : realHz_ >= 1e3 ? QString::number(realHz_ / 1e3, 'g', 4) + " kHz" : QString::number(realHz_, 'g', 4) + " Hz";
        const double right = L.real.right() - 1 - 12;
        const double vw = Width(f.mono12, v);
        Text(p, f.mono12, lampRun, right, cy, v, Qt::AlignRight);
        Text(p, f.mono12, inkSoft, right - vw - 4, cy, "real", Qt::AlignRight);
    }
    VLine(p, L.real.right() - 1, L.real.top(), 44, rule);

    const double sx = L.stat.left() + 14;
    const double w = Text(p, f.mono13, ink, sx, cy, "Executed");
    Text(p, f.mono13b, ink, sx + w + 8, cy, QLocale().toString(static_cast<qulonglong>(ticks_)));

    Fill(p, QRectF(L.ix, GRID_TOP - 2, L.iw, 2), rule);
}

int Workbench::hitRegisterRow(const Layout&, const QPointF& pt) const {
    const double x = pt.x() - 26, y = pt.y() - 138;
    if (x < 58.8 || x > 126 || y < 0 || y >= REG_ROWS * 24)
        return -1;
    return static_cast<int>(y / 24);
}

void Workbench::paintRegisters(QPainter& p, const Layout& L) {
    const Fonts& f = F();
    const double sx = L.ix;
    Fill(p, QRectF(sx, GRID_TOP, LEFT_W - 1, SECTION_BAR), bar);
    Text(p, f.silk, barInk, sx + 12, GRID_TOP + 12, "REGISTERS");
    Text(p, f.silk, mix(bar, barInk, 0.8), sx + LEFT_W - 1 - 12, GRID_TOP + 12, "DOUBLE-CLICK TO EDIT", Qt::AlignRight);

    const double tx = sx + 12, ty = GRID_TOP + SECTION_BAR + 12;
    const double tagW = 58.8, hxW = 67.2, dcW = 58.3, tableW = LEFT_W - 1 - 24.0;
    const qint64 now = clock_.elapsed();

    for (int i = 0; i < REG_ROWS; ++i) {
        const double y = ty + i * 24;
        const double cy = y + 12;
        const uint16_t v = regs_[i];
        const QString name = REG_NAMES[i];
        const bool general = i < 8, pointer = i >= 8 && i < 12;
        const QColor tag = general ? regGeneral : pointer ? regPointer : regInstruction;

        const bool flash = flashUntil_[i] > now;
        const QColor rowBg = flash ? hi : QColor();
        const QColor fg = flash ? hiInk : QColor();

        if (flash)
            Fill(p, QRectF(tx + tagW, y, tableW - tagW, 24), hi);
        Fill(p, QRectF(tx, y, tagW, 24), tag);
        HLine(p, tx + tagW, y + 23, tableW - tagW, ruleSoft);
        if (i == 8 || i == 12)
            HLine(p, tx, y, tableW, rule);
        VLine(p, tx + tagW - 1, y, 24, rule);
        Text(p, f.mono13b, regInk, tx + 8, cy, name);

        const QString hx = i == 10 ? "0b" + QString::number(v & 0xF, 2).rightJustified(4, '0') : Hex(v);
        if (editKind_ == EditRegister && editIndex_ == i) {
        }
        else
            Text(p, f.mono13b, flash ? fg : ink, tx + tagW + 8, cy, hx);

        const double dx = tx + tagW + hxW;
        if (i == 12) {
            const Insn insn = [&] {
                std::vector<uint16_t> tmp(2);
                tmp[0] = regs_[12];
                tmp[1] = regs_[13];
                return Disassemble(tmp.data(), 0);
            }();
            Text(p, f.mono13, flash ? fg : ink, dx + 8, cy, FormatInsn(insn));
        }
        else if (i == 13) {
            Text(p, f.mono13, flash ? fg : inkSoft, dx + 8, cy, "extension word");
        }
        else {
            if (i != 10)
                Text(p, f.mono13, flash ? fg : inkSoft, dx + dcW - 8, cy, Dec(v), Qt::AlignRight);
            const double bx = dx + dcW + 8;
            if (i == 10) {
                double fx = bx;
                const char* flagNames[4] = {"Z", "N", "C", "O"};
                for (int k = 0; k < 4; ++k) {
                    const bool on = (v >> k) & 1;
                    fx += Text(p, on ? f.mono12b : f.mono12, flash ? fg : on ? ink : ruleSoft, fx, cy, flagNames[k]);
                    fx += Width(f.mono12, " ");
                }
            }
            else
                Text(p, f.mono12, flash ? fg : inkSoft, bx, cy, Binary16(v));
        }
        (void)rowBg;
    }
}

void Workbench::paintScreen(QPainter& p, const Layout& L) {
    const Fonts& f = F();
    Fill(p, QRectF(L.midX, GRID_TOP, L.midW - 1, SECTION_BAR), bar);
    Text(p, f.silk, barInk, L.midX + 12, GRID_TOP + 12, "SCREEN");
    Text(p, f.silk, mix(bar, barInk, 0.8), L.midX + L.midW - 1 - 12, GRID_TOP + 12, "128 X 128 RGB565", Qt::AlignRight);

    Fill(p, L.bezel, rule);
    Fill(p, L.canvas, black);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(Snap(L.canvas), screen_);

    Text(p, f.mono12, inkFaint, L.cap.left(), L.cap.center().y(), "0x8000 - 0xBFFF");
    Text(p, f.mono12, inkFaint, L.cap.right(), L.cap.center().y(), QString::number(lit_) + " lit", Qt::AlignRight);
}

int Workbench::hitMemoryCell(const Layout& L, const QPointF& pt) const {
    const double x = pt.x() - L.midX, y = pt.y() - (L.memTop + 22.5);
    if (x < L.memAddrW || x >= L.memAddrW + MEM_COLS * L.memColW || y < 0 || y >= MEM_ROWS * 23)
        return -1;
    const int col = static_cast<int>((x - L.memAddrW) / L.memColW);
    const int row = static_cast<int>(y / 23);
    return (base_ + row * MEM_COLS + col) & 0xFFFF;
}

void Workbench::paintMemory(QPainter& p, const Layout& L) {
    const Fonts& f = F();
    const double mx = L.midX, mw = L.midW - 1;
    Fill(p, QRectF(mx, L.memSh, mw, SECTION_BAR), bar);
    Text(p, f.silk, barInk, mx + 12, L.memSh + 12, "MEMORY");

    auto tool = [&](const QRectF& r, const QString& label, int id) {
        const bool over = hover_ == id;
        if (over)
            Fill(p, r, barInk);
        Frame(p, r, 1, barInk);
        Text(p, f.mono11, over ? bar : barInk, r.center().x(), r.center().y(), label, Qt::AlignHCenter);
    };
    tool(L.pgUp, "PG-", 20);
    tool(L.pgDn, "PG+", 21);

    const double top = L.memTop;
    Fill(p, QRectF(mx, top, mw, 22.5), well);
    HLine(p, mx, top + 21, mw, rule);
    Text(p, f.mono13, inkFaint, mx + L.memAddrW / 2, top + 11, "Addr", Qt::AlignHCenter);
    for (int c = 0; c < MEM_COLS; ++c)
        Text(p, f.mono13, inkFaint, mx + L.memAddrW + c * L.memColW + L.memColW / 2, top + 11, "+" + QString::number(c), Qt::AlignHCenter);

    const uint16_t pc = regs_[9];
    for (int r = 0; r < MEM_ROWS; ++r) {
        const double y = top + 22.5 + r * 23;
        const double cy = y + 11.5;
        const uint16_t a0 = static_cast<uint16_t>(base_ + r * MEM_COLS);
        Fill(p, QRectF(mx, y, L.memAddrW, 23), well);
        VLine(p, mx + L.memAddrW - 1, y, 23, rule);
        HLine(p, mx, y + 22, mw, ruleSoft);
        Text(p, f.mono13, inkSoft, mx + 8, cy, Hex(a0));
        for (int c = 0; c < MEM_COLS; ++c) {
            const uint16_t a = static_cast<uint16_t>(a0 + c);
            const double x = mx + L.memAddrW + c * L.memColW;
            const QRectF cell(x, y, L.memColW, 23);
            const bool touchedCell = (touched_[a >> 6] >> (a & 63)) & 1;
            const bool isPc = a == pc;
            const bool over = hover_ == 1000 + r * MEM_COLS + c;
            if (isPc)
                Fill(p, cell, hi);
            else if (over)
                Fill(p, cell, well);
            if (c < MEM_COLS - 1)
                VLine(p, x + L.memColW - 1, y, 23, ruleSoft);
            HLine(p, x, y + 22, L.memColW, ruleSoft);
            if (a == sel_)
                Frame(p, cell, 2, focus);
            if (editKind_ == EditMemory && editIndex_ == a)
                continue;
            Text(p, touchedCell || isPc ? f.mono13b : f.mono13, isPc ? hiInk : touchedCell ? ink : inkFaint, x + L.memColW / 2, cy, Hex(ram_[a]).mid(2), Qt::AlignHCenter);
        }
    }
}

void Workbench::paintPorts(QPainter& p, const Layout& L) {
    const Fonts& f = F();
    Fill(p, QRectF(L.ix, L.portsSh, LEFT_W - 1, SECTION_BAR), bar);
    Text(p, f.silk, barInk, L.ix + 12, L.portsSh + 12, "IO PORTS");
    Text(p, f.silk, mix(bar, barInk, 0.8), L.ix + LEFT_W - 1 - 12, L.portsSh + 12, "CLICK: TOGGLE / RIGHT-CLICK: BIND KEY", Qt::AlignRight);

    const double px = L.ix + 12, pw = LEFT_W - 1 - 24;
    const char names[3] = {'A', 'B', 'C'};
    for (int port = 0; port < 3; ++port) {
        const double top = L.portsTop + port * PORT_STRIDE;
        Text(p, f.silk, ink, px, top + 10, QString("PORT ") + names[port]);
        Text(p, f.mono13b, ink, px + pw, top + 10, Hex(io_[port]), Qt::AlignRight);
        HLine(p, px, top + 19, pw, rule);
        for (int col = 0; col < 16; ++col) {
            const int bit = 15 - col;
            const QRectF dip = dipRect(L, port, col);
            const double cx = px + pw / 16 * col + pw / 32;
            const double by = dip.top();
            Fill(p, dip, well);
            Frame(p, dip, 1, rule);
            const bool on = (io_[port] >> bit) & 1;
            if (hover_ == 2000 + port * 16 + bit)
                Frame(p, dip.adjusted(-1, -1, 1, 1), 1, hi);
            Fill(p, QRectF(dip.left() + 3, dip.top() + (on ? 3 : 17), 16, 14), on ? bitOn : inkFaint);
            Text(p, f.mono11, inkSoft, cx, by + 36 + 7, QString::number(bit), Qt::AlignHCenter);
            const QString label = keyLabel(port, bit);
            if (!label.isEmpty()) {
                const bool binding = binding_.first == port && binding_.second == bit;
                const double lw = std::max(12.0, Width(f.mono10b, label));
                const QRectF kr(cx - lw / 2, by + 52, lw, 12);
                if (binding)
                    Fill(p, kr, hi);
                Text(p, f.mono10b, binding ? black : inkFaint, cx, by + 58, label, Qt::AlignHCenter);
            }
        }
    }
}

QRectF Workbench::dipRect(const Layout& L, int port, int col) const {
    const double px = L.ix + 12, pw = LEFT_W - 1 - 24;
    const double cx = px + pw / 16 * col + pw / 32;
    return QRectF(std::round(cx - 11), L.portsTop + port * PORT_STRIDE + 28, 22, 34);
}

int Workbench::hitDip(const Layout& L, const QPointF& pt) const {
    for (int port = 0; port < 3; ++port)
        for (int col = 0; col < 16; ++col)
            if (dipRect(L, port, col).contains(pt))
                return port * 16 + (15 - col);
    return -1;
}

void Workbench::paintListing(QPainter& p, const Layout& L) {
    const Fonts& f = F();
    Fill(p, QRectF(L.rightX, L.listSh, RIGHT_W, SECTION_BAR), bar);
    Text(p, f.silk, barInk, L.rightX + 12, L.listSh + 12, "LISTING");

    const QRectF cb(L.follow.left() + 4, L.follow.top() + 3, 13, 13);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    if (follow_) {
        p.setBrush(hi);
        p.drawRoundedRect(cb, 2, 2);
        QPen pen(Qt::white, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.drawPolyline(QPolygonF({QPointF(cb.left() + 3, cb.top() + 7), QPointF(cb.left() + 5.5, cb.top() + 9.5), QPointF(cb.left() + 10, cb.top() + 4)}));
    }
    else {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(barInk, 1));
        p.drawRoundedRect(cb.adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
    }
    p.setRenderHint(QPainter::Antialiasing, false);
    Text(p, f.mono11, barInk, L.follow.left() + 26, L.follow.center().y(), "follow PC");

    const uint16_t pc = regs_[9];
    if (follow_)
        const_cast<Workbench*>(this)->listBase_ = sweepStart(pc, 3);
    uint16_t a = listBase_;
    for (int i = 0; i < L.listRows; ++i) {
        const double y = L.listTop + i * 24;
        const double cy = y + 11.5;
        const Insn insn = Disassemble(ram_.data(), a);
        const bool cur = a == pc;
        const bool over = hover_ == 3000 + i;
        const bool hasBp = breakpoints_[a];
        if (cur)
            Fill(p, QRectF(L.rightX, y, RIGHT_W, 24), hi);
        else if (over)
            Fill(p, QRectF(L.rightX, y, RIGHT_W, 24), well);
        HLine(p, L.rightX, y + 23, RIGHT_W, ruleSoft);

        const QRectF bp(L.rightX + 6, std::round(y + 6.5), 10, 10);
        if (hasBp) {
            Fill(p, bp, lampHalt);
        }
        else
            Frame(p, bp, 1, over ? ink : ruleSoft);

        const QColor soft = cur ? mix(hi, hiInk, 0.75) : inkSoft;
        const QColor faint = cur ? mix(hi, hiInk, 0.75) : inkFaint;
        Text(p, f.mono13, soft, L.rightX + 22, cy, Hex(a));
        QString words = Hex(insn.w).mid(2);
        if (insn.words == 2)
            words += " " + Hex(insn.imm).mid(2);
        Text(p, f.mono13, faint, L.rightX + 80, cy, words);

        double x = L.rightX + 162;
        const double opW = std::max(60.0, Width(f.mono13b, insn.name));
        Text(p, f.mono13b, cur ? hiInk : insn.w == 0 ? inkFaint : ink, x, cy, insn.name);
        x += opW;
        for (const OperandPart& part : Operands(insn)) {
            const double w = Text(p, f.mono13, cur ? hiInk : ink, x, cy, part.text);
            if (part.address)
                HLine(p, x, std::round(cy + 7), w, cur ? hiInk : ink);
            x += w;
        }
        if (a == sel_ && !cur)
            Frame(p, QRectF(L.rightX, y, RIGHT_W, 24), 2, focus);
        a = static_cast<uint16_t>(a + insn.words);
    }

    const QString hintText = "Decoded from RAM. Click the square to set a breakpoint; click a row to find it in memory.";
    p.setFont(f.mono12);
    const QRectF hr(L.hint.left() + 12, L.hint.top() + 10, L.hint.width() - 24, 32);
    QFontMetricsF fm(f.mono12);
    QStringList words2 = hintText.split(' ');
    QString line;
    double ly = hr.top() + 8;
    for (const QString& word : words2) {
        const QString trial = line.isEmpty() ? word : line + " " + word;
        if (fm.horizontalAdvance(trial) > hr.width() && !line.isEmpty()) {
            Text(p, f.mono12, inkFaint, hr.left(), ly, line);
            ly += 16;
            line = word;
        }
        else
            line = trial;
    }
    Text(p, f.mono12, inkFaint, hr.left(), ly, line);
}

void Workbench::paintFooter(QPainter& p, const Layout& L) {
    const Fonts& f = F();
    Fill(p, QRectF(L.ix, L.footTop - 0, L.iw, 2), rule);
    const double cy = L.footTop + 2 + 4 + 8;
    double x = L.ix + 12;
    auto item = [&](const QString& label, const QString& value) {
        x += Text(p, f.mono12, inkSoft, x, cy, label + " ");
        x += Text(p, f.mono12b, ink, x, cy, value);
        x += 24;
    };
    item("PC", Hex(regs_[9]));
    item("SP", Hex(regs_[8]));
    item("Program", program_);
}

void Workbench::beginEdit(EditKind kind, int index, const QRectF& rect, const QString& text) {
    editKind_ = kind;
    editIndex_ = index;
    editor_->setStyleSheet(QString("QLineEdit{background:%1;color:#f4f5f6;border:1px solid #f4f5f6;padding:0 2px;selection-background-color:#4477ff;selection-color:#ffffff;}")
                               .arg(kind == EditRegister ? "#181b1f" : "#2d3239"));
    editor_->setAlignment(kind == EditMemory ? Qt::AlignCenter : Qt::AlignLeft);
    editor_->setMaxLength(kind == EditMemory ? 4 : 8);
    editor_->setGeometry(Snap(rect).toRect());
    editor_->setText(text);
    editor_->show();
    editor_->setFocus();
    editor_->selectAll();
}

void Workbench::commitEdit() {
    if (editKind_ == EditNone)
        return;
    const EditKind kind = editKind_;
    const int index = editIndex_;
    QString text = editor_->text().trimmed();
    editKind_ = EditNone;
    editor_->hide();
    setFocus();

    bool ok = false;
    uint value = 0;
    if (kind == EditRegister && index == 10) {
        if (text.startsWith("0b", Qt::CaseInsensitive))
            text = text.mid(2);
        value = text.toUInt(&ok, 2);
    }
    else {
        if (text.startsWith("0x", Qt::CaseInsensitive))
            text = text.mid(2);
        value = text.toUInt(&ok, 16);
    }
    if (ok) {
        value &= 0xFFFF;
        auto guard = core_.lock();
        if (kind == EditRegister) {
            static const RegisterName order[REG_ROWS] = {R0, R1, R2, R3, R4, R5, R6, R7, SP, PC, FLAGS, RAM_ADDRESS, IR0, IR1};
            core_.setReg(order[index], static_cast<uint16_t>(value));
        }
        else {
            core_.poke(static_cast<uint16_t>(index), static_cast<uint16_t>(value));
            if (core_.tickCount == 0)
                core_.init();
        }
    }
    refresh();
    lastRegs_ = regs_;
}

void Workbench::cancelEdit() {
    editKind_ = EditNone;
    editor_->hide();
    setFocus();
}

void Workbench::showSamplesMenu(const QPoint& globalPos) {
    QMenu menu(this);
    if (samples_.isEmpty()) {
        QAction* none = menu.addAction("No sample programs found");
        none->setEnabled(false);
    }
    for (const QString& path : samples_)
        menu.addAction(QFileInfo(path).dir().dirName())->setData(path);
    if (QAction* chosen = menu.exec(globalPos))
        loadProgram(chosen->data().toString());
}

void Workbench::showUnitMenu(const QPoint& globalPos) {
    QMenu menu(this);
    QAction* hzA = menu.addAction("Hz");
    QAction* khz = menu.addAction("kHz");
    QAction* mhz = menu.addAction("MHz");
    QAction* chosen = menu.exec(globalPos);
    if (chosen == hzA) applyUnit(0);
    else if (chosen == khz) applyUnit(1);
    else if (chosen == mhz) applyUnit(2);
}

void Workbench::setSliderFromX(double x) {
    const Layout L = layout();
    const double tx0 = L.range.left() + 18, tx1 = L.range.right() - 1 - 18;
    const double frac = std::clamp((x - tx0) / (tx1 - tx0), 0.0, 1.0);
    const double value = std::round(frac * std::log10(HZ_MAX) * 100) / 100;
    setHz(std::pow(10.0, value));
}

void Workbench::mousePressEvent(QMouseEvent* e) {
    setFocus();
    if (editKind_ != EditNone)
        commitEdit();
    const Layout L = layout();
    const QPointF pt = e->position();

    if (e->button() == Qt::RightButton) {
        const int dip = hitDip(L, pt);
        if (dip >= 0) {
            binding_ = {dip / 16, dip % 16};
            update();
            return;
        }
        emit contextMenuRequested(e->globalPosition().toPoint());
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;

    if (binding_.first >= 0) {
        binding_ = {-1, -1};
        update();
    }

    if (L.load.contains(pt)) { emit loadRequested(); return; }
    if (L.samples.contains(pt)) { showSamplesMenu(mapToGlobal(QPoint(static_cast<int>(L.samples.left()), static_cast<int>(L.samples.bottom())))); return; }
    if (L.run.contains(pt)) { togglePlay(); return; }
    if (L.stepBtn.contains(pt)) { step(e->modifiers() & Qt::ShiftModifier); return; }
    if (L.reset.contains(pt)) { resetCore(); return; }
    if (L.unit.contains(pt)) { showUnitMenu(mapToGlobal(QPoint(static_cast<int>(L.unit.left()), static_cast<int>(L.unit.bottom())))); return; }
    if (L.range.contains(pt)) { draggingSlider_ = true; setSliderFromX(pt.x()); return; }
    const double rateValues[4] = {1e3, 1e5, 1e6, 1e7};
    for (int i = 0; i < 4; ++i)
        if (L.rates[i].contains(pt)) { setHz(rateValues[i]); return; }
    if (L.pgUp.contains(pt)) { base_ = static_cast<uint16_t>(base_ - 80); update(); return; }
    if (L.pgDn.contains(pt)) { base_ = static_cast<uint16_t>(base_ + 80); update(); return; }
    if (L.follow.contains(pt)) { follow_ = !follow_; update(); return; }

    const int cell = hitMemoryCell(L, pt);
    if (cell >= 0) { sel_ = cell; update(); return; }

    const int dip = hitDip(L, pt);
    if (dip >= 0) {
        setPortBit(dip / 16, dip % 16, !((io_[dip / 16] >> (dip % 16)) & 1));
        update();
        return;
    }

    if (pt.x() >= L.rightX && pt.y() >= L.listTop && pt.y() < L.listTop + L.listRows * 24) {
        const int row = static_cast<int>((pt.y() - L.listTop) / 24);
        uint16_t a = listBase_;
        for (int i = 0; i < row; ++i)
            a = static_cast<uint16_t>(a + Disassemble(ram_.data(), a).words);
        if (pt.x() < L.rightX + 22) {
            setBreakpoint(a, !breakpoints_[a]);
        }
        else {
            sel_ = a;
            base_ = a & 0xFFF8;
        }
        update();
    }
}

void Workbench::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton)
        return;
    const Layout L = layout();
    const QPointF pt = e->position();

    const int row = hitRegisterRow(L, pt);
    if (row >= 0) {
        if (row == 11 || row == 12 || row == 13)
            return;
        const QRectF cell(L.ix + 12 + 58.8, 138 + row * 24, 67.2, 24);
        const QString text = row == 10 ? QString::number(regs_[row] & 0xF, 2) : Hex(regs_[row]);
        beginEdit(EditRegister, row, cell.adjusted(1, 2, -1, -2), text);
        return;
    }
    const int mem = hitMemoryCell(L, pt);
    if (mem >= 0) {
        const int offset = static_cast<uint16_t>(mem - base_);
        const int r = offset / MEM_COLS, c = offset % MEM_COLS;
        const QRectF cell(L.midX + L.memAddrW + c * L.memColW, L.memTop + 22.5 + r * 23, L.memColW, 23);
        beginEdit(EditMemory, mem, cell.adjusted(1, 1, -1, -1), Hex(ram_[mem]).mid(2));
        sel_ = mem;
    }
}

void Workbench::mouseMoveEvent(QMouseEvent* e) {
    const Layout L = layout();
    const QPointF pt = e->position();
    if (draggingSlider_) {
        setSliderFromX(pt.x());
        return;
    }
    int h = -1;
    if (L.load.contains(pt)) h = 1;
    else if (L.samples.contains(pt)) h = 2;
    else if (L.run.contains(pt)) h = 3;
    else if (L.stepBtn.contains(pt)) h = 4;
    else if (L.reset.contains(pt)) h = 5;
    else if (L.pgUp.contains(pt)) h = 20;
    else if (L.pgDn.contains(pt)) h = 21;
    for (int i = 0; i < 4 && h < 0; ++i)
        if (L.rates[i].contains(pt)) h = 10 + i;
    if (h < 0) {
        const int cell = hitMemoryCell(L, pt);
        if (cell >= 0) {
            const int offset = static_cast<uint16_t>(cell - base_);
            h = 1000 + offset;
        }
    }
    if (h < 0) {
        const int dip = hitDip(L, pt);
        if (dip >= 0)
            h = 2000 + dip;
    }
    if (h < 0 && pt.x() >= L.rightX && pt.y() >= L.listTop && pt.y() < L.listTop + L.listRows * 24)
        h = 3000 + static_cast<int>((pt.y() - L.listTop) / 24);
    if (h != hover_) {
        hover_ = h;
        const bool pointer = h > 0;
        setCursor(pointer ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void Workbench::mouseReleaseEvent(QMouseEvent*) {
    draggingSlider_ = false;
}

void Workbench::leaveEvent(QEvent*) {
    hover_ = -1;
    update();
}

void Workbench::wheelEvent(QWheelEvent* e) {
    const Layout L = layout();
    if (e->position().y() > L.memTop && e->position().x() > L.midX && e->position().x() < L.rightX) {
        base_ = static_cast<uint16_t>(base_ + (e->angleDelta().y() > 0 ? -MEM_COLS : MEM_COLS));
        update();
    }
}

void Workbench::keyPressEvent(QKeyEvent* e) {
    if (binding_.first >= 0) {
        for (auto it = binds_.begin(); it != binds_.end();) {
            if (it.value() == binding_)
                it = binds_.erase(it);
            else
                ++it;
        }
        if (e->key() != Qt::Key_Escape)
            binds_[e->key()] = binding_;
        binding_ = {-1, -1};
        saveBindings();
        update();
        return;
    }
    if (editKind_ != EditNone && e->key() == Qt::Key_Escape) {
        cancelEdit();
        return;
    }
    auto it = binds_.find(e->key());
    if (it != binds_.end()) {
        if (!e->isAutoRepeat() && !(keyHeld_[it->first] & (1u << it->second))) {
            keyHeld_[it->first] |= 1u << it->second;
            setPortBit(it->first, it->second, true);
        }
        return;
    }
    if (e->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        QWidget::keyPressEvent(e);
        return;
    }
    QWidget::keyPressEvent(e);
}

void Workbench::keyReleaseEvent(QKeyEvent* e) {
    auto it = binds_.find(e->key());
    if (it == binds_.end() || e->isAutoRepeat()) {
        QWidget::keyReleaseEvent(e);
        return;
    }
    if (keyHeld_[it->first] & (1u << it->second)) {
        keyHeld_[it->first] &= ~(1u << it->second);
        setPortBit(it->first, it->second, false);
    }
}

bool Workbench::eventFilter(QObject* watched, QEvent* event) {
    if (watched == editor_ && editKind_ != EditNone) {
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            cancelEdit();
            return true;
        }
        if (event->type() == QEvent::FocusOut) {
            commitEdit();
            return false;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void Workbench::focusOutEvent(QFocusEvent*) {
    for (int port = 0; port < 3; ++port)
        for (int bit = 0; bit < 16; ++bit)
            if (keyHeld_[port] & (1u << bit)) {
                keyHeld_[port] &= ~(1u << bit);
                setPortBit(port, bit, false);
            }
}

void Workbench::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) {
        dropping_ = true;
        e->acceptProposedAction();
        update();
    }
}

void Workbench::dragLeaveEvent(QDragLeaveEvent*) {
    dropping_ = false;
    update();
}

void Workbench::dropEvent(QDropEvent* e) {
    dropping_ = false;
    const auto urls = e->mimeData()->urls();
    if (!urls.isEmpty())
        loadProgram(urls.first().toLocalFile());
    update();
}
