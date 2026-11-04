#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>
#include <cmath>

#include "ui/workbench.hpp"
#include "ui/layout.hpp"
#include "ui/paint.hpp"
#include "ui/disasm.hpp"

using namespace ui;
using namespace theme;

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
    hzField_->setFont(fontSet().mono15b);
    hzField_->setStyleSheet("QLineEdit{background:#181818;color:#f2f2f2;border:0;padding:0 8px;selection-background-color:#4a7fbb;selection-color:#ffffff;}"
                            "QLineEdit:focus{background:#2c2c2c;}");
    connect(hzField_, &QLineEdit::textEdited, this, [this] { readHzField(); });
    connect(hzField_, &QLineEdit::editingFinished, this, [this] { setHz(hz_); setFocus(); });

    goField_ = new QLineEdit(this);
    goField_->setPlaceholderText("go 0x");
    goField_->setFont(fontSet().mono11);
    goField_->setStyleSheet("QLineEdit{background:transparent;color:#ffffff;border:1px solid #ffffff;padding:0 6px;selection-background-color:#ffffff;selection-color:#2c4d78;}");
    QPalette pal = goField_->palette();
    QColor placeholder(Qt::white);
    placeholder.setAlphaF(0.7);
    pal.setColor(QPalette::PlaceholderText, placeholder);
    goField_->setPalette(pal);
    connect(goField_, &QLineEdit::returnPressed, this, [this] { goToAddress(); setFocus(); });

    editor_ = new QLineEdit(this);
    editor_->setFont(fontSet().mono13b);
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
    for (int p = 0; p < PORT_COUNT; ++p)
        for (int b = 0; b < PORT_BITS; ++b) {
            // bindings saved before ports were numbered are keyed A_/B_/C_
            const QVariant legacy = settings.value(QString("%1_%2").arg(QChar('A' + p)).arg(b), 0);
            const int key = settings.value(QString("%1_%2").arg(p).arg(b), legacy).toInt();
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
    for (auto it = binds_.begin(); it != binds_.end(); ++it)
        settings.setValue(QString("%1_%2").arg(it.value().first).arg(it.value().second), it.key());
}

QString Workbench::keyLabel(int port, int bit) const {
    if (binding_.first == port && binding_.second == bit)
        return "key?";
    for (auto it = binds_.begin(); it != binds_.end(); ++it)
        if (it.value().first == port && it.value().second == bit)
            return keyName(it.key());
    return QString();
}

void Workbench::setSamples(const QStringList& paths) {
    samples_ = paths;
    update();
}

void Workbench::toast(const QString& text) {
    toast_ = text;
    toastUntil_ = clock_.elapsed() + TOAST_MS;
    update();
}

