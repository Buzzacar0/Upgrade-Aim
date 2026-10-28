#include <windows.h>

#include <QApplication>
#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <QIcon>
#include <QTimer>

#include "app_controller.hpp"
#include "i18n.hpp"
#include "launcher.hpp"
#include "stickyaim/platform/hidhide.hpp"
#include "stickyaim/platform/license.hpp"

#include <cstdio>
#include <string>
#include "main_window.hpp"
#include "stickyaim/core/logger.hpp"
#include "stickyaim/platform/win_util.hpp"
#include "widgets/ui_helpers.hpp"

int main(int argc, char** argv) {
    // Elevated helper started by Settings → "Hide the controller from games":
    //   UpgradeAim.exe --hidhide-setup <VID hex> <PID hex> [--undo]
    if (argc >= 4 && std::string(argv[1]) == "--hidhide-setup") {
        const auto vid = static_cast<std::uint16_t>(std::stoul(argv[2], nullptr, 16));
        const auto pid = static_cast<std::uint16_t>(std::stoul(argv[3], nullptr, 16));
        const bool undo = argc >= 5 && std::string(argv[4]) == "--undo";
        wchar_t exe[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::string log;
        const int rc = sa::win::hidhide::run(vid, pid, undo, sa::win::toUtf8(exe), &log);
        const std::string logDir = sa::win::userDataDir() + "\\logs";
        CreateDirectoryW(sa::win::fromUtf8(logDir).c_str(), nullptr);
        if (FILE* f = _wfopen(sa::win::fromUtf8(logDir + "\\hidhide.log").c_str(), L"w")) {
            std::fputs(log.c_str(), f);
            std::fclose(f);
        }
        return rc;
    }

    QApplication qapp(argc, argv);
    QApplication::setApplicationName("Upgrade Aim");
    QApplication::setOrganizationName("Upgrade Aim");
    QApplication::setApplicationVersion("0.1.1");
    qapp.setStyle("Fusion");
    qapp.setPalette(ui::palette());
    qapp.setFont(QFont(ui::fontFamily(), 10));
    qapp.setStyleSheet(ui::styleSheet());

    const std::string logDir = sa::win::userDataDir() + "\\logs";
    CreateDirectoryW(sa::win::fromUtf8(logDir).c_str(), nullptr);
    sa::Logger::instance().start(logDir + "\\app.jsonl");
    sa::logInfo("app", "Upgrade Aim starting");

    // Language: the launcher asks (pt-BR by default); --lang en|pt-BR skips it,
    // and so does --screenshots (English unless --lang says otherwise).
    const QStringList args = QApplication::arguments();
    if (const auto i = args.indexOf("--write-icon"); i >= 0 && i + 1 < args.size()) {
        // Build helper: write resources/app.ico (PNG-compressed entries, 16-256 px).
        const QIcon icon(":/ui/up-icon.png");
        const int sizes[] = {16, 20, 24, 32, 40, 48, 64, 128, 256};
        QList<QByteArray> pngs;
        for (int s : sizes) {
            QByteArray png;
            QBuffer buf(&png);
            buf.open(QIODevice::WriteOnly);
            icon.pixmap(s, s).toImage().save(&buf, "PNG");
            pngs << png;
        }
        QFile f(args[i + 1]);
        if (!f.open(QIODevice::WriteOnly)) return 1;
        QDataStream out(&f);
        out.setByteOrder(QDataStream::LittleEndian);
        out << quint16(0) << quint16(1) << quint16(pngs.size());
        quint32 offset = 6 + 16 * static_cast<quint32>(pngs.size());
        for (int k = 0; k < pngs.size(); ++k) {
            const int s = sizes[k];
            out << quint8(s >= 256 ? 0 : s) << quint8(s >= 256 ? 0 : s) << quint8(0) << quint8(0) << quint16(1) << quint16(32)
                << quint32(pngs[k].size()) << offset;
            offset += static_cast<quint32>(pngs[k].size());
        }
        for (const auto& png : pngs) out.writeRawData(png.constData(), static_cast<int>(png.size()));
        return 0;
    }
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
    // Without a valid activation the start screen always appears (it holds the
    // activation field), whatever the command line says.
    const bool licensed = sa::win::license::installedLicense().valid;
    if (licensed && langArg >= 0 && langArg + 1 < args.size()) {
        i18n::setLanguage(i18n::fromCode(args[langArg + 1]));
    } else if (licensed && shotsArg >= 0) {
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
    sa::logInfo("app", "Upgrade Aim exiting");
    sa::Logger::instance().stop();
    return rc;
}
