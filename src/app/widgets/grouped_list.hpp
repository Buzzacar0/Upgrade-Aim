#pragma once

#include <QFrame>
#include <QList>
#include <QStringList>
#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace ui {

// Grouped list in the style of macOS System Settings: an optional header,
// a rounded card of rows separated by inset hairlines, and an optional footer.
class Group : public QWidget {
    Q_OBJECT
public:
    explicit Group(const QString& header = {}, QWidget* parent = nullptr);

    // Title (and optional secondary line) on the left, a compact control on
    // the right. Spin boxes and combo boxes are sized to fit the row. The
    // returned row can be hidden; its separator hides with it.
    QWidget* addRow(const QString& title, QWidget* trailing = nullptr, const QString& subtitle = {});
    // Full-width content inside the card (tables, text editors, plots).
    QWidget* addContent(QWidget* content, int margin = 16);
    void setFooter(const QString& text);
    QLabel* footerLabel() const { return footer_; }

private:
    QWidget* startRow();
    QVBoxLayout* card_;
    QLabel* footer_;
    int rows_ = 0;
};

// Segmented control (mutually exclusive buttons in one rounded track).
class Segmented : public QFrame {
    Q_OBJECT
public:
    explicit Segmented(const QStringList& items, QWidget* parent = nullptr);
    int current() const { return current_; }
    void setCurrent(int index);  // does not emit
    QString currentText() const;

Q_SIGNALS:
    void currentChanged(int index);

private:
    QList<QPushButton*> buttons_;
    int current_ = 0;
};

// A page column with a maximum width, centered in the available space.
// Returns the column's layout; the container is added to `host`.
QVBoxLayout* centeredColumn(QWidget* host, int maxWidth = 760);

QPushButton* linkButton(const QString& text);  // borderless blue text button

}  // namespace ui
