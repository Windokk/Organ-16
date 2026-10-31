#pragma once

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QDir>
#include <QCoreApplication>
#include <QString>

namespace theme {

inline const QColor paper("#23272d");
inline const QColor panel("#2d3239");
inline const QColor well("#181b1f");
inline const QColor ink("#f4f5f6");
inline const QColor inkSoft("#c0c5cc");
inline const QColor inkFaint("#939aa5");
inline const QColor rule("#6c7480");
inline const QColor ruleSoft("#3c424b");
inline const QColor focus("#7f9fff");
inline const QColor lampRun("#2ee66b");
inline const QColor lampHalt("#ff3d3d");
inline const QColor lampWait("#ff8a1f");
inline const QColor lampOff("#3b414a");
inline const QColor bitOn("#4477ff");
inline const QColor bar("#24489a");
inline const QColor barInk("#ffffff");
inline const QColor hi("#4477ff");
inline const QColor hiInk("#ffffff");
inline const QColor regGeneral("#75a7ea");
inline const QColor regPointer("#e98a24");
inline const QColor regInstruction("#52ad40");
inline const QColor regInk("#16150f");
inline const QColor black("#000000");

inline QStringList fontDirs() {
    QStringList dirs;
#ifdef ORGAN_FONT_DIR
    dirs << QString(ORGAN_FONT_DIR);
#endif
    const QString app = QCoreApplication::applicationDirPath();
    dirs << app + "/fonts" << app + "/../src/fonts" << app + "/../../src/fonts";
    return dirs;
}

inline void loadFonts() {
    for (const QString& name : {":/fonts/B612-Regular.ttf", ":/fonts/B612-Bold.ttf", ":/fonts/B612Mono-Regular.ttf", ":/fonts/B612Mono-Bold.ttf"})
        QFontDatabase::addApplicationFont(name);
    for (const QString& dir : fontDirs()) {
        const QDir d(dir);
        for (const QString& file : d.entryList({"*.ttf", "*.otf"}, QDir::Files))
            QFontDatabase::addApplicationFont(d.filePath(file));
    }
}

inline QFont make(const QStringList& families, double px, bool bold, double spacing = 0) {
    QFont f;
    f.setFamilies(families);
    f.setPixelSize(static_cast<int>(px));
    f.setBold(bold);
    f.setStyleStrategy(QFont::PreferAntialias);
    f.setHintingPreference(QFont::PreferNoHinting);
    if (spacing != 0)
        f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
    return f;
}

inline QFont sans(double px, bool bold = false, double spacing = 0) {
    return make({"B612", "Arial Narrow", "Liberation Sans Narrow", "Helvetica Neue", "Arial", "sans-serif"}, px, bold, spacing);
}

inline QFont mono(double px, bool bold = false) {
    return make({"B612 Mono", "Consolas", "DejaVu Sans Mono", "Courier New", "monospace"}, px, bold);
}

inline QColor mix(const QColor& a, const QColor& b, double t) {
    return QColor::fromRgbF(a.redF() * (1 - t) + b.redF() * t, a.greenF() * (1 - t) + b.greenF() * t, a.blueF() * (1 - t) + b.blueF() * t);
}

}
