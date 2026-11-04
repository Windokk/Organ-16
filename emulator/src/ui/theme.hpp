#pragma once

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QDir>
#include <QCoreApplication>
#include <QString>

namespace theme {

inline const QColor paper("#222222");
inline const QColor panel("#2c2c2c");
inline const QColor well("#181818");
inline const QColor ink("#f2f2f2");
inline const QColor inkSoft("#c2c2c2");
inline const QColor inkFaint("#929292");
inline const QColor rule("#6e6e6e");
inline const QColor ruleSoft("#3c3c3c");
inline const QColor focus("#9cc0e8");
// colour is only used for an active state, everything else is grey
inline const QColor lampRun("#2ee66b");
inline const QColor lampWait("#ff8a1f");
inline const QColor lampHalt("#ff3d3d");
inline const QColor lampOff("#3b3b3b");
inline const QColor bitOn("#ffd23f");
inline const QColor bar("#2c4d78");
inline const QColor barInk("#ffffff");
inline const QColor hi("#4a7fbb");
inline const QColor hiInk("#ffffff");
inline const QColor regGeneral("#75a7ea");
inline const QColor regPointer("#e98a24");
inline const QColor regInstruction("#52ad40");
inline const QColor regInk("#16150f");
inline const QColor stepBtn("#ffd23f");
inline const QColor resetBtn("#ff3d3d");
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
    const QDir bundled(":/fonts");
    for (const QString& file : bundled.entryList({"*.ttf", "*.otf"}, QDir::Files))
        QFontDatabase::addApplicationFont(bundled.filePath(file));
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
    return make({"IBM Plex Sans", "Segoe UI", "Helvetica Neue", "Arial", "sans-serif"}, px, bold, spacing);
}

inline QFont mono(double px, bool bold = false) {
    return make({"IBM Plex Mono", "Consolas", "DejaVu Sans Mono", "Courier New", "monospace"}, px, bold);
}

inline QColor mix(const QColor& a, const QColor& b, double t) {
    return QColor::fromRgbF(a.redF() * (1 - t) + b.redF() * t, a.greenF() * (1 - t) + b.greenF() * t, a.blueF() * (1 - t) + b.blueF() * t);
}

}
