#pragma once

#include <QDialog>
#include <QPixmap>

#include "i18n.hpp"
#include "stickyaim/platform/license.hpp"

class QLabel;
class QLineEdit;
class QPushButton;

// Start screen: background picture at reduced opacity, language choice
// (Portuguese (Brazil) by default), activation code and Start. The choice and the picture path
// are remembered in %APPDATA%\UpgradeAim\launcher.ini.
class Launcher : public QDialog {
    Q_OBJECT
public:
    explicit Launcher(QWidget* parent = nullptr);
    i18n::Lang language() const { return lang_; }

    // Language remembered from the last launch (pt-BR on first run).
    static i18n::Lang savedLanguage();

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    void setLanguage(i18n::Lang lang);
    void chooseBackground();
    void loadBackground();
    void retranslate();
    void activate();

    i18n::Lang lang_;
    QPixmap background_;
    QLabel *subtitle_, *languageLabel_, *note_;
    QPushButton *pt_, *en_, *start_, *image_;
    // Activation (shown until a valid code is stored)
    sa::win::license::Info license_;
    QWidget* activation_;
    QLabel *activationTitle_, *machineLabel_, *activationError_;
    QLineEdit* code_;
    QPushButton *activate_, *copyId_;
};
