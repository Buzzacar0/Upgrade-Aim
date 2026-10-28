#pragma once

#include <QColor>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QLabel>
#include <QPalette>
#include <QString>

class QPainter;
class QRectF;
class QVBoxLayout;

namespace ui {

// Dark palette in the style of macOS system colors. Custom-painted widgets
// and the style sheet use the same values.
struct Colors {
    static QColor background() { return QColor(0x10, 0x10, 0x12); }  // window
    static QColor sidebar() { return QColor(0x17, 0x17, 0x1a); }
    static QColor panel() { return QColor(0x1c, 0x1c, 0x1f); }       // cards
    static QColor control() { return QColor(0x2a, 0x2a, 0x2e); }     // fields, buttons
    static QColor separator() { return QColor(0x2c, 0x2c, 0x30); }
    static QColor grid() { return QColor(0x26, 0x26, 0x2a); }
    static QColor axis() { return QColor(0x48, 0x48, 0x4a); }
    static QColor text() { return QColor(0xf5, 0xf5, 0xf7); }
    static QColor muted() { return QColor(0x98, 0x98, 0x9d); }
    static QColor tertiary() { return QColor(0x63, 0x63, 0x66); }
    static QColor raw() { return QColor(0xae, 0xae, 0xb2); }
    static QColor hip() { return QColor(0x0a, 0x84, 0xff); }   // system blue
    static QColor ads() { return QColor(0xff, 0x9f, 0x0a); }   // system orange
    static QColor good() { return QColor(0x30, 0xd1, 0x58); }  // system green
    static QColor warn() { return QColor(0xff, 0xd6, 0x0a); }  // system yellow
    static QColor bad() { return QColor(0xff, 0x45, 0x3a); }   // system red
};

QString styleSheet();
QPalette palette();
QString fontFamily();  // Segoe UI Variable on Windows 11, Segoe UI otherwise

// Rounded card background for custom-painted widgets (transparent corners).
void paintCard(QPainter& p, const QRectF& r);
// Dark title bar and matching caption color on Windows 11 (no-op elsewhere).
void applyWindowChrome(QWidget* window);

// A card container (rounded panel). Returns the card; its layout is set in *layout.
QFrame* card(QVBoxLayout** layout, int margin = 18);

QDoubleSpinBox* spin(double min, double max, double step, int decimals, double value = 0.0);
QLabel* heading(const QString& text);     // large page title
QLabel* subheading(const QString& text);  // secondary line under a title
QLabel* note(const QString& text);        // small secondary explanatory text
QLabel* caption(const QString& text);     // small label above a value
QString fmt(double v, int decimals = 2);

}  // namespace ui
