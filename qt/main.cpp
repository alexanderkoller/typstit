#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // Ensure Qt finds its macOS style plugin when running from a dev build.
    // macdeployqt handles this for release builds; for dev builds we point
    // at the Homebrew plugin directory so QMacStyle (native NSControl drawing)
    // is used instead of the cross-platform Fusion fallback.
    QApplication::addLibraryPath("/opt/homebrew/share/qt/plugins");

    QApplication app(argc, argv);
    app.setApplicationName("Typstit");
    app.setOrganizationDomain("typstit.com");

    MainWindow w;
    w.resize(900, 700);
    w.show();
    return app.exec();
}
