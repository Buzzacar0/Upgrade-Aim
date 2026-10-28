#include <windows.h>

#include <QApplication>
#include <QTimer>

#include "app_controller.hpp"
#include "i18n.hpp"
#include "launcher.hpp"
#include "main_window.hpp"
#include "stickyaim/core/logger.hpp"
#include "stickyaim/platform/win_util.hpp"
#include "widgets/ui_helpers.hpp"

int main(int argc, char** argv) {
    QApplication qapp(argc, argv);
    QApplication::setApplicationName("StickyAim Lab");
    QApplication::setOrganizationName("StickyAim");
    qapp.setStyle("Fusion");
    qapp.setPalette(ui::palette());
    qapp.setFont(QFont(ui::fontFamily(), 10));
    qapp.setStyleSheet(ui::styleSheet());

    const std::string logDir = sa::win::userDataDir() + "\\logs";
    CreateDirectoryW(sa::win::fromUtf8(logDir).c_str(), nullptr);
    sa::Logger::instance().start(logDir + "\\app.jsonl");
    sa::logInfo("app", "StickyAim Lab starting");

    // Language: the launcher asks (pt-BR by default); --lang en|pt-BR skips it,
    // and so does --screenshots (English unless --lang says otherwise).
    const QStringList args = QApplication::arguments();
    if (const auto i = args.indexOf("--launcher-shot"); i >= 0 && i + 1 < args.size()) {
        // Documentation helper: render the launcher to an image and exit.
        Launcher launcher;
        launcher.show();
        QTimer::singleShot(800, &launcher, [&] {
            launcher.grab().save(args[i + 1]);
            qapp.quit();
        });
        const int rc = qapp.exec();
        sa::Logger::instance().stop();
        return rc;
    }
    const auto shotsArg = args.indexOf("--screenshots");
    const auto langArg = args.indexOf("--lang");
    if (langArg >= 0 && langArg + 1 < args.size()) {
        i18n::setLanguage(i18n::fromCode(args[langArg + 1]));
    } else if (shotsArg >= 0) {
        i18n::setLanguage(i18n::Lang::En);
    } else {
        Launcher launcher;
        if (launcher.exec() != QDialog::Accepted) {
            sa::logInfo("app", "launcher closed without starting");
            sa::Logger::instance().stop();
            return 0;
        }
        i18n::setLanguage(launcher.language());
    }
    sa::logInfo("app", "language " + i18n::code(i18n::language()).toStdString());

    int rc = 0;
    {
        AppController app;
        // Start in safe preview: processing is visible, nothing is injected.
        app.startEngine(nullptr);
        MainWindow w(&app);
        w.show();
        if (shotsArg >= 0 && shotsArg + 1 < args.size()) w.captureAllPages(args[shotsArg + 1]);
        rc = qapp.exec();
    }
    sa::logInfo("app", "StickyAim Lab exiting");
    sa::Logger::instance().stop();
    return rc;
}
