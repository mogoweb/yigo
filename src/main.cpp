#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include "MainWindow.h"
#include "AppSettings.h"
#include <cstdio>

int main(int argc, char* argv[]) {
    // HiDPI: spec §1 integer-scale baseline (Qt 5.6+ attribute, must be set
    // before QApplication)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    app.setApplicationName("YiGo");
    app.setApplicationVersion("0.5.0");
    app.setOrganizationName("YiGo");
    app.setOrganizationDomain("yigo");   // stable default QSettings location

    // i18n: persisted choice wins; empty follows the system locale
    AppSettings settings;
    QString lang = settings.language();
    if (lang.isEmpty()) {
        const QString sys = QLocale::system().name();   // e.g. "zh_CN"
        lang = sys.startsWith("zh") ? QStringLiteral("zh_CN")
                                    : QStringLiteral("en");
    }
    if (lang != QStringLiteral("en")) {
        // translations live next to the binary at build time, and in
        // /usr/share/yigo/translations when installed
        QTranslator* translator = new QTranslator(&app);
        const QString name = QStringLiteral("yigo_") + lang;
        const QString qtName = QStringLiteral("qtbase_") + lang;
        bool ok = translator->load(name, QStringLiteral(YIGO_TRANSLATIONS_DIR))
                  || translator->load(name,
                       QStringLiteral("/usr/share/yigo/translations"));
        if (ok) {
            app.installTranslator(translator);
            QTranslator* qtTranslator = new QTranslator(&app);
            if (qtTranslator->load(qtName,
                    QStringLiteral("/usr/share/qt5/translations")))
                app.installTranslator(qtTranslator);
        }
    }

    MainWindow w;
    w.show();
    return app.exec();
}
