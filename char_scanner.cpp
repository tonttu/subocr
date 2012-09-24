#include "char_scanner.hpp"

#include <QQueue>
#include <QSettings>
#include <QBuffer>

bool findPoint(const QImage& src, QPoint& res, const QPointF& from, const QPointF& to)
{
  const QPointF v = to - from;
  const float len = std::sqrt(v.x()*v.x() + v.y()*v.y());
  const QPointF d = v / len;

  QPointF p(from);
  for (int i = 0; i < len; ++i) {
    int x = p.x();
    int y = p.y();
    if (src.valid(x, y) && src.pixelIndex(x, y) <= 127) {
      res.setX(x);
      res.setY(y);
      return true;
    }
    p += d;
  }
  return false;
}

void drawLine(QImage& target, QPoint p1, QPoint p2)
{
  const QPoint v = p2 - p1;
  const float len = std::sqrt(v.x()*v.x() + v.y()*v.y());
  const QPointF d(v.x() / len, v.y() / len);
  QPointF p(p1);
  for (int i = 0; i < len; ++i) {
    target.setPixel(p.toPoint(), 0);
    p += d;
  }
}

QImage convexHull(QImage src)
{
  const int steps = 100;
  const float r = 2.0f * M_PI / steps;
  float w = src.width() * 0.5f, h = src.height() * 0.5f;
  const float radius = std::sqrt(w * w + h * h);

  bool first = true;
  QPoint firstPoint;
  QPoint prev;

  for (int i = 0; i <= steps; ++i) {
    const float rads = r * i;
    float c = std::cos(rads);
    float s = std::sin(rads);
    QPoint p;
    if (findPoint(src, p, QPointF(w - c * radius, h - s * radius), QPointF(w, h))) {
      if (first) {
        first = false;
        firstPoint = p;
      } else {
        drawLine(src, prev, p);
        prev = p;
      }
    }
  }
  drawLine(src, prev, firstPoint);
  return src;
}

static QMap<QByteArray, QPair<QString, bool>> s_cache;

void compress(QByteArray& data, const QImage& img)
{
  QBuffer buffer(&data);
  buffer.open(QBuffer::WriteOnly);
  img.save(&buffer, "PPM");
}

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
    QByteArray data = settings.value("image").toByteArray();
    QByteArray data2;
    compress(data2, QImage::fromData(data));
    s_cache[data2] = qMakePair(settings.value("text").toString(), settings.value("italic").toBool());
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
  const int s_space[] = { 9, 8 };

  m_chars.clear();
  for (int x = 0; x < m_image.width(); ++x) {
    for (int y = 0; y < m_image.height(); ++y) {
      if (m_image.pixelIndex(x, y) <= 127)
        continue;
      // found character center, scan character
      try {
        Char chr = scanChar(QPoint(x, y));
        chr.search();
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
      }
    }
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
    if (!tmp.isEmpty()) {
      if (tmp.width() < 6) {
        tmp.setLeft(tmp.left()-1);
        tmp.setRight(tmp.right()+1);
      }
      mergeScan(target, rect, QRect(tmp.left(), rect.top()-s_padding,
                                    tmp.width(), s_padding));
    }
  }

  if (rect.height() < m_image.height() * 0.8f && rect.bottom() + s_padding < target.rect().bottom()) {
    QRect tmp = selectRect(target, rect, rect.bottom() - s_neighborhood, rect.bottom());
    if (!tmp.isEmpty()) {
      // colon etc
      int padding = s_padding;
      if (rect.height() < m_image.height() * 0.2f) {
        float v = std::abs(rect.center().y() - m_image.height()/2) / (0.5f*m_image.height());
        // exclude dash
        if (v > 0.2)
          padding = target.rect().bottom() - rect.bottom() - 2;
      }
      mergeScan(target, rect, QRect(tmp.left(), rect.bottom(),
                                    tmp.width(), padding));
    }
  }

  const QImage cropped = target.copy(rect);
  QRect tmp = selectRect(convexHull(cropped), cropped.rect(), cropped.height() * 0.4, cropped.height() * 0.6);
  if (tmp.isEmpty()) {
    return Char(cropped, rect, rect.left(), rect.right());
  } else {
    return Char(cropped, rect, rect.left() + tmp.left(), rect.left() + tmp.right());
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
  compress(m_cacheKey, m_image);
}

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
    if (!m_text.isEmpty() && !m_italic) {
      m_left = m_rect.left();
      m_right = m_rect.right();
    }
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
