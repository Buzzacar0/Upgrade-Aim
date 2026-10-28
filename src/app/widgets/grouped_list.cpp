#include "widgets/grouped_list.hpp"

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace ui {

Group::Group(const QString& header, QWidget* parent) : QWidget(parent) {
    setObjectName("group");
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    if (!header.isEmpty()) {
        auto* h = new QLabel(header);
        h->setObjectName("groupHeader");
        v->addWidget(h);
    }
    auto* card = new QFrame;
    card->setObjectName("groupCard");
    card_ = new QVBoxLayout(card);
    card_->setContentsMargins(0, 0, 0, 0);
    card_->setSpacing(0);
    v->addWidget(card);
    footer_ = new QLabel;
    footer_->setObjectName("groupFooter");
    footer_->setWordWrap(true);
    footer_->setTextFormat(Qt::RichText);
    footer_->hide();
    v->addWidget(footer_);
}

QWidget* Group::startRow() {
    auto* row = new QWidget;
    row->setObjectName("groupRow");
    auto* v = new QVBoxLayout(row);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    if (rows_++ > 0) {
        // Inset hairline, as in macOS lists.
        auto* sepRow = new QHBoxLayout;
        sepRow->setContentsMargins(16, 0, 0, 0);
        auto* sep = new QFrame;
        sep->setObjectName("rowSep");
        sep->setFixedHeight(1);
        sepRow->addWidget(sep);
        v->addLayout(sepRow);
    }
    card_->addWidget(row);
    return row;
}

QWidget* Group::addRow(const QString& title, QWidget* trailing, const QString& subtitle) {
    QWidget* row = startRow();
    auto* body = new QWidget;
    body->setObjectName("groupRow");
    body->setMinimumHeight(44);
    auto* h = new QHBoxLayout(body);
    h->setContentsMargins(16, 9, 14, 9);
    h->setSpacing(14);
    auto* texts = new QVBoxLayout;
    texts->setSpacing(1);
    auto* t = new QLabel(title);
    t->setObjectName("rowTitle");
    texts->addWidget(t);
    if (!subtitle.isEmpty()) {
        auto* s = new QLabel(subtitle);
        s->setObjectName("rowSubtitle");
        s->setWordWrap(true);
        texts->addWidget(s);
    }
    h->addLayout(texts, 1);
    if (trailing) {
        if (qobject_cast<QAbstractSpinBox*>(trailing)) trailing->setFixedWidth(124);
        if (auto* combo = qobject_cast<QComboBox*>(trailing)) {
            combo->setMinimumWidth(190);
            combo->setMaximumWidth(300);
        }
        h->addWidget(trailing, 0, Qt::AlignVCenter | Qt::AlignRight);
    }
    row->layout()->addWidget(body);
    return row;
}

QWidget* Group::addContent(QWidget* content, int margin) {
    QWidget* row = startRow();
    auto* body = new QWidget;
    body->setObjectName("groupRow");
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(margin, margin * 3 / 4, margin, margin * 3 / 4);
    v->addWidget(content);
    row->layout()->addWidget(body);
    return row;
}

void Group::setFooter(const QString& text) {
    footer_->setText(text);
    footer_->setVisible(!text.isEmpty());
}

Segmented::Segmented(const QStringList& items, QWidget* parent) : QFrame(parent) {
    setObjectName("segmented");
    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(2, 2, 2, 2);
    h->setSpacing(2);
    for (int i = 0; i < items.size(); ++i) {
        auto* b = new QPushButton(items[i]);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setChecked(i == 0);
        connect(b, &QPushButton::clicked, this, [this, i] {
            setCurrent(i);
            Q_EMIT currentChanged(i);
        });
        buttons_ << b;
        h->addWidget(b);
    }
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void Segmented::setCurrent(int index) {
    current_ = index;
    for (int i = 0; i < buttons_.size(); ++i) buttons_[i]->setChecked(i == index);
}

QString Segmented::currentText() const { return buttons_.value(current_) ? buttons_[current_]->text() : QString(); }

QVBoxLayout* centeredColumn(QWidget* host, int maxWidth) {
    auto* outer = new QHBoxLayout(host);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* column = new QWidget;
    column->setObjectName("column");
    column->setMaximumWidth(maxWidth);
    auto* v = new QVBoxLayout(column);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(22);
    outer->addStretch(1);
    outer->addWidget(column, 100);
    outer->addStretch(1);
    return v;
}

QPushButton* linkButton(const QString& text) {
    auto* b = new QPushButton(text);
    b->setObjectName("link");
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

}  // namespace ui
