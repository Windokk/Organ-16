#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QScreen>
#include <QScrollArea>
#include <QShortcut>
#include <QTimer>

#include "core/core.hpp"
#include "core/sim_thread.hpp"
#include "export/gif_recorder.hpp"
#include "export/screenshot.hpp"
#include "ui/theme.hpp"
#include "ui/workbench.hpp"

namespace {

Core core;
SimThread sim(core);
Workbench* workbench = nullptr;
QWidget* window = nullptr;
QWidget* debugWindow = nullptr;
std::vector<std::pair<const char*, QWidget*>> debugLeds;
GifRecorder gifRecorder;
QTimer gifTimer;
QAction* recordGifAction = nullptr;

constexpr int GIF_CAPTURE_INTERVAL_MS = 40;

const char* const APP_STYLE = R"(
QMenu { background: #2d3239; color: #f4f5f6; border: 1px solid #6c7480; padding: 0; }
QMenu::item { padding: 6px 18px; }
QMenu::item:selected { background: #4477ff; color: #ffffff; }
QMenu::item:disabled { color: #939aa5; }
QMenu::separator { height: 1px; background: #3c424b; }
QToolTip { background: #2d3239; color: #f4f5f6; border: 1px solid #6c7480; }
QScrollArea { background: #23272d; border: 0; }
QScrollBar:vertical { background: #181b1f; width: 12px; margin: 0; }
QScrollBar::handle:vertical { background: #6c7480; min-height: 24px; }
QScrollBar:horizontal { background: #181b1f; height: 12px; margin: 0; }
QScrollBar::handle:horizontal { background: #6c7480; min-width: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
)";

ScreenPixels CopyScreen()
{
    auto guard = core.lock();
    return core.screen;
}

QString WithSuffix(QString path, const QString& suffix)
{
    if (QFileInfo(path).suffix().isEmpty())
        path += "." + suffix;
    return path;
}

QString DefaultExportName(const QString& suffix)
{
    return "organ16_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "." + suffix;
}

void SaveScreenshot()
{
    const ScreenPixels pixels = CopyScreen();
    QString path = QFileDialog::getSaveFileName(window, "Save screenshot", DefaultExportName("jpg"), "JPEG image (*.jpg *.jpeg)");
    if (path.isEmpty())
        return;
    path = WithSuffix(path, "jpg");
    if (!SaveScreenshotJpeg(pixels, path))
        QMessageBox::warning(window, "File Error", "Could not save the screenshot to " + path);
}

void StopGifRecording()
{
    gifTimer.stop();
    const bool ok = gifRecorder.stop();
    recordGifAction->setText("Start GIF recording...");
    window->setWindowTitle("Organ 16 Emulator");
    if (!ok)
        QMessageBox::warning(window, "File Error", "The GIF file could not be written completely.");
}

void StartGifRecording()
{
    QString path = QFileDialog::getSaveFileName(window, "Record GIF", DefaultExportName("gif"), "GIF image (*.gif)");
    if (path.isEmpty())
        return;
    path = WithSuffix(path, "gif");
    if (!gifRecorder.start(path.toStdString())) {
        QMessageBox::warning(window, "File Error", "Could not create " + path);
        return;
    }
    gifRecorder.addFrame(CopyScreen());
    gifTimer.start(GIF_CAPTURE_INTERVAL_MS);
    recordGifAction->setText("Stop GIF recording");
    window->setWindowTitle("Organ 16 Emulator - recording GIF");
}

void ToggleGifRecording()
{
    if (gifRecorder.recording())
        StopGifRecording();
    else
        StartGifRecording();
}

void ImportProgram()
{
    const QString fileName = QFileDialog::getOpenFileName(window, "Open Program File", "", "Program Files (*.bin)");
    if (!fileName.isEmpty())
        workbench->loadProgram(fileName);
}

QStringList FindSamples()
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        if (dir.exists("programs/asm")) {
            QDir programs(dir.filePath("programs/asm"));
            QStringList paths;
            const QStringList preferred = {"cube", "pong", "fill_screen_red", "io", "stack", "tests", "division_by_0"};
            QStringList names = programs.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const QString& name : preferred)
                if (names.removeOne(name))
                    names.prepend(name);
            QStringList ordered;
            for (const QString& name : preferred)
                if (names.contains(name))
                    ordered << name;
            for (const QString& name : names)
                if (!ordered.contains(name))
                    ordered << name;
            for (const QString& name : ordered) {
                QDir sub(programs.filePath(name));
                const QStringList bins = sub.entryList({"*.bin"}, QDir::Files, QDir::Name);
                if (!bins.isEmpty())
                    paths << sub.filePath(bins.first());
            }
            return paths;
        }
        if (!dir.cdUp())
            break;
    }
    return {};
}

void BuildDebugWindow()
{
    debugWindow = new QWidget;
    debugWindow->setWindowTitle("Organ 16 debug");
    debugWindow->setStyleSheet("QWidget{background:#23272d;color:#f4f5f6;}");
    QGridLayout* grid = new QGridLayout(debugWindow);
    const char* names[8] = {"IsCurrExt", "IsCurrAddrBase", "IsCurrAddr", "IsCurrAddrJsr", "IsCurrSpChange", "IsCurrJsr", "IsCurrRts", "RegIsCurrAddr"};
    for (int i = 0; i < 8; ++i) {
        QLabel* label = new QLabel(names[i]);
        label->setFont(theme::mono(12));
        label->setAlignment(Qt::AlignCenter);
        QWidget* led = new QWidget;
        led->setFixedSize(40, 40);
        led->setStyleSheet("background:#3b414a;");
        grid->addWidget(label, (i / 4) * 2, i % 4);
        grid->addWidget(led, (i / 4) * 2 + 1, i % 4, Qt::AlignHCenter);
        debugLeds.push_back({names[i], led});
    }
}

void RefreshDebug()
{
    if (!debugWindow || !debugWindow->isVisible())
        return;
    std::array<bool, 8> values;
    {
        auto guard = core.lock();
        values = {core.ffExt, core.ffAddrBase, core.ffAddrBase || core.ffAddrJsr, core.ffAddrJsr,
                  core.ffSpChange, core.ffJsr, core.ffRts, core.ffRegAddr};
    }
    for (size_t i = 0; i < debugLeds.size(); ++i)
        debugLeds[i].second->setStyleSheet(values[i] ? "background:#2ee66b;" : "background:#3b414a;");
}

void ShowContextMenu(const QPoint& pos)
{
    QMenu menu;
    QAction* import = menu.addAction("Import program...");
    menu.addSeparator();
    QAction* shot = menu.addAction("Save screenshot (JPEG 800x800)...");
    shot->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    recordGifAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    menu.addAction(recordGifAction);
    menu.addSeparator();
    QAction* debug = menu.addAction("Show debug panel");

    QAction* chosen = menu.exec(pos);
    if (chosen == import)
        ImportProgram();
    else if (chosen == shot)
        SaveScreenshot();
    else if (chosen == debug) {
        if (!debugWindow)
            BuildDebugWindow();
        debugWindow->show();
        debugWindow->raise();
    }
}

}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Emulator");
    app.setOrganizationName("Organ16");
    theme::loadFonts();
    app.setStyleSheet(APP_STYLE);

    QCommandLineParser parser;
    QCommandLineOption programOption("program", "Load a program at startup.", "file");
    QCommandLineOption shotOption("shot", "Save a screenshot of the window to a PNG and quit.", "file");
    QCommandLineOption sizeOption("size", "Window size for --shot.", "WxH", "1600x900");
    QCommandLineOption ticksOption("ticks", "Half ticks to run before --shot.", "n", "0");
    QCommandLineOption playOption("play", "Run the clock at this many Hz for 600 ms before --shot.", "hz");
    QCommandLineOption breakOption("break", "Set a breakpoint at this hex address.", "addr");
    parser.addOptions({programOption, shotOption, sizeOption, ticksOption, playOption, breakOption});
    parser.process(app);

    recordGifAction = new QAction("Start GIF recording...");
    QObject::connect(recordGifAction, &QAction::triggered, &ToggleGifRecording);

    workbench = new Workbench(core, sim);
    workbench->setSamples(FindSamples());

    {
        auto guard = core.lock();
        core.init();
    }

    QObject::connect(workbench, &Workbench::loadRequested, &ImportProgram);
    QObject::connect(workbench, &Workbench::contextMenuRequested, &ShowContextMenu);

    const bool shotMode = parser.isSet(shotOption);

    QScrollArea* scroll = nullptr;
    if (shotMode) {
        window = workbench;
    }
    else {
        scroll = new QScrollArea;
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setWidget(workbench);
        window = scroll;
    }
    window->setWindowTitle("Organ 16 Emulator");

    if (parser.isSet(programOption))
        workbench->loadProgram(parser.value(programOption));

    if (shotMode) {
        const QStringList dims = parser.value(sizeOption).split('x');
        workbench->resize(dims.value(0).toInt(), dims.value(1).toInt());
        const uint64_t ticks = parser.value(ticksOption).toULongLong();
        if (ticks) {
            auto guard = core.lock();
            core.run(ticks);
        }
        workbench->show();
        workbench->refresh();
        app.processEvents();
        if (parser.isSet(breakOption))
            workbench->setBreakpointAt(static_cast<uint16_t>(parser.value(breakOption).toUInt(nullptr, 16)));
        int wait = 150;
        if (parser.isSet(playOption)) {
            workbench->playAt(parser.value(playOption).toDouble());
            sim.start();
            wait = 700;
        }
        QTimer frames;
        QObject::connect(&frames, &QTimer::timeout, [] { workbench->refresh(); });
        frames.start(16);
        QTimer::singleShot(wait, [&] {
            workbench->refresh();
            app.processEvents();
            const bool ok = workbench->grab().save(parser.value(shotOption));
            app.exit(ok ? 0 : 1);
        });
        return app.exec();
    }

    const QSize avail = app.primaryScreen() ? app.primaryScreen()->availableSize() : QSize(1600, 900);
    window->resize(std::min(1600, avail.width()), std::min(900, avail.height()));
    window->show();
    workbench->setFocus();

    sim.start();

    QShortcut shotShortcut(QKeySequence(Qt::CTRL | Qt::Key_P), window);
    QObject::connect(&shotShortcut, &QShortcut::activated, &SaveScreenshot);
    QShortcut gifShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), window);
    QObject::connect(&gifShortcut, &QShortcut::activated, &ToggleGifRecording);

    QTimer frameTimer;
    frameTimer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&frameTimer, &QTimer::timeout, [] {
        workbench->refresh();
        RefreshDebug();
    });
    frameTimer.start(16);

    gifTimer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&gifTimer, &QTimer::timeout, [] {
        if (!gifRecorder.addFrame(CopyScreen()))
            StopGifRecording();
    });
    QObject::connect(&app, &QApplication::aboutToQuit, [] {
        if (gifRecorder.recording())
            StopGifRecording();
    });

    const int result = app.exec();
    sim.stop();
    return result;
}
