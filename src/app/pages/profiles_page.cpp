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
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

#include "app_controller.hpp"
#include "pages/pages.hpp"
#include "widgets/context_editor.hpp"
#include "widgets/ui_helpers.hpp"
#include "i18n.hpp"

ProfilesPage::ProfilesPage(AppController* app, QWidget* parent) : QWidget(parent), app_(app) {
    auto* root = new QHBoxLayout(this);

    // Library column.
    auto* libCol = new QVBoxLayout;
    libCol->addWidget(ui::heading(T("Aim Profiles")));
    list_ = new QListWidget;
    libCol->addWidget(list_, 1);
    auto* libBtns = new QGridLayout;
    auto* loadBtn = new QPushButton(T("Load"));
    auto* saveBtn = new QPushButton(T("Save"));
    auto* saveAsBtn = new QPushButton(T("Save as…"));
    auto* newBtn = new QPushButton(T("New from current"));
    libBtns->addWidget(loadBtn, 0, 0);
    libBtns->addWidget(saveBtn, 0, 1);
    libBtns->addWidget(saveAsBtn, 1, 0);
    libBtns->addWidget(newBtn, 1, 1);
    libCol->addLayout(libBtns);
    fileLabel_ = ui::note("");
    libCol->addWidget(fileLabel_);
    libCol->addWidget(ui::note(T("Shipped example profiles are read-only starting points; Save writes to your profile folder.")));
    root->addLayout(libCol, 1);

    // Editor column.
    auto* edCol = new QVBoxLayout;
    auto* general = new QGroupBox(T("Profile"));
    auto* gForm = new QFormLayout(general);
    description_ = new QPlainTextEdit;
    description_->setMaximumHeight(60);
    gForm->addRow(T("Description"), description_);
    adsFromLt_ = new QCheckBox(T("Use ADS response while the left trigger is held"));
    adsEnter_ = ui::spin(0.01, 1.0, 0.01, 2, 0.15);
    adsExit_ = ui::spin(0.0, 0.99, 0.01, 2, 0.08);
    transition_ = ui::spin(0.0, 500.0, 1.0, 0, 0.0);
    transition_->setToolTip(T("Crossfade between hip and ADS responses. 0 = switch on the same sample."));
    gForm->addRow(adsFromLt_);
    gForm->addRow(T("ADS enter at LT ≥"), adsEnter_);
    gForm->addRow(T("ADS exit at LT ≤"), adsExit_);
    gForm->addRow(T("Hip ↔ ADS crossfade (ms)"), transition_);
    moveInner_ = ui::spin(0.0, 0.85, 0.005, 3, 0.05);
    moveOuter_ = ui::spin(0.15, 1.0, 0.005, 3, 0.98);
    gForm->addRow(T("Move stick inner deadzone"), moveInner_);
    gForm->addRow(T("Move stick outer threshold"), moveOuter_);
    ltInner_ = ui::spin(0.0, 0.9, 0.01, 2, 0.0);
    rtInner_ = ui::spin(0.0, 0.9, 0.01, 2, 0.0);
    gForm->addRow(T("Left trigger deadzone"), ltInner_);
    gForm->addRow(T("Right trigger deadzone"), rtInner_);
    edCol->addWidget(general);

    auto* tabs = new QTabWidget;
    hip_ = new ContextEditor;
    ads_ = new ContextEditor;
    auto wrap = [](QWidget* w) {
        auto* s = new QScrollArea;
        s->setWidgetResizable(true);
        s->setWidget(w);
        return s;
    };
    tabs->addTab(wrap(hip_), T("Hip-fire"));
    tabs->addTab(wrap(ads_), T("ADS"));
    edCol->addWidget(tabs, 1);
    edCol->addWidget(ui::note(T("Fine adjustments and fast movements are configured per context with the fine (near-center) and "
                              "fast (near-rim) bands of each curve, and with the smoothing flick bypass.")));
    errors_ = new QLabel;
    errors_->setStyleSheet("color: #FF453A");
    errors_->setWordWrap(true);
    edCol->addWidget(errors_);
    root->addLayout(edCol, 3);

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
    connect(newBtn, &QPushButton::clicked, this, saveAs);
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
