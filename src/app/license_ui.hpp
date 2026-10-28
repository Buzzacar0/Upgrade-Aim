#pragma once

#include <QDateTime>
#include <QString>

#include "stickyaim/platform/license.hpp"

// Formatting helpers for license state shown in the UI.
namespace license_ui {

inline QString expiryText(const sa::win::license::Info& i) {
    return QDateTime::fromSecsSinceEpoch(sa::win::license::toUnixSeconds(i.expiresMinute)).toLocalTime().toString("dd/MM/yyyy HH:mm");
}

// "2 d 4 h", "3 h 15 min", "12 min" (units read the same in Portuguese and English).
inline QString remainingText(const sa::win::license::Info& i) {
    const qint64 left = std::max<qint64>(0, i.expiresMinute - sa::win::license::nowMinute());
    if (left >= 1440) return QString("%1 d %2 h").arg(left / 1440).arg((left % 1440) / 60);
    if (left >= 60) return QString("%1 h %2 min").arg(left / 60).arg(left % 60);
    return QString("%1 min").arg(left);
}

inline qint64 minutesLeft(const sa::win::license::Info& i) { return i.expiresMinute - sa::win::license::nowMinute(); }

}  // namespace license_ui
