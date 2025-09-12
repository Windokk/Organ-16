/*
This is "main.cpp", the main file for the Organ16 emulator.
This whole project is work in progress and needs optimization and fixing.
Expect garbage code, bugs and crashes 😅.
*/

#include "qt_includes.hpp"

#include <bitset>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "layouts/screen/canvas.hpp"
#include "layouts/regs/clck_btn.hpp"
#include "layouts/regs/flow_layout.hpp"
#include "layouts/ram_panel.hpp"

#include "backend/cpu.hpp"

#include "splitter.hpp"

QMainWindow* window;
std::unordered_map<RegisterName, QLineEdit*> registersLineEdits;
std::unordered_map<std::string, QWidget*> debugValuesLEDs;
QLabel* clockLabel;
CanvasWidget* canvas;
RamPanel* ramPanel;
QTableView* ramView;
FullSplitter* HSplitterBottom;
QAction *toggleAutomatic;
QAction *toggleManual;
QTimer timer;
QFuture<void> runLoop;
std::atomic<bool> running = false;

bool debugPanelShown = false;

int savedClockFrequency;
bool automaticClock = false;

void ToggleManualClock(bool checked){
    automaticClock = false;
    Clock::GetInstance()->SetFrequency(0);
    timer.stop();
}

void ToggleAutomaticClock(bool checked){
    automaticClock = true;
    timer.stop();
    QObject::disconnect(&timer, nullptr, nullptr, nullptr);

    Clock::GetInstance()->SetFrequency(savedClockFrequency);

    if (Clock::GetInstance()->GetFrequency() > 0) {
        QObject::connect(&timer, &QTimer::timeout, []() {
            CPU::GetInstance()->RunFrame();
        });
        timer.start(1000 / Clock::GetInstance()->GetFrequency());
    }
}

void UpdateDebugValues(std::unordered_map<std::string, bool> debugValues){
    for(auto& pair : debugValues){
        if(debugValuesLEDs.find(pair.first) != debugValuesLEDs.end()){
            debugValuesLEDs.at(pair.first)->setStyleSheet(pair.second ? "background-color: rgba(40, 197, 26, 1);" : "background-color: rgba(143, 143, 143, 1);");
        }
    }
}

void Import_RAM() {
    CPU::GetInstance()->Reset();

    QString fileName = QFileDialog::getOpenFileName(
        window,
        "Open Program File",
        "",
        "Program Files (*.bin)"
    );

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            QString fileContent = QString::fromUtf8(file.readAll());
            file.close();
            QStringList wordList = fileContent.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            std::vector<uint16_t> wordValues;
            for (const QString& wordStr : wordList) {
                bool ok;
                uint16_t value = wordStr.toUShort(&ok, 16);  // Base 16 (hex)
                if (ok) {
                    wordValues.push_back(value);
                } 
                else {
                    QMessageBox::warning(window, "File Error", "Invalid word in file : " + wordStr);
                }
            }
            if(wordValues.size() == ADDRESS_SPACE){
                RAM::GetInstance()->Load(wordValues);
            }
            else{
                QMessageBox::warning(window, "File Error", "Invalid file size : " + QString::number(wordValues.size()));
            }
        }
        else {
            QMessageBox::warning(window, "File Error", "Could not open the selected file.");
        }
    }
    
    CPU::GetInstance()->Init();
}

QWidget* MakeDebugWidget(const std::string& tempValueName) {
    QWidget *widget = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(3, 3, 3, 3);

    // Label
    QLabel *label = new QLabel(QString::fromStdString(tempValueName));
    label->setAlignment(Qt::AlignCenter);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    // LED
    QWidget *led = new QWidget;
    led->setFixedSize(40, 40);
    led->setStyleSheet("background-color: rgba(143, 143, 143, 1);");
    led->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Wrapper to center LED horizontally
    QWidget *ledWrapper = new QWidget;
    QHBoxLayout *ledLayout = new QHBoxLayout(ledWrapper);
    ledLayout->setContentsMargins(0, 0, 0, 0);
    ledLayout->addStretch();
    ledLayout->addWidget(led);
    ledLayout->addStretch();

    // Add to main layout
    layout->addWidget(label);
    layout->addWidget(ledWrapper);

    widget->setLayout(layout);
    widget->setMaximumHeight(70);

    debugValuesLEDs.emplace(tempValueName, led);

    return widget;
}

