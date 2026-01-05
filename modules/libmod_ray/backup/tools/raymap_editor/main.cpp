#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("RayMap Editor");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("BennuGD2");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
