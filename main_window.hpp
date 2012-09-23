#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include <QMainWindow>
#include <QWaitCondition>
#include <QMutex>

namespace Ui
{
  class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT
  
public:
  explicit MainWindow(QWidget* parent = 0);
  ~MainWindow();

  void setFiles(const QStringList& files);
  void startProcessing();

signals:
  void logError(const QString& msg);
  void progress(int value);
  void userInput(const QImage&, const QImage&, QRect);
  void currentFile(const QString& file);

private:
  QImage convertImage(const QImage& img);
  void process();
  QString process(const QImage& img);
  QString postProcess(const QString& in);

private slots:
  void logErrorSlot(const QString& msg);
  void progressSlot(int value);
  void userInputSlot(const QImage& img, const QImage&, QRect rect);
  void currentFileSlot(const QString& file);

  void inputChanged();
  void save();
  void skipImage();

private:
  Ui::MainWindow* m_ui;
  QStringList m_files;

  bool m_aborting;
  QWaitCondition m_userInputCond;
  QMutex m_userInputCondMutex;
  QString m_userInput;
  bool m_userInputItalic;
  bool m_waitingUserInput;
};

#endif // MAIN_WINDOW_HPP