void Workbench::refresh() {
    {
        auto guard = core_.lock();
        for (int i = 0; i < REG_SP; ++i)
            regs_[i] = core_.r[i];
        regs_[REG_SP] = core_.sp;
        regs_[REG_PC] = core_.pc;
        regs_[REG_FLAGS] = core_.flags;
        regs_[REG_ADDR] = core_.ramAddr;
        regs_[REG_IR0] = core_.ir0;
        regs_[REG_IR1] = core_.ir1;
        io_ = core_.io;
        ticks_ = core_.tickCount;
        halted_ = decodeTable[core_.ir0 >> 9].hlt;
        std::copy(core_.ram.begin(), core_.ram.end(), ram_.begin());
        touched_ = core_.touched;
        if (core_.screenDirty) {
            core_.screenDirty = false;
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
    running_ = sim_.automaticEnabled();
    hit_ = sim_.breakHit();

    const qint64 now = clock_.elapsed();
    for (int i = 0; i < REG_ROWS; ++i) {
        if (regs_[i] != lastRegs_[i] && !(running_ && hz_ > 100))
            flashUntil_[i] = now + FLASH_MS;
        lastRegs_[i] = regs_[i];
    }

    if (!running_) {
        realHz_ = 0;
        measureAt_ = now;
        measureTicks_ = sim_.halfTicksRun();
    }
    else if (now - measureAt_ >= MEASURE_MS) {
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
            // run half ticks until the instruction is done: clock low and no
            // extension word, stack, register-address or jsr cycle pending.
            // the 64 cap is only there so a stuck core can't hang the UI
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
        QMessageBox::warning(this, "File Error", "Cannot open " + path);
        return false;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    // a file made only of hex digits and whitespace is read as a text dump of
    // words, anything else as raw little-endian 16-bit words
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
                QMessageBox::warning(this, "File Error", "Bad word: " + token);
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
        QMessageBox::warning(this, "File Error", "Too many words: " + QString::number(words.size()));
        return false;
    }
    const size_t given = words.size();
    words.resize(ADDRESS_SPACE, 0);  // short files are zero-padded up to the full 64K

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
    toast(program_ + ": " + QString::number(nonZero) + " non-zero" + (given != ADDRESS_SPACE ? " (padded)" : ""));
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

Workbench::Layout Workbench::layout() const {
    Layout L;
    L.W = width();
    L.H = height();
    L.ix = MARGIN;
    L.iw = L.W - 2 * MARGIN;
    L.bottom = L.H - MARGIN;

    const Fonts& f = fontSet();
    double x = L.ix;

    // each header cell is its text plus MARGIN either side and a 1px divider
    const double logoW = LOGO_H * LOGO_ASPECT;
    L.brand = QRectF(x, HEADER_TOP, MARGIN + logoW + 10 + textWidth(f.brand, "Organ 16") + MARGIN + 1, BAR_H);
    L.logo = QRectF(x + MARGIN, HEADER_TOP + 8, logoW, LOGO_H);
    x += L.brand.width();
    L.load = QRectF(x, HEADER_TOP, MARGIN + textWidth(f.body, "Load .bin") + MARGIN + 1, BAR_H);
    x += L.load.width();
    double samplesW = textWidth(f.body, "Samples");
    for (const QString& s : samples_)
        samplesW = std::max(samplesW, textWidth(f.body, QFileInfo(s).dir().dirName()));
    L.samples = QRectF(x, HEADER_TOP, MARGIN + samplesW + MARGIN + 1, BAR_H);

    const char* lampNames[3] = {"RUN", "BRK", "HALT"};
    double lampW = 28 + 32;
    for (int i = 0; i < 3; ++i)
        lampW += 12 + 6 + textWidth(f.silk, lampNames[i]);
    L.lamps = QRectF(L.ix + L.iw - lampW, HEADER_TOP, lampW, BAR_H);
    double lx = L.lamps.left() + MARGIN;
    for (int i = 0; i < 3; ++i) {
        L.lamp[i] = QRectF(lx, HEADER_TOP + MARGIN, 12, 12);
        L.lampText[i] = lx + 18;
        lx += 18 + textWidth(f.silk, lampNames[i]) + 16;
    }

    x = L.ix;
    auto button = [&](const QString& label) {
        const double w = 18 + textWidth(f.button, label) + 18 + 1;
        QRectF r(x, TRANSPORT_TOP, w, BAR_H);
        x += w;
        return r;
    };
    L.run = button(running_ ? "Pause" : "Run");
    L.stepBtn = button("Step");
    L.reset = button("Reset");

    const double hzW = 92, unitW = 62, rangeW = 150;
    L.hz = QRectF(x, TRANSPORT_TOP, hzW, BAR_H);
    x += hzW;
    L.unit = QRectF(x, TRANSPORT_TOP, unitW, BAR_H);
    x += unitW;
    L.range = QRectF(x, TRANSPORT_TOP + 2, rangeW, BAR_H);
    x += rangeW;
    const char* rateLabels[4] = {"1k", "100k", "1M", "10M"};
    for (int i = 0; i < 4; ++i) {
        const double w = 12 + textWidth(f.mono12, rateLabels[i]) + 12 + 1;
        L.rates[i] = QRectF(x, TRANSPORT_TOP + 2, w, BAR_H);
        x += w;
    }
    const QString realText = realHz_ > 0 ? "real " : QString();
    const double realW = std::max(96.0, 12 + textWidth(f.mono12, realText) + (realHz_ > 0 ? 64 : 0) + 12 + 1);
    L.real = QRectF(x, TRANSPORT_TOP, realW, 44);

    const double statW = MARGIN + textWidth(f.mono13, "Executed") + 8 + textWidth(f.mono13b, QLocale().toString(static_cast<qulonglong>(ticks_))) + MARGIN;
    L.stat = QRectF(L.ix + L.iw - statW, TRANSPORT_TOP, statW, 44);

    L.midX = L.ix + LEFT_W;
    L.rightX = L.ix + L.iw - RIGHT_W;
    L.midW = L.rightX - L.midX;

    const double areaBottom = L.bottom - FOOTER_H;
    const double availH = areaBottom - GRID_TOP;
    // the screen takes whatever height the memory table leaves, between 128 and 416px square
    const double capH = 15, bezelBorder = 6;
    const double fixedMid = SECTION_BAR + PAD + bezelBorder + capH + PAD + SECTION_BAR + MEM_HEAD_H + MEM_ROWS * MEM_ROW_H;
    const double bezel = std::max(128.0, std::min({416.0, L.midW - 1 - 2 * PAD, availH - fixedMid}));
    L.bezel = QRectF(L.midX + (L.midW - 1 - bezel) / 2, GRID_TOP + SECTION_BAR + PAD, bezel, bezel);
    L.canvas = L.bezel.adjusted(bezelBorder, bezelBorder, -bezelBorder, -bezelBorder);
    L.cap = QRectF(L.midX + PAD, L.bezel.bottom() + bezelBorder, L.midW - 1 - 2 * PAD, capH);
    L.memSh = L.cap.bottom() + PAD;
    L.memTop = L.memSh + SECTION_BAR;
    L.memColW = (L.midW - 1 - MEM_ADDR_W) / MEM_COLS;

    const double pgW = textWidth(f.mono11, "PG-") + 12 + 2;
    const double right = L.midX + L.midW - 1 - PAD;
    const double goW = 70, toolH = 18;
    L.pgDn = QRectF(right - pgW, L.memSh + 3, pgW, toolH);
    L.pgUp = QRectF(L.pgDn.left() - pgW, L.memSh + 3, pgW, toolH);
    L.goBox = QRectF(L.pgUp.left() - 4 - goW, L.memSh + 3, goW, toolH);

    L.portsSh = REG_TABLE_Y + REG_ROWS * REG_ROW_H + PAD;
    L.portsTop = L.portsSh + SECTION_BAR + PAD;
    L.listSh = GRID_TOP;
    L.listTop = L.listSh + SECTION_BAR;
    L.listRows = std::max(5, static_cast<int>((availH - SECTION_BAR) / LIST_ROW_H));
    const double followW = 20 + 6 + textWidth(f.mono11, "follow PC");
    L.follow = QRectF(L.rightX + RIGHT_W - PAD - followW, L.listSh + 2.5, followW, 19);

    L.footTop = std::max({L.bottom - FOOTER_H, L.portsTop + PORT_COUNT * PORT_STRIDE});
    return L;
}

void Workbench::resizeEvent(QResizeEvent*) {
    placeEditors(layout());
}

void Workbench::placeEditors(const Layout& L) {
    const QRect hz = snap(L.hz).toRect();
    if (hzField_->geometry() != hz)
        hzField_->setGeometry(hz);
    const QRect go = snap(L.goBox).toRect();
    if (goField_->geometry() != go)
        goField_->setGeometry(go);
}

void Workbench::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    const Layout L = layout();
    placeEditors(L);

    p.fillRect(rect(), paper);
    const QRectF frame(PAD, PAD, L.W - 2 * PAD, L.H - 2 * PAD);
    fill(p, frame, panel);
    outline(p, frame, 2, rule);

    paintHeader(p, L);
    paintTransport(p, L);

    fill(p, QRectF(L.ix, GRID_TOP - 2, L.iw, 2), rule);
    vLine(p, L.ix + LEFT_W - 1, GRID_TOP, L.footTop - GRID_TOP, rule);
    vLine(p, L.midX + L.midW - 1, GRID_TOP, L.footTop - GRID_TOP, rule);

    paintRegisters(p, L);
    paintScreen(p, L);
    paintMemory(p, L);
    paintPorts(p, L);
    paintListing(p, L);
    paintFooter(p, L);

    if (!toast_.isEmpty() && clock_.elapsed() < toastUntil_) {
        const double w = textWidth(fontSet().mono13, toast_) + 32;
        const QRectF r((L.W - w) / 2, L.H - 2 * PAD - 34, w, 34);
        fill(p, r, hi);
        putText(p, fontSet().mono13, hiInk, r.center().x(), r.center().y(), toast_, Qt::AlignHCenter);
    }
    if (dropping_) {
        p.setOpacity(0.92);
        p.fillRect(rect(), ink);
        p.setOpacity(1);
        putText(p, fontSet().display, paper, L.W / 2, L.H / 2, "Drop .bin", Qt::AlignHCenter);
    }
}
