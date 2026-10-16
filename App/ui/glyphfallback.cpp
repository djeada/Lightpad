#include "glyphfallback.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QPainter>

namespace {

QImage drawGlyph(uint codePoint, const QFont &font) {
  QImage image(40, 40, QImage::Format_ARGB32);
  image.fill(Qt::white);
  QPainter painter(&image);
  painter.setFont(font);
  painter.setPen(Qt::black);
  const QString text = QString::fromUcs4(&codePoint, 1);
  painter.drawText(QRect(0, 0, 40, 40), Qt::AlignCenter, text);
  return image;
}

} // namespace

GlyphFallback::GlyphFallback(QObject *parent) : QObject(parent) {}

bool GlyphFallback::canRender(uint cp) {

  if (cp < 0x2190) {
    return true;
  }
  const auto cached = m_cache.constFind(cp);
  if (cached != m_cache.constEnd()) {
    return cached.value();
  }

  QFont font = QApplication::font();
  font.setPixelSize(24);
  const QImage glyph = drawGlyph(cp, font);

  const QImage missing = drawGlyph(0xFFFF, font);
  const QImage blank = drawGlyph(0x0020, font);
  const bool ok = glyph != missing && glyph != blank;
  m_cache.insert(cp, ok);
  return ok;
}

QString GlyphFallback::sanitize(const QString &text) {
  bool needsWork = false;
  for (int i = 0; i < text.size(); ++i) {
    const QChar c = text.at(i);
    if (c.unicode() >= 0x2190 || c.isHighSurrogate()) {
      needsWork = true;
      break;
    }
  }
  if (!needsWork) {
    return text;
  }

  const QList<uint> ucs4 = text.toUcs4();
  QList<uint> kept;
  kept.reserve(ucs4.size());
  bool removedLast = false;
  for (uint cp : ucs4) {

    if (cp == 0xFE0F || cp == 0xFE0E || cp == 0x200D) {
      continue;
    }
    if (!canRender(cp)) {
      removedLast = true;
      continue;
    }
    if (removedLast && cp == ' ' && kept.isEmpty()) {
      removedLast = false;
      continue;
    }
    removedLast = false;
    kept.append(cp);
  }
  return QString::fromUcs4(kept.constData(), kept.size());
}

bool GlyphFallback::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::Polish) {
    if (auto *button = qobject_cast<QAbstractButton *>(watched)) {
      const QString clean = sanitize(button->text());
      if (clean != button->text()) {
        button->setText(clean);
      }
    } else if (auto *label = qobject_cast<QLabel *>(watched)) {
      if (label->textFormat() != Qt::RichText) {
        const QString clean = sanitize(label->text());
        if (clean != label->text()) {
          label->setText(clean);
        }
      }
    } else if (auto *group = qobject_cast<QGroupBox *>(watched)) {
      const QString clean = sanitize(group->title());
      if (clean != group->title()) {
        group->setTitle(clean);
      }
    }
  } else if (event->type() == QEvent::Show) {
    if (auto *menu = qobject_cast<QMenu *>(watched)) {
      const auto actions = menu->actions();
      for (QAction *action : actions) {
        const QString clean = sanitize(action->text());
        if (clean != action->text()) {
          action->setText(clean);
        }
      }
    }
  }
  return QObject::eventFilter(watched, event);
}
