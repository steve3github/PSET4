#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    std::cout << "Program started\n";
    Sleep(2000);
    
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}