void ShowDebug(){
    if(debugPanelShown)
        return;

    debugPanelShown = true;

    QWidget* debugPanel = new QWidget;
    debugPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* debugLayout = new QVBoxLayout(debugPanel);

    // ---------- Temporary Values Flip flops ----------
    QGridLayout* gridLayout = new QGridLayout;
    gridLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    gridLayout->addWidget(MakeDebugWidget("IsCurrExt"), 0, 0);
    gridLayout->addWidget(MakeDebugWidget("IsCurrAddrBase"), 0, 1);
    gridLayout->addWidget(MakeDebugWidget("IsCurrAddr"), 0, 2);
    gridLayout->addWidget(MakeDebugWidget("IsCurrAddrJsr"), 0, 3);
    gridLayout->addWidget(MakeDebugWidget("IsCurrSpChange"), 1, 0);
    gridLayout->addWidget(MakeDebugWidget("IsCurrJsr"), 1, 1);
    gridLayout->addWidget(MakeDebugWidget("IsCurrRts"), 1, 2);
    gridLayout->addWidget(MakeDebugWidget("RegIsCurrAddr"), 1, 3);
    
    debugLayout->addLayout(gridLayout);
    HSplitterBottom->addWidget(debugPanel);
    debugPanel->show(); 
}

void UpdateVisualRAMCurrentAddress(uint16_t oldAddress, uint16_t newAddress){
    int oldRow = oldAddress / 16;
    int oldColumn = oldAddress % 16;
    ramPanel->setCellBackground(oldRow, oldColumn, QColor("#5e5e5e"));
    int row = newAddress / 16;
    int column = newAddress % 16;
    ramPanel->setCellBackground(row, column, Qt::darkGreen);
}

void ResetVisualRAM()
{
    ramPanel->updateRAM();
}

std::pair<int, int> GetScreenDim()
{
    return std::pair<int, int>(128, 128);
}

void SetScreenPixel(int x, int y, QColor color)
{
    canvas->setPixel(x, y, color);
}

void OnClockClick() {
    QtConcurrent::run([]() {
        CPU::GetInstance()->RunFrame();
    });
}

void OnResetClick(){
    CPU::GetInstance()->Reset();
    canvas->clear();
}

void UpdateRegValue(RegisterName name, uint16_t value)
{
    if(name == FLAGS){
        std::bitset<4> bits(value & 0xF); 
        QString binaryString = "0b" + QString::fromStdString(bits.to_string());
        registersLineEdits[FLAGS]->setText(binaryString);
    }
    else{
        std::stringstream ss;
        ss << "0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << value;
        QString hexString = QString::fromStdString(ss.str());

        registersLineEdits.at(name)->setText(hexString);
    }
}

void UpdateClockLabel(bool newValue) {
    QMetaObject::invokeMethod(clockLabel, [newValue]() {
        clockLabel->setText(newValue ? "HIGH" : "LOW");

        QString color = newValue ? "9, 202, 25, 255" : "167, 167, 167, 255";
        QString baseStyle = R"(
            QLabel {
                border: none;
                background: transparent;
                font-weight: 600;
                font-size: 14px;
                padding: 2px 4px;
                color: rgba(%1);
            }
        )";
        clockLabel->setStyleSheet(baseStyle.arg(color));
    });
}

