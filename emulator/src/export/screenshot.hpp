#pragma once

#include <QImage>
#include <QString>

#include "export/export_geometry.hpp"

QImage RenderExportImage(const ScreenPixels& pixels);

bool SaveScreenshotJpeg(const ScreenPixels& pixels, const QString& path);
