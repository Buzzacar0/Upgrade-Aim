#include <QGridLayout>
#include <QRegularExpression>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include "app_controller.hpp"
#include "pages/pages.hpp"
#include "widgets/context_editor.hpp"
#include "widgets/grouped_list.hpp"
#include "widgets/ui_helpers.hpp"
#include "i18n.hpp"

ProfilesPage::ProfilesPage(AppController* app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(0, 0, 0, 0);
    page->setSpacing(18);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(ui::heading(T("Aim Profiles")));
    titles->addWidget(ui::subheading(T("Choose a profile and tune how each context responds. Changes apply immediately.")));
    page->addLayout(titles);

    auto* root = new QHBoxLayout;
    root->setSpacing(24);
    page->addLayout(root, 1);

    // Library column.
    auto* libCol = new QVBoxLayout;
    libCol->setSpacing(10);
    auto* libHeader = new QLabel(T("Library"));
    libHeader->setObjectName("groupHeader");
    libCol->addWidget(libHeader);
    list_ = new QListWidget;
    list_->setMinimumHeight(220);
    libCol->addWidget(list_);
    auto* libBtns = new QHBoxLayout;
    libBtns->setSpacing(8);
    auto* loadBtn = new QPushButton(T("Use"));
    loadBtn->setProperty("primary", true);
    auto* saveBtn = new QPushButton(T("Save"));
    auto* saveAsBtn = new QPushButton(T("Save as…"));
    libBtns->addWidget(loadBtn);
    libBtns->addWidget(saveBtn);
    libBtns->addWidget(saveAsBtn);
    libCol->addLayout(libBtns);
    fileLabel_ = ui::note("");
    libCol->addWidget(fileLabel_);
    libCol->addWidget(ui::note(T("Shipped example profiles are read-only starting points; Save writes to your profile folder.")));
    libCol->addStretch();
    auto* libHost = new QWidget;
    libHost->setObjectName("column");
    libHost->setFixedWidth(300);
    libHost->setLayout(libCol);
    root->addWidget(libHost, 0, Qt::AlignTop);

    // Editor column.
    auto* edCol = new QVBoxLayout;
    edCol->setSpacing(22);
    auto* general = new ui::Group(T("Profile"));
    description_ = new QPlainTextEdit;
    description_->setFixedHeight(74);
    description_->setPlaceholderText(T("Description"));
    general->addContent(description_, 12);
    adsFromLt_ = new QCheckBox;
    general->addRow(T("ADS on the left trigger"), adsFromLt_, T("Use the ADS response while the left trigger is held."));
    adsEnter_ = ui::spin(0.01, 1.0, 0.01, 2, 0.15);
    general->addRow(T("ADS enter at LT ≥"), adsEnter_);
    adsExit_ = ui::spin(0.0, 0.99, 0.01, 2, 0.08);
    general->addRow(T("ADS exit at LT ≤"), adsExit_);
    transition_ = ui::spin(0.0, 500.0, 1.0, 0, 0.0);
    general->addRow(T("Hip ↔ ADS crossfade (ms)"), transition_, T("0 = switch on the same sample."));
    edCol->addWidget(general);

    auto* move = new ui::Group(T("Movement stick and triggers"));
    moveInner_ = ui::spin(0.0, 0.85, 0.005, 3, 0.05);
    move->addRow(T("Move stick inner deadzone"), moveInner_);
    moveOuter_ = ui::spin(0.15, 1.0, 0.005, 3, 0.98);
    move->addRow(T("Move stick outer threshold"), moveOuter_);
    ltInner_ = ui::spin(0.0, 0.9, 0.01, 2, 0.0);
    move->addRow(T("Left trigger deadzone"), ltInner_);
    rtInner_ = ui::spin(0.0, 0.9, 0.01, 2, 0.0);
    move->addRow(T("Right trigger deadzone"), rtInner_);
    edCol->addWidget(move);

    auto* ctxRow = new QHBoxLayout;
    auto* ctxHeader = new QLabel(T("Aim stick"));
    ctxHeader->setObjectName("groupHeader");
    auto* ctxSwitch = new ui::Segmented({T("Hip-fire"), T("ADS")});
    ctxRow->addWidget(ctxHeader);
    ctxRow->addStretch();
    ctxRow->addWidget(ctxSwitch);
    edCol->addLayout(ctxRow);
    auto* stack = new QStackedWidget;
    hip_ = new ContextEditor;
    ads_ = new ContextEditor;
    stack->addWidget(hip_);
    stack->addWidget(ads_);
    connect(ctxSwitch, &ui::Segmented::currentChanged, stack, &QStackedWidget::setCurrentIndex);
    edCol->addWidget(stack);
    errors_ = new QLabel;
    errors_->setStyleSheet("color: #FF453A");
    errors_->setWordWrap(true);
    edCol->addWidget(errors_);
    edCol->addStretch();
    root->addLayout(edCol, 1);
    connect(hip_, &ContextEditor::changed, this, &ProfilesPage::pushToController);
    connect(ads_, &ContextEditor::changed, this, &ProfilesPage::pushToController);
    for (auto* s : {adsEnter_, adsExit_, transition_, moveInner_, moveOuter_, ltInner_, rtInner_})
        connect(s, &QDoubleSpinBox::valueChanged, this, &ProfilesPage::pushToController);
    connect(adsFromLt_, &QCheckBox::toggled, this, &ProfilesPage::pushToController);
    connect(description_, &QPlainTextEdit::textChanged, this, &ProfilesPage::pushToController);
    connect(app_, &AppController::profileChanged, this, &ProfilesPage::load);

    auto loadSelected = [this] {
        auto* item = list_->currentItem();
        if (!item) return;
        QString err;
        if (!app_->loadProfileFile(item->data(Qt::UserRole).toString(), &err)) QMessageBox::warning(this, T("Load profile"), err);
    };
    connect(loadBtn, &QPushButton::clicked, this, loadSelected);
    connect(list_, &QListWidget::itemDoubleClicked, this, loadSelected);
    auto saveAs = [this] {
        bool ok = false;
        const QString name = QInputDialog::getText(this, T("Save profile"), T("Profile name:"), QLineEdit::Normal,
                                                   QString::fromStdString(app_->profile().name), &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        sa::Profile p = app_->profile();
        p.name = name.trimmed().toStdString();
        app_->setProfile(p);
        QString file = name.trimmed();
        file.replace(QRegularExpression("[^A-Za-z0-9_\\- ]"), "_");
        QString err;
        if (!app_->saveProfileFile(app_->userProfilesDir() + "/" + file + ".json", &err)) QMessageBox::warning(this, T("Save"), err);
        refreshList();
    };
    connect(saveAsBtn, &QPushButton::clicked, this, saveAs);
    connect(saveBtn, &QPushButton::clicked, this, [this, saveAs] {
        const QString path = app_->profilePath();
        // Never overwrite the shipped examples.
        if (path.isEmpty() || !path.startsWith(app_->userProfilesDir())) {
            saveAs();
            return;
        }
        QString err;
        if (!app_->saveProfileFile(path, &err)) QMessageBox::warning(this, T("Save"), err);
    });

    refreshList();
    load();
}

void ProfilesPage::refreshList() {
    list_->clear();
    for (const auto& path : app_->profileLibrary()) {
        auto* item = new QListWidgetItem(QFileInfo(path).completeBaseName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        if (!path.startsWith(app_->userProfilesDir())) item->setText(item->text() + T("  (example)"));
        list_->addItem(item);
    }
}

void ProfilesPage::load() {
    updating_ = true;
    const auto& p = app_->profile();
    hip_->setConfig(p.hip);
    ads_->setConfig(p.ads);
    for (auto* w : std::initializer_list<QWidget*>{adsEnter_, adsExit_, transition_, moveInner_, moveOuter_, ltInner_, rtInner_, adsFromLt_, description_})
        w->blockSignals(true);
    adsFromLt_->setChecked(p.adsFromLeftTrigger);
    adsEnter_->setValue(p.adsEnter);
    adsExit_->setValue(p.adsExit);
    transition_->setValue(p.contextTransitionMs);
    moveInner_->setValue(p.moveStick.inner);
    moveOuter_->setValue(p.moveStick.outer);
    ltInner_->setValue(p.leftTrigger.inner);
    rtInner_->setValue(p.rightTrigger.inner);
    if (description_->toPlainText() != QString::fromStdString(p.description)) description_->setPlainText(QString::fromStdString(p.description));
    for (auto* w : std::initializer_list<QWidget*>{adsEnter_, adsExit_, transition_, moveInner_, moveOuter_, ltInner_, rtInner_, adsFromLt_, description_})
        w->blockSignals(false);
    errors_->setText(app_->profileErrors().join("\n"));
    fileLabel_->setText(QString("<b>%1</b>%2<br>%3")
                            .arg(QString::fromStdString(p.name).toHtmlEscaped())
                            .arg(app_->profileDirty() ? T(" — unsaved changes") : "")
                            .arg(app_->profilePath().isEmpty() ? T("not saved yet") : app_->profilePath().toHtmlEscaped()));
    updating_ = false;
}

void ProfilesPage::pushToController() {
    if (updating_) return;
    sa::Profile p = app_->profile();
    p.hip = hip_->config();
    p.ads = ads_->config();
    p.description = description_->toPlainText().toStdString();
    p.adsFromLeftTrigger = adsFromLt_->isChecked();
    p.adsEnter = static_cast<float>(adsEnter_->value());
    p.adsExit = static_cast<float>(adsExit_->value());
    p.contextTransitionMs = static_cast<float>(transition_->value());
    p.moveStick.inner = static_cast<float>(moveInner_->value());
    p.moveStick.outer = static_cast<float>(moveOuter_->value());
    p.leftTrigger.inner = static_cast<float>(ltInner_->value());
    p.rightTrigger.inner = static_cast<float>(rtInner_->value());
    app_->setProfile(p);
}
