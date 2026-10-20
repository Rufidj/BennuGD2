#include <QApplication>
#include <clocale>

#include "ui/MainWindow.h"
#include "ui/Theme.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    setlocale(LC_NUMERIC, "C");
    app.setApplicationName("Recovery Studio");
    app.setOrganizationName("Recovery Studio");

    Theme::applyDark(app);

    MainWindow window;
    window.resize(1480, 920);
    window.show();

    return app.exec();
}

