#include <QApplication>

#include "main_window.hpp"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  QStringList files;
  for (int i = 1; i < argc; ++i)
    files << argv[i];

  MainWindow mainwindow;
  mainwindow.setFiles(files);
  mainwindow.show();
  mainwindow.startProcessing();

  app.exec();
}
