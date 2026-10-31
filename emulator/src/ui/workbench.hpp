#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QLineEdit>
#include <QMap>
#include <QPixmap>
#include <QStringList>
#include <QWidget>

#include <array>
#include <vector>

#include "core/core.hpp"
#include "core/sim_thread.hpp"

class Workbench : public QWidget {
    Q_OBJECT

public:
    Workbench(Core& core, SimThread& sim, QWidget* parent = nullptr);

    void refresh();
    void setSamples(const QStringList& paths);
    bool loadProgram(const QString& path);
    void toast(const QString& text);

    void togglePlay();
    void step(bool halfTick);
    void resetCore();
    void playAt(double hz);
    void setBreakpointAt(uint16_t address) { setBreakpoint(address, true); }

    QSize sizeHint() const override { return QSize(1600, 900); }
    QSize minimumSizeHint() const override { return QSize(1366, 880); }

signals:
    void loadRequested();
    void contextMenuRequested(const QPoint& globalPos);
    void resetPerformed();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dragLeaveEvent(QDragLeaveEvent*) override;
    void dropEvent(QDropEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;

private:
    struct Layout;

    enum EditKind { EditNone, EditRegister, EditMemory };

    Layout layout() const;
    void paintHeader(QPainter&, const Layout&);
    void paintTransport(QPainter&, const Layout&);
    void paintRegisters(QPainter&, const Layout&);
    void paintScreen(QPainter&, const Layout&);
    void paintMemory(QPainter&, const Layout&);
    void paintPorts(QPainter&, const Layout&);
    void paintListing(QPainter&, const Layout&);
    void paintFooter(QPainter&, const Layout&);

    void placeEditors(const Layout&);
    void beginEdit(EditKind kind, int index, const QRectF& rect, const QString& text);
    void commitEdit();
    void cancelEdit();

    void setHz(double hz, bool fromField = false);
    void readHzField();
    void applyUnit(int unit);
    void setSliderFromX(double x);
    void goToAddress();
    void setBreakpoint(uint16_t address, bool enabled);
    void setPortBit(int port, int bit, bool on);
    uint16_t sweepStart(uint16_t pc, int back) const;
    void loadBindings();
    void saveBindings() const;
    QString keyLabel(int port, int bit) const;
    void showSamplesMenu(const QPoint& globalPos);
    void showUnitMenu(const QPoint& globalPos);
    QRectF dipRect(const Layout&, int port, int col) const;
    int hitDip(const Layout&, const QPointF&) const;
    int hitRegisterRow(const Layout&, const QPointF&) const;
    int hitMemoryCell(const Layout&, const QPointF&) const;

    Core& core_;
    SimThread& sim_;

    std::array<uint16_t, 14> regs_{};
    std::array<uint16_t, 14> lastRegs_{};
    std::array<qint64, 14> flashUntil_{};
    std::vector<uint16_t> ram_;
    std::array<uint64_t, ADDRESS_SPACE / 64> touched_{};
    std::array<uint16_t, 3> io_{};
    QImage screen_;
    int lit_ = 0;
    uint64_t ticks_ = 0;
    bool halted_ = false;
    bool running_ = false;
    bool hit_ = false;
    QString program_ = "none";

    double hz_ = 1e6;
    double realHz_ = 0;
    uint64_t measureTicks_ = 0;
    qint64 measureAt_ = 0;

    uint16_t base_ = 0;
    int sel_ = -1;
    uint16_t listBase_ = 0;
    bool follow_ = true;
    std::vector<uint8_t> breakpoints_;

    int hover_ = -1;
    QPointF mouse_{-1, -1};
    bool draggingSlider_ = false;
    bool dropping_ = false;

    QLineEdit* hzField_;
    QLineEdit* goField_;
    QLineEdit* editor_;
    EditKind editKind_ = EditNone;
    int editIndex_ = -1;

    QMap<int, std::pair<int, int>> binds_;
    std::pair<int, int> binding_{-1, -1};
    std::array<uint16_t, 3> keyHeld_{};

    QStringList samples_;
    QPixmap logo_;

    QString toast_;
    qint64 toastUntil_ = 0;
    QElapsedTimer clock_;
};
