#include "main_window.hpp"
#include "ui_main_window.h"
#include "line_splitter.hpp"
#include "char_scanner.hpp"

#include <QtConcurrentRun>
#include <QPainter>
#include <QPixmap>

MainWindow::MainWindow(QWidget* parent) :
  QMainWindow(parent),
  m_ui(new Ui::MainWindow),
  m_aborting(false),
  m_userInputItalic(false),
  m_waitingUserInput(false)
{
  m_ui->setupUi(this);
  m_ui->log->hide();

  connect(this, SIGNAL(logError(QString)), this, SLOT(logErrorSlot(QString)), Qt::QueuedConnection);
  connect(this, SIGNAL(progress(int)), this, SLOT(progressSlot(int)), Qt::QueuedConnection);
  connect(this, SIGNAL(userInput(QImage,QImage,QRect)), this, SLOT(userInputSlot(QImage,QImage,QRect)), Qt::BlockingQueuedConnection);
  connect(this, SIGNAL(currentFile(QString)), this, SLOT(currentFileSlot(QString)), Qt::QueuedConnection);

  connect(m_ui->okButton, SIGNAL(clicked()), this, SLOT(save()));
  connect(m_ui->input, SIGNAL(returnPressed()), this, SLOT(save()));
  connect(m_ui->input, SIGNAL(textChanged(QString)), this, SLOT(inputChanged()));
  connect(m_ui->skipImageButton, SIGNAL(clicked()), this, SLOT(skipImage()));
}

MainWindow::~MainWindow()
{
  m_aborting = true;
  QMutexLocker locker(&m_userInputCondMutex);
  m_waitingUserInput = false;
  m_userInputCond.wakeAll();
  delete m_ui;
}

void MainWindow::setFiles(const QStringList& files)
{
  m_ui->progressBar->setRange(0, files.size());
  m_ui->progressBar->setValue(0);
  m_files = files;
}

void MainWindow::startProcessing()
{
  QtConcurrent::run(this, &MainWindow::process);
}

void MainWindow::process()
{
  for (int i = 0; i < m_files.size(); ++i) {
    emit currentFile(m_files[i]);
    QImage img(m_files[i]);
    if (img.isNull()) {
      emit logError(QString("Failed to open %1\n").arg(m_files[i]));
    } else {
      if (img.format() != QImage::Format_Indexed8) {
        img = convertImage(img);
      }
      QString text = process(img);
      if (m_aborting)
        return;

      QFile txtFile(m_files[i] + ".txt");
      if (txtFile.open(QFile::WriteOnly)) {
        txtFile.write(text.toUtf8().data());
      } else {
        emit logError(QString("Failed to open '%1': %s2\n").arg(txtFile.fileName(), txtFile.errorString()));
      }
    }
    emit progress(i+1);
  }
}

QImage MainWindow::convertImage(const QImage& img)
{
  QImage target(img.width(), img.height(), QImage::Format_Indexed8);
  QVector<QRgb> palette(256);
  for (int i = 0; i < 256; ++i)
    palette[i] = qRgb(i, i, i);
  target.setColorTable(palette);

  for (int y = 0; y < img.height(); ++y) {
    for (int x = 0; x < img.width(); ++x) {
      QRgb rgb = img.pixel(x, y);

      bool center = qAlpha(rgb) > 127 && qRed(rgb) > 127;
      target.setPixel(x, y, center ? 255 : qAlpha(rgb) > 127 ? 50 : 0);
    }
  }

  return target;
}

QString MainWindow::process(const QImage& img)
{
  QList<QImage> lines = LineSplitter::split(img);

  QString output;
  foreach (const QImage& line, lines) {
    CharScanner scanner(line);
    QString text;
    auto& chars = scanner.chars();

    for (auto it = chars.begin(); it != chars.end(); ++it) {
      if (!it->search()) {
        {
          QMutexLocker locker(&m_userInputCondMutex);
          emit userInput(line, it->image(), it->rect());
          while (m_waitingUserInput)
            m_userInputCond.wait(&m_userInputCondMutex);
          if (m_aborting)
            return "";
        }

        if (m_userInput.isEmpty())
          return QString();
        it->setText(m_userInput, m_userInputItalic);
      }
      text += it->text();
    }

    output += postProcess(text)+"\n";
  }
  return output;
}

QString MainWindow::postProcess(const QString& in)
{
  static bool once = false;
  static bool beginning = true;
  static QRegExp endR("[.!?]\"?$", Qt::CaseSensitive, QRegExp::RegExp2);

  static QList<QPair<QRegExp, QString>> s_rules;
  if (!once) {
    QFile file("renames.txt");
    if (file.open(QFile::ReadOnly)) {
      QStringList rules = QString::fromUtf8(file.readAll().data()).split(QRegExp("[\r\n]+"), QString::SkipEmptyParts);
      foreach (const QString& rule, rules) {
        QStringList tmp = rule.split('\t');
        if (tmp.size() == 2) {
          s_rules << qMakePair(QRegExp(tmp[0], Qt::CaseSensitive, QRegExp::RegExp2), tmp[1]);
        } else {
          emit logError(QString("Invalid rule '%1' in regexp rules file '%1'\n").arg(rule, file.fileName()));
        }
      }
    } else {
      emit logError(QString("Failed to open regexp rules file '%1': %2\n").arg(file.fileName(), file.errorString()));
    }
    once = true;
  }

  QString out = in;
  for (auto it = s_rules.begin(); it != s_rules.end(); ++it) {
    out.replace(it->first, it->second);
  }

  if (beginning && out.startsWith('l'))
    out.replace(0, 1, 'I');

  beginning = endR.indexIn(out) >= 0;
  return out;
}

void MainWindow::logErrorSlot(const QString& msg)
{
  m_ui->log->show();
  QTextCursor cursor = m_ui->log->textCursor();
  cursor.movePosition(QTextCursor::End);
  cursor.insertText(msg);
}

void MainWindow::progressSlot(int value)
{
  m_ui->progressBar->setValue(value);
  if (value == m_ui->progressBar->maximum())
    m_ui->filename->setText("");
}

void MainWindow::userInputSlot(const QImage& img, const QImage& hl, QRect rect)
{
  QImage hlImage = img.convertToFormat(QImage::Format_ARGB32);;

  for (int x = 0; x < hl.width(); ++x) {
    for (int y = 0; y < hl.height(); ++y) {
      if (hl.pixelIndex(x, y) > 127)
        continue;
      hlImage.setPixel(rect.left()+x, rect.top()+y, qRgb(255, 0, 0));
    }
  }

  m_waitingUserInput = true;
  m_ui->img->setPixmap(QPixmap::fromImage(hlImage));
}

void MainWindow::currentFileSlot(const QString& file)
{
  m_ui->filename->setText(file);
}

void MainWindow::inputChanged()
{
  m_ui->okButton->setEnabled(!m_ui->input->text().isEmpty());
}

void MainWindow::save()
{
  m_userInput = m_ui->input->text();
  m_userInputItalic = m_ui->italic->checkState() == Qt::Checked;
  m_ui->input->setText(QString());
  QMutexLocker locker(&m_userInputCondMutex);
  m_waitingUserInput = false;
  m_userInputCond.wakeAll();
  m_ui->img->clear();
}

void MainWindow::skipImage()
{
  m_ui->input->setText(QString());
  save();
}
