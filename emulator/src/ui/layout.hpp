#pragma once

#include "ui/workbench.hpp"

// every panel's geometry, recomputed from the window size on each paint/mouse event
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
    double memSh = 0, memTop = 0, memColW = 0;
    QRectF pgUp, pgDn, goBox;
    double portsSh = 0, portsTop = 0;
    double listSh = 0, listTop = 0;
    int listRows = 0;
    QRectF follow;
};
