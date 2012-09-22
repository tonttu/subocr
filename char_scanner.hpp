#ifndef CHAR_SCANNER_HPP
#define CHAR_SCANNER_HPP

#include <QString>
#include <QRect>
#include <QImage>

class CharScanner
{
public:
  class Char
  {
  public:
    Char(const QImage& image, const QRect& rect, int left, int right);
    //Char(const QImage& image, const QRect& rect, const Char& merged);
    Char(const QString& txt, bool italic);
    Char(const Char&) = default;
    Char& operator=(const Char&) = default;
    Char() {}

    bool search();
    const QString& text() const;
    void setText(const QString& text, bool italic);

    const QRect& rect() const { return m_rect; }
    const QImage& image() const { return m_image; }
    bool italic() const { return m_italic; }
    int left() const { return m_left; }
    int right() const { return m_right; }

  private:
    QByteArray m_cacheKey;
    QImage m_image;
    QRect m_rect;
    QString m_text;
    bool m_italic;
    int m_left, m_right;
  };


public:
  CharScanner(const QImage& image);
  QVector<Char>& chars();

private:
  void scan();
  void mergeScan(QImage& target, QRect& rectOut, const QRect& rectIn);
  Char scanChar(QPoint p);
  QRect selectRect(const QImage& img, const QRect& rectIn, int yl, int yh);
  void floodFill(QImage& target, QRect& rect, QPoint p);

private:
  const QImage& m_image;
  QVector<CharScanner::Char> m_chars;
};

#endif // CHAR_SCANNER_HPP
