#include "char_scanner.hpp"

#include <QQueue>
#include <QDebug>
#include <QSettings>
#include <QBuffer>

/*void blit0(QImage& target, const QImage& src, QPoint offset)
{
  for (int y = 0; y < src.height(); ++y)
    for (int x = 0; x < src.width(); ++x)
      if (src.pixelIndex(x, y) == 0)
        target.setPixel(offset.x() + x, offset.y() + y, 0);
}*/

/*bool operator<(const QImage& a, const QImage& b)
{
  const QSize as = a.size();
  const QSize bs = b.size();
  if (as == bs) {
    return memcmp(a.bits(), b.bits(), a.bytesPerLine() * a.height()) < 0;
  } else {
    return as.width() == bs.width() ? as.height() < bs.height() : as.width() < bs.width();
  }
}
*/

QMap<QByteArray, QPair<QString, bool>> s_cache;

void cacheLoad()
{
  static bool once = false;
  if (once)
    return;
  once = true;

  QSettings settings("Subocr", "Cache");
  const int size = settings.beginReadArray("cache");
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    s_cache[settings.value("image").toByteArray()] =
        qMakePair(settings.value("text").toString(), settings.value("italic").toBool());
  }
  settings.endArray();
}

void cacheSave()
{
  QSettings settings("Subocr", "Cache");
  settings.clear();
  settings.beginWriteArray("cache", s_cache.size());
  int i = 0;
  for (auto it = s_cache.begin(); it != s_cache.end(); ++it) {
    settings.setArrayIndex(i++);
    settings.setValue("image", it.key());
    settings.setValue("text", it->first);
    settings.setValue("italic", it->second);
  }
  settings.endArray();
}

QPair<QString, bool> cacheGet(const QByteArray& key)
{
  return s_cache.value(key);
}

void cachePut(const QByteArray& key, const QString& txt, bool italic)
{
  s_cache[key] = qMakePair(txt, italic);
}

CharScanner::CharScanner(const QImage& image)
  : m_image(image)
{
}

QVector<CharScanner::Char>& CharScanner::chars()
{
  scan();
  return m_chars;
}

void CharScanner::scan()
{
  const int s_space[] = { 10, 8 };

  m_chars.clear();
  //QVector<bool> foundEdge(m_image.height(), false);
  for (int x = 0; x < m_image.width();) {
    for (int y = 0; y < m_image.height(); ++y) {
      bool center = m_image.pixelIndex(x, y) > 127;
      /*bool edge = m_image.pixelIndex(x, y) <= 127;
      if (foundEdge[y] && !edge) {*/
      if (center) {
        // found character center, scan character
        try {
          qDebug() << "Scanning";
          Char chr = scanChar(QPoint(x, y));
          chr.search();
          qDebug() << "Got:" << chr.text();
          if (!m_chars.isEmpty() && m_chars.last().text() != " ") {
            int space = chr.left() - m_chars.last().right();
            if (space > (m_chars.last().italic() ? s_space[1] : s_space[0]))
              m_chars << Char(" ", space > s_space[0]);
          }
          m_chars << chr;
          x = chr.rect().right() + 1;
          break;
        } catch (...) {
          // we were outside or too small img etc
          //foundEdge[y] = false;
        }
      /*} else if (!foundEdge[y] && edge) {
        foundEdge[y] = true;*/
      }
    }
    ++x;
  }
}

void CharScanner::mergeScan(QImage& target, QRect& rectOut, const QRect& rectIn)
{
  for (int x = rectIn.left(); x <= rectIn.right(); ++x) {
    for (int y = rectIn.top(); y <= rectIn.bottom(); ++y) {
      bool center = m_image.pixelIndex(x, y) > 127;

      if (!center) continue;

      rectOut.setLeft(std::min(rectOut.left(), x));
      rectOut.setRight(std::max(rectOut.right(), x));
      rectOut.setTop(std::min(rectOut.top(), y));
      rectOut.setBottom(std::max(rectOut.bottom(), y));

      floodFill(target, rectOut, QPoint(x, y));
    }
  }
}