QWidget* MakeRegisterWidget(const QString& regName) {
    QWidget* w = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignCenter);

    QLineEdit* lineEdit = new QLineEdit("0x0000");
    lineEdit->setAlignment(Qt::AlignCenter);
    lineEdit->setFixedHeight(20);
    QFile lineEditQss(":/style/stylesheets/lineedit.qss");
    lineEditQss.open(QFile::ReadOnly);
    QString lineEditStyle = QString::fromUtf8(lineEditQss.readAll());
    lineEdit->setStyleSheet(lineEditStyle);
    layout->addWidget(lineEdit);

    registersLineEdits.emplace(RegisterFromString(regName.toStdString()), lineEdit);

    QLabel* img = new QLabel;
    img->setPixmap(":/images/imgs/"+regName+".png");
    img->setScaledContents(true);
    img->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    img->setMinimumSize(40, 40);
    img->setMaximumSize(80, 80);

    layout->addWidget(img, 0, Qt::AlignHCenter);

    w->setMinimumSize(60, 60);

    if(regName == "FLAGS"){
        lineEdit->setText("0b0000");
    }

    return w;
}

QWidget* MakeResetWidget(){
    QWidget* w = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(14, 4, 14, 4);
    layout->setSpacing(4);
    layout->setAlignment(Qt::AlignCenter);

    QLabel* label = new QLabel("RESET ?");
    label->setAlignment(Qt::AlignCenter);
    label->setFixedWidth(113);
    label->setFixedHeight(20);
    label->setStyleSheet(QString(
        "QLabel {"
        "   border: none;"
        "   background: transparent;"
        "   color: #a7a7a7;"
        "   font-weight: 600;"
        "   font-size: 14px;"
        "   padding: 2px 4px;"
        "}"
    ));
    layout->addWidget(label);

    ClockButton* btn = new ClockButton(":/images/imgs/RESET.png", OnResetClick);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    btn->setMinimumSize(40, 40);
    btn->setMaximumSize(80, 80);

    layout->addWidget(btn, 0, Qt::AlignHCenter);

    w->setMinimumSize(60, 60);
    return w;
}

QWidget* MakeClockWidget() {
    QWidget* w = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(14, 4, 14, 4);
    layout->setSpacing(4);
    layout->setAlignment(Qt::AlignCenter);

    QLabel* label = new QLabel("LOW");
    label->setAlignment(Qt::AlignCenter);
    label->setFixedWidth(113);
    label->setFixedHeight(20);
    label->setStyleSheet(QString(
        "QLabel {"
        "   border: none;"
        "   background: transparent;"
        "   color: #a7a7a7;"
        "   font-weight: 600;"
        "   font-size: 14px;"
        "   padding: 2px 4px;"
        "}"
    ));
    layout->addWidget(label);

    clockLabel = label;

    ClockButton* btn = new ClockButton(":/images/imgs/CLOCK.png", OnClockClick);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    btn->setMinimumSize(40, 40);
    btn->setMaximumSize(80, 80);

    layout->addWidget(btn, 0, Qt::AlignHCenter);

    w->setMinimumSize(60, 60);
    return w;
}

