#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "ui/workbench.hpp"
#include "ui/layout.hpp"
#include "ui/paint.hpp"
#include "ui/disasm.hpp"

using namespace ui;
using namespace theme;

void Workbench::beginEdit(EditKind kind, int index, const QRectF& rect, const QString& text) {
    editKind_ = kind;
    editIndex_ = index;
    editor_->setStyleSheet(QString("QLineEdit{background:%1;color:#f2f2f2;border:1px solid #f2f2f2;padding:0 2px;selection-background-color:#4a7fbb;selection-color:#ffffff;}")
                               .arg(kind == EditRegister ? "#181818" : "#2c2c2c"));
    editor_->setAlignment(kind == EditMemory ? Qt::AlignCenter : Qt::AlignLeft);
    editor_->setMaxLength(kind == EditMemory ? 4 : 8);
    editor_->setGeometry(snap(rect).toRect());
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
    if (kind == EditRegister && index == REG_FLAGS) {
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
        QAction* none = menu.addAction("No samples");
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
    const double tx0 = L.range.left() + SLIDER_INSET, tx1 = L.range.right() - 1 - SLIDER_INSET;
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
            binding_ = {dip / PORT_BITS, dip % PORT_BITS};
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
        for (int i = 0; i < 4; ++i)
        if (L.rates[i].contains(pt)) { setHz(RATE_VALUES[i]); return; }
    if (L.pgUp.contains(pt)) { base_ = static_cast<uint16_t>(base_ - MEM_PAGE); update(); return; }
    if (L.pgDn.contains(pt)) { base_ = static_cast<uint16_t>(base_ + MEM_PAGE); update(); return; }
    if (L.follow.contains(pt)) { follow_ = !follow_; update(); return; }

    const int cell = hitMemoryCell(L, pt);
    if (cell >= 0) { sel_ = cell; update(); return; }

    const int dip = hitDip(L, pt);
    if (dip >= 0) {
        setPortBit(dip / PORT_BITS, dip % PORT_BITS, !((io_[dip / PORT_BITS] >> (dip % PORT_BITS)) & 1));
        update();
        return;
    }

    if (pt.x() >= L.rightX && pt.y() >= L.listTop && pt.y() < L.listTop + L.listRows * LIST_ROW_H) {
        const int row = static_cast<int>((pt.y() - L.listTop) / LIST_ROW_H);
        uint16_t a = listBase_;
        for (int i = 0; i < row; ++i)
            a = static_cast<uint16_t>(a + disassemble(ram_.data(), a).words);
        if (pt.x() < L.rightX + LIST_GUTTER) {
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
        if (row == REG_ADDR || row == REG_IR0 || row == REG_IR1)
            return;
        const QRectF cell(REG_TABLE_X + REG_TAG_W, REG_TABLE_Y + row * REG_ROW_H, REG_HEX_W, REG_ROW_H);
        const QString text = row == REG_FLAGS ? QString::number(regs_[row] & 0xF, 2) : hex(regs_[row]);
        beginEdit(EditRegister, row, cell.adjusted(1, 2, -1, -2), text);
        return;
    }
    const int mem = hitMemoryCell(L, pt);
    if (mem >= 0) {
        const int offset = static_cast<uint16_t>(mem - base_);
        const int r = offset / MEM_COLS, c = offset % MEM_COLS;
        const QRectF cell(L.midX + MEM_ADDR_W + c * L.memColW, L.memTop + MEM_HEAD_H + r * MEM_ROW_H, L.memColW, MEM_ROW_H);
        beginEdit(EditMemory, mem, cell.adjusted(1, 1, -1, -1), hex(ram_[mem]).mid(2));
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
    int h = HOVER_NONE;
    if (L.load.contains(pt)) h = HOVER_LOAD;
    else if (L.samples.contains(pt)) h = HOVER_SAMPLES;
    else if (L.run.contains(pt)) h = HOVER_RUN;
    else if (L.stepBtn.contains(pt)) h = HOVER_STEP;
    else if (L.reset.contains(pt)) h = HOVER_RESET;
    else if (L.pgUp.contains(pt)) h = HOVER_PG_UP;
    else if (L.pgDn.contains(pt)) h = HOVER_PG_DN;
    for (int i = 0; i < 4 && h < 0; ++i)
        if (L.rates[i].contains(pt)) h = HOVER_RATE + i;
    if (h < 0) {
        const int cell = hitMemoryCell(L, pt);
        if (cell >= 0) {
            const int offset = static_cast<uint16_t>(cell - base_);
            h = HOVER_MEM + offset;
        }
    }
    if (h < 0) {
        const int dip = hitDip(L, pt);
        if (dip >= 0)
            h = HOVER_DIP + dip;
    }
    if (h < 0 && pt.x() >= L.rightX && pt.y() >= L.listTop && pt.y() < L.listTop + L.listRows * LIST_ROW_H)
        h = HOVER_LIST + static_cast<int>((pt.y() - L.listTop) / LIST_ROW_H);
    if (h != hover_) {
        hover_ = h;
        const bool pointer = h != HOVER_NONE;
        setCursor(pointer ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void Workbench::mouseReleaseEvent(QMouseEvent*) {
    draggingSlider_ = false;
}

void Workbench::leaveEvent(QEvent*) {
    hover_ = HOVER_NONE;
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
    for (int port = 0; port < PORT_COUNT; ++port)
        for (int bit = 0; bit < PORT_BITS; ++bit)
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
