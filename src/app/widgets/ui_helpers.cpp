#include "widgets/ui_helpers.hpp"

#include <windows.h>
#include <dwmapi.h>

#include <QFontDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#include <cmath>

namespace ui {

QString fontFamily() {
    static const QString family = [] {
        for (const char* f : {"Segoe UI Variable Text", "Segoe UI Variable", "Segoe UI"})
            if (QFontDatabase::hasFamily(f)) return QString(f);
        return QString();
    }();
    return family;
}

QString styleSheet() {
    QString css = R"(
* { outline: 0; }
QWidget { background: #101012; color: #F5F5F7; font-family: "@FONT@"; font-size: 13px; font-weight: 400; }
QLabel, QCheckBox, QRadioButton { background: transparent; }
QScrollArea, QScrollArea > QWidget > QWidget, QStackedWidget { background: transparent; border: none; }
QToolTip { background: #2A2A2E; color: #F5F5F7; border: 1px solid #3A3A3F; border-radius: 6px; padding: 6px 8px; }

/* Sidebar */
QWidget#sidebar { background: #17171A; border-right: 1px solid #222226; }
QWidget#sidebar QLabel { background: transparent; }
QLabel#brand { font-size: 15px; font-weight: 600; color: #F5F5F7; }
QLabel#brandSub { font-size: 11px; color: #8E8E93; }
QListWidget#nav { background: transparent; border: none; font-size: 13px; }
QListWidget#nav::item { padding: 7px 10px; border-radius: 8px; margin: 1px 10px; color: #E5E5EA; }
QListWidget#nav::item:hover { background: rgba(255,255,255,0.05); }
QListWidget#nav::item:selected { background: rgba(10,132,255,0.24); color: #FFFFFF; }

/* Cards: QGroupBox renders as an inset grouped card with its title above */
QGroupBox { background: #1C1C1F; border: 1px solid #26262A; border-radius: 14px; margin-top: 26px;
  padding: 16px 16px 14px 16px; font-size: 12px; font-weight: 600; color: #98989D; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 4px; top: 4px; padding: 0; }
QFrame#card { background: #1C1C1F; border: 1px solid #26262A; border-radius: 16px; }
QFrame#card QLabel { background: transparent; }

/* Typography */
QLabel#heading { font-size: 26px; font-weight: 700; color: #F5F5F7; }
QLabel#subheading { font-size: 13px; color: #98989D; }
QLabel#muted { font-size: 12px; color: #8E8E93; }
QLabel#caption { font-size: 12px; font-weight: 500; color: #98989D; }
QLabel#metric { font-size: 30px; font-weight: 600; color: #F5F5F7; }
QLabel#metricUnit { font-size: 15px; font-weight: 500; color: #98989D; }

/* Buttons */
QPushButton { background: #2A2A2E; border: none; border-radius: 8px; padding: 7px 16px; color: #F5F5F7; font-weight: 500; }
QPushButton:hover { background: #333338; }
QPushButton:pressed { background: #3A3A40; }
QPushButton:checked { background: #0A84FF; color: #FFFFFF; }
QPushButton:disabled { background: #1F1F23; color: #5A5A5F; }
QPushButton[primary="true"] { background: #0A84FF; color: #FFFFFF; font-weight: 600; }
QPushButton[primary="true"]:hover { background: #2E96FF; }
QPushButton[primary="true"]:pressed { background: #0070E0; }
QPushButton#danger { background: rgba(255,69,58,0.16); color: #FF6961; }
QPushButton#danger:hover { background: rgba(255,69,58,0.24); }

/* Fields */
QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit, QPlainTextEdit, QTextEdit {
  background: #2A2A2E; border: 1px solid transparent; border-radius: 7px; padding: 5px 9px;
  selection-background-color: #0A84FF; selection-color: #FFFFFF; }
QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover, QLineEdit:hover { background: #313136; }
QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {
  border: 1px solid #0A84FF; }
QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled { color: #5A5A5F; background: #1F1F23; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox::down-arrow { image: url(:/ui/chevron-down.svg); width: 10px; height: 6px; }
QComboBox QAbstractItemView { background: #2A2A2E; border: 1px solid #3A3A3F; border-radius: 8px; padding: 4px;
  selection-background-color: #0A84FF; selection-color: #FFFFFF; outline: 0; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { border: none; background: transparent; width: 18px; }
QAbstractSpinBox::up-arrow { image: url(:/ui/chevron-up.svg); width: 9px; height: 5px; }
QAbstractSpinBox::down-arrow { image: url(:/ui/chevron-down.svg); width: 9px; height: 5px; }

/* Switches instead of checkboxes */
QCheckBox { spacing: 10px; }
QCheckBox::indicator { width: 38px; height: 22px; }
QCheckBox::indicator:unchecked { image: url(:/ui/switch-off.svg); }
QCheckBox::indicator:checked { image: url(:/ui/switch-on.svg); }
QCheckBox::indicator:unchecked:disabled { image: url(:/ui/switch-off-disabled.svg); }
QCheckBox::indicator:checked:disabled { image: url(:/ui/switch-on-disabled.svg); }
QCheckBox:disabled { color: #5A5A5F; }

/* Segmented control look for tabs */
QTabWidget::pane { border: none; top: 10px; }
QTabWidget::tab-bar { alignment: left; }
QTabBar { background: transparent; }
QTabBar::tab { background: #2A2A2E; color: #D1D1D6; padding: 6px 20px; border: none; margin: 0; font-weight: 500; }
QTabBar::tab:first { border-top-left-radius: 8px; border-bottom-left-radius: 8px; }
QTabBar::tab:last { border-top-right-radius: 8px; border-bottom-right-radius: 8px; }
QTabBar::tab:only-one { border-radius: 8px; }
QTabBar::tab:selected { background: #5A5A5F; color: #FFFFFF; }
QTabBar::tab:hover:!selected { background: #333338; }

/* Tables and lists */
QTableWidget, QTableView, QTreeWidget, QListWidget { background: #1C1C1F; alternate-background-color: #202024;
  border: none; border-radius: 10px; gridline-color: #26262A; selection-background-color: rgba(10,132,255,0.28);
  selection-color: #FFFFFF; }
QTableWidget::item, QTableView::item { padding: 4px 8px; }
QListWidget::item { padding: 5px 8px; border-radius: 6px; }
QListWidget::item:selected { background: rgba(10,132,255,0.28); }
QHeaderView { background: transparent; }
QHeaderView::section { background: transparent; color: #98989D; border: none; border-bottom: 1px solid #2C2C30;
  padding: 6px 8px; font-size: 12px; font-weight: 600; }
QTableCornerButton::section { background: transparent; border: none; }

/* Scroll bars: thin, overlay-like */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: rgba(255,255,255,0.16); border-radius: 3px; min-height: 32px; margin: 0 2px; }
QScrollBar::handle:horizontal { background: rgba(255,255,255,0.16); border-radius: 3px; min-width: 32px; margin: 2px 0; }
QScrollBar::handle:hover { background: rgba(255,255,255,0.28); }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

QSlider::groove:horizontal { height: 4px; background: #3A3A3F; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #0A84FF; border-radius: 2px; }
QSlider::handle:horizontal { background: #FFFFFF; width: 18px; height: 18px; margin: -7px 0; border-radius: 9px; }

/* Status pills */
QLabel#badgePreview, QLabel#badgeInject, QLabel#badgeBypass { border-radius: 11px; padding: 4px 12px; font-size: 12px; font-weight: 600; }
QLabel#badgePreview { background: rgba(48,209,88,0.14); color: #30D158; }
QLabel#badgeInject { background: rgba(255,159,10,0.16); color: #FF9F0A; }
QLabel#badgeBypass { background: rgba(255,214,10,0.14); color: #FFD60A; }

QStatusBar { background: #101012; color: #8E8E93; border-top: 1px solid #1E1E22; font-size: 12px; }
QStatusBar::item { border: none; }
QMessageBox, QDialog { background: #1C1C1F; }
QMenu { background: #2A2A2E; border: 1px solid #3A3A3F; border-radius: 8px; padding: 4px; }
QMenu::item { padding: 6px 18px; border-radius: 5px; }
QMenu::item:selected { background: #0A84FF; }
)";
    return css.replace("@FONT@", fontFamily());
}

QPalette palette() {
    QPalette pal;
    pal.setColor(QPalette::Window, Colors::background());
    pal.setColor(QPalette::WindowText, Colors::text());
    pal.setColor(QPalette::Base, Colors::control());
    pal.setColor(QPalette::AlternateBase, Colors::panel());
    pal.setColor(QPalette::Text, Colors::text());
    pal.setColor(QPalette::Button, Colors::control());
    pal.setColor(QPalette::ButtonText, Colors::text());
    pal.setColor(QPalette::Highlight, Colors::hip());
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::ToolTipBase, Colors::control());
    pal.setColor(QPalette::ToolTipText, Colors::text());
    pal.setColor(QPalette::PlaceholderText, Colors::tertiary());
    pal.setColor(QPalette::Disabled, QPalette::Text, Colors::tertiary());
    pal.setColor(QPalette::Disabled, QPalette::WindowText, Colors::tertiary());
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, Colors::tertiary());
    return pal;
}

void paintCard(QPainter& p, const QRectF& r) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 16, 16);
    p.fillPath(path, Colors::panel());
    p.setPen(QPen(QColor(0x26, 0x26, 0x2a), 1));
    p.drawPath(path);
    p.restore();
}

void applyWindowChrome(QWidget* window) {
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    const QColor c = Colors::background();
    const COLORREF caption = RGB(c.red(), c.green(), c.blue());
    DwmSetWindowAttribute(hwnd, 35 /* DWMWA_CAPTION_COLOR */, &caption, sizeof(caption));
    const COLORREF border = RGB(0x26, 0x26, 0x2a);
    DwmSetWindowAttribute(hwnd, 34 /* DWMWA_BORDER_COLOR */, &border, sizeof(border));
}

QFrame* card(QVBoxLayout** layout, int margin) {
    auto* f = new QFrame;
    f->setObjectName("card");
    auto* l = new QVBoxLayout(f);
    l->setContentsMargins(margin, margin - 2, margin, margin - 2);
    l->setSpacing(6);
    if (layout) *layout = l;
    return f;
}

QDoubleSpinBox* spin(double min, double max, double step, int decimals, double value) {
    auto* s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setSingleStep(step);
    s->setDecimals(decimals);
    s->setValue(value);
    s->setKeyboardTracking(false);
    return s;
}

QLabel* heading(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("heading");
    return l;
}

QLabel* subheading(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("subheading");
    l->setWordWrap(true);
    return l;
}

QLabel* note(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("muted");
    l->setWordWrap(true);
    return l;
}

QLabel* caption(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("caption");
    return l;
}

QString fmt(double v, int decimals) {
    if (!std::isfinite(v)) return "—";
    return QString::number(v, 'f', decimals);
}

}  // namespace ui