void SetupGUI(){
    window = new QMainWindow;

    window->resize(800, 600);
    
    /* ============ Menu ============== */
    QMenuBar *menubar = new QMenuBar(window);
    menubar->setObjectName("menubar");

    QMenu *file_menu = new QMenu("File", menubar);
    QAction* importAction = new QAction();
    importAction->setText("Import program...");
    importAction->setToolTip("Load a compiled program file to RAM");
    QObject::connect(importAction, &QAction::triggered, &Import_RAM);
    file_menu->addAction(importAction);

    QMenu *debug_menu = new QMenu("Debug", menubar);
    QAction* showSignals = new QAction();
    showSignals->setText("Show debug panel");
    showSignals->setToolTip("Show all intermediate/temporary values");
    QObject::connect(showSignals, &QAction::triggered, &ShowDebug);
    debug_menu->addAction(showSignals);
    
    QMenu *simulation_menu = new QMenu("Simulation", menubar);
    QMenu* modClockFreq = new QMenu("Clock frequency...", simulation_menu);
    modClockFreq->setToolTip("Modify automatic clock frequency");

    QWidgetAction* clockFrequencyAction = new QWidgetAction(simulation_menu);

    QWidget* mainFrequencyWidget = new QWidget();
    QVBoxLayout* frequencyVLayout = new QVBoxLayout(mainFrequencyWidget);

    QLabel* frequencyTitle = new QLabel(
        QString("Frequency: %1 MHz").arg(savedClockFrequency)
    );
    frequencyTitle->setAlignment(Qt::AlignHCenter);

    QSlider* slider = new QSlider(Qt::Horizontal);
    slider->setMinimumSize(200, 50);
    slider->setRange(0, 100);
    slider->setValue(Clock::GetInstance()->GetFrequency());
    QObject::connect(slider, &QSlider::valueChanged, [frequencyTitle](int value){
        savedClockFrequency = value;
        if(automaticClock)
            ToggleAutomaticClock(true);

        if(value = 0)
            ToggleManualClock(true);
        frequencyTitle->setText(QString("Frequency: %1 MHz").arg(savedClockFrequency));
    });
    QFile sliderQss(":/style/stylesheets/slider.qss");
    sliderQss.open(QFile::ReadOnly);
    QString sliderStyle = QString::fromUtf8(sliderQss.readAll());
    slider->setStyleSheet(sliderStyle);

    frequencyVLayout->addWidget(frequencyTitle);
    frequencyVLayout->addWidget(slider);

    clockFrequencyAction->setDefaultWidget(mainFrequencyWidget);
    modClockFreq->addAction(clockFrequencyAction);

    QMenu* modClockType = new QMenu("Clock type...", simulation_menu);
    modClockType->setToolTip("Modify clock interaction type");
    toggleAutomatic = new QAction("Automatic", modClockType);
    toggleAutomatic->setCheckable(true);
    toggleAutomatic->setChecked(false);
    QObject::connect(toggleAutomatic, &QAction::toggled, [=](bool checked){
        if (checked) {
            toggleManual->setChecked(false);
            ToggleAutomaticClock(checked);
        }
        else{
            toggleManual->setChecked(true);
            ToggleManualClock(true);
        }
    });
    modClockType->addAction(toggleAutomatic);

    toggleManual = new QAction("Manual", modClockType);
    toggleManual->setCheckable(true);
    toggleManual->setChecked(false);
    QObject::connect(toggleManual, &QAction::toggled, [=](bool checked){
        if (checked) {
            toggleAutomatic->setChecked(false);
            ToggleManualClock(checked);
        }
        else{
            toggleAutomatic->setChecked(true);
            ToggleAutomaticClock(true);
        }
    });
    modClockType->addAction(toggleManual);

    simulation_menu->addMenu(modClockType);
    simulation_menu->addMenu(modClockFreq);

    menubar->addMenu(file_menu);
    menubar->addMenu(debug_menu);
    menubar->addMenu(simulation_menu);
    window->setMenuBar(menubar);

    QFile menuQss(":/style/stylesheets/menu.qss");
    menuQss.open(QFile::ReadOnly);
    QString menuStyle = QString::fromUtf8(menuQss.readAll());
    qApp->setStyleSheet(menuStyle);

    /* ============ Splitters ============== */
    FullSplitter *VSplitter = new FullSplitter(Qt::Vertical);
    VSplitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    FullSplitter *HSplitter = new FullSplitter(Qt::Horizontal);
    HSplitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    HSplitterBottom = new FullSplitter(Qt::Horizontal);
    HSplitterBottom->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    /* ============ Right panel ============== */
    QWidget* rightPanel = new QWidget;
    rightPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    
    canvas = new CanvasWidget;
    canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rightLayout->addWidget(canvas);

    
    /* ============ Left panel ============== */
    QWidget* leftPanel = new QWidget;
    leftPanel->setMinimumWidth(200);
    leftPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);

    ramPanel = new RamPanel();
    ramView = new QTableView;
    ramView->setModel(ramPanel);
    ramView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    ramView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ramView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ramView->horizontalHeader()->setDefaultSectionSize(50);
    ramView->verticalHeader()->setDefaultSectionSize(20);
    ramView->setFont(QFont("Courier", 10));
    ramView->setWindowTitle("RAM Viewer");
    QFile ramViewQss(":/style/stylesheets/ram_panel.qss");
    ramViewQss.open(QFile::ReadOnly);
    QString ramViewStyle = QString::fromUtf8(ramViewQss.readAll());
    ramView->setStyleSheet(ramViewStyle);
    leftLayout->addWidget(ramView);
    ramView->setMaximumWidth(870);

    
    /* ============ Bottom panel ============== */
    QWidget* bottomPanel = new QWidget;
    bottomPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* bottomLayout = new QVBoxLayout(bottomPanel);

    QScrollArea* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    QFile scrollAreaQss(":/style/stylesheets/scrollarea.qss");
    scrollAreaQss.open(QFile::ReadOnly);
    QString scrollAreaStyle = QString::fromUtf8(scrollAreaQss.readAll());
    scrollArea->setStyleSheet(scrollAreaStyle);

    QWidget* scrollContent = new QWidget;
    QVBoxLayout* mainLayout = new QVBoxLayout(scrollContent);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    // ---------- General Purpose Registers ----------
    QWidget* gpRegsGroup = new QWidget;
    FlowLayout* gpRegsLayout = new FlowLayout(gpRegsGroup);
    gpRegsLayout->addWidget(MakeRegisterWidget("R0"));
    gpRegsLayout->addWidget(MakeRegisterWidget("R1"));
    gpRegsLayout->addWidget(MakeRegisterWidget("R2"));
    gpRegsLayout->addWidget(MakeRegisterWidget("R3"));
    gpRegsLayout->addWidget(MakeRegisterWidget("R4"));
    gpRegsLayout->addWidget(MakeRegisterWidget("R5"));
    gpRegsLayout->addWidget(MakeRegisterWidget("R6"));
    gpRegsLayout->addWidget(MakeRegisterWidget("R7"));
    mainLayout->addWidget(gpRegsGroup);

    // ---------- Special Registers ----------
    QWidget* specRegsGroup = new QWidget;
    FlowLayout* specRegsLayout = new FlowLayout(specRegsGroup);
    specRegsLayout->addWidget(MakeRegisterWidget("SP"));
    specRegsLayout->addWidget(MakeRegisterWidget("PC"));
    specRegsLayout->addWidget(MakeRegisterWidget("FLAGS"));
    specRegsLayout->addWidget(MakeRegisterWidget("RAM_ADDRESS"));
    mainLayout->addWidget(specRegsGroup);

    // ---------- IR Registers ----------
    QWidget* irRegsGroup = new QWidget;
    FlowLayout* irRegsLayout = new FlowLayout(irRegsGroup);
    irRegsLayout->addWidget(MakeRegisterWidget("IR0"));
    irRegsLayout->addWidget(MakeRegisterWidget("IR1"));
    mainLayout->addWidget(irRegsGroup);

    // ---------- Clock Group ----------
    QWidget* clockGroup = new QWidget;
    FlowLayout* clockLayout = new FlowLayout(clockGroup);
    clockLayout->addWidget(MakeClockWidget());
    clockLayout->addWidget(MakeResetWidget());
    mainLayout->addWidget(clockGroup);

    // ---------- Finalize scroll area ----------
    scrollContent->setLayout(mainLayout);
    scrollArea->setWidget(scrollContent);

    bottomLayout->addWidget(scrollArea);

    HSplitter->addWidget(leftPanel);
    HSplitter->addWidget(rightPanel);
    HSplitter->setStretchFactor(0, 0);
    HSplitter->setStretchFactor(1, 1);
    HSplitter->setChildrenCollapsible(false);

    HSplitterBottom->addWidget(bottomPanel);

    VSplitter->setChildrenCollapsible(false);

    VSplitter->addWidget(HSplitter);
    VSplitter->addWidget(HSplitterBottom);

    window->setCentralWidget(VSplitter);
    window->setWindowTitle("Organ 16 Emulator");
    window->show();
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    SetupGUI();

    CPU* cpu = CPU::GetInstance();

    cpu->Init();

    return app.exec();
}