CharScanner::Char CharScanner::scanChar(QPoint p)
{
  QImage target(m_image.width(), m_image.height(), QImage::Format_Indexed8);
  target.setColorTable(m_image.colorTable());

  memset(target.bits(), 255, target.bytesPerLine() * target.height());

  QRect rect(p, QSize(1, 1));
  floodFill(target, rect, p);
  if (rect.width() < 3 && rect.height() < 3)
    throw "too small";

  const int s_padding = 5;
  const int s_neighborhood = 2;

  if (rect.top() > s_padding) {
    QRect tmp = selectRect(target, rect, rect.top(), rect.top() + s_neighborhood);
    qDebug() << rect << s_padding << tmp;
    if (!tmp.isEmpty())
      mergeScan(target, rect, QRect(tmp.left(), rect.top()-s_padding,
                                    tmp.width(), s_padding));
  }

  if (rect.height() < m_image.height() * 0.8f && rect.bottom() + s_padding < target.rect().bottom()) {
    qDebug() << "low!";
    QRect tmp = selectRect(target, rect, rect.bottom() - s_neighborhood, rect.bottom());
    if (!tmp.isEmpty())
      mergeScan(target, rect, QRect(tmp.left(), rect.bottom(),
                                    tmp.width(), s_padding));
  }

  QRect tmp = selectRect(target, rect, target.height() * 0.4, target.height() * 0.6);
  if (tmp.isEmpty()) {
    return Char(target.copy(rect), rect, rect.left(), rect.right());
  } else {
    return Char(target.copy(rect), rect, tmp.left(), tmp.right());
  }
}

QRect CharScanner::selectRect(const QImage& img, const QRect& rectIn, int yl, int yh)
{
  int min_x = rectIn.right() + 1;
  int max_x = rectIn.left() - 1;
  for (int y = yl; y < yh; ++y) {
    for (int x = rectIn.left(); x <= rectIn.right(); ++x) {
      if (img.pixelIndex(x, y) == 0) {
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
      }
    }
  }
  if (min_x == rectIn.right() + 1) {
    return QRect();
  }
  return QRect(min_x, rectIn.top(), max_x-min_x+1, rectIn.height());
}

void CharScanner::floodFill(QImage& target, QRect& rect, QPoint p)
{
  QQueue<QPoint> q;
  q.enqueue(p);

  while (!q.isEmpty()) {
    p = q.dequeue();

/*    if (p.y() == 0 || p.y() == target.height()-1)
      throw "outside";*/

    if (target.pixelIndex(p) == 0)
      continue;

    rect.setLeft(std::min(rect.left(), p.x()));
    rect.setRight(std::max(rect.right(), p.x()));
    rect.setTop(std::min(rect.top(), p.y()));
    rect.setBottom(std::max(rect.bottom(), p.y()));

    target.setPixel(p, 0);

    if (p.x() > 0 && m_image.pixelIndex(p.x()-1, p.y()) > 127)
      q.enqueue(QPoint(p.x()-1, p.y()));

    if (p.x() < m_image.width()-1 && m_image.pixelIndex(p.x()+1, p.y()) > 127)
      q.enqueue(QPoint(p.x()+1, p.y()));

    if (p.y() > 0 && m_image.pixelIndex(p.x(), p.y()-1) > 127)
      q.enqueue(QPoint(p.x(), p.y()-1));

    if (p.y() < m_image.height()-1 && m_image.pixelIndex(p.x(), p.y()+1) > 127)
      q.enqueue(QPoint(p.x(), p.y()+1));
  }
}

///////////////////////////////////////////////////////////////////////////////

CharScanner::Char::Char(const QImage& image, const QRect& rect, int left, int right)
  : m_image(image),
    m_rect(rect),
    m_italic(false),
    m_left(left),
    m_right(right)
{
  cacheLoad();
  QBuffer buffer(&m_cacheKey);
  buffer.open(QBuffer::WriteOnly);
  m_image.save(&buffer, "PNG");
}
/*
CharScanner::Char::Char(const QImage& image, const QRect& rect, const Char& merged)
{
  m_rect = rect.united(merged.rect());
  m_image = QImage(m_rect.width(), m_rect.height(), QImage::Format_Indexed8);
  m_image.setColorTable(image.colorTable());
  memset(m_image.bits(), 255, m_image.bytesPerLine() * m_image.height());

  blit0(m_image, image, rect.topLeft() - m_rect.topLeft());
  blit0(m_image, merged.m_image, merged.rect().topLeft() - m_rect.topLeft());

  QBuffer buffer(&m_cacheKey);
  buffer.open(QBuffer::WriteOnly);
  m_image.save(&buffer, "PNG");
}*/

CharScanner::Char::Char(const QString& text, bool italic)
  : m_text(text),
    m_italic(italic),
    m_left(100000),
    m_right(0)
{
}

bool CharScanner::Char::search()
{
  if (m_text.isEmpty()) {
    auto p = cacheGet(m_cacheKey);
    m_text = p.first;
    m_italic = p.second;
  }
  return !m_text.isEmpty();
}

const QString& CharScanner::Char::text() const
{
  return m_text;
}

void CharScanner::Char::setText(const QString& text, bool italic)
{
  m_text = text;
  m_italic = italic;
  cachePut(m_cacheKey, text, italic);
  cacheSave();
}
