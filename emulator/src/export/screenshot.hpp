#pragma once

#include <QImage>
#include <QString>

#include "export/export_geometry.hpp"

QImage renderExportImage(const ScreenPixels& pixels);

bool saveScreenshotJpeg(const ScreenPixels& pixels, const QString& path);
