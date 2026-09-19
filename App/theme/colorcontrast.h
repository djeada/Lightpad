#ifndef COLORCONTRAST_H
#define COLORCONTRAST_H

#include <QColor>
#include <QtGlobal>
#include <cmath>
#include <initializer_list>

namespace ColorContrast {

constexpr qreal TextRatio = 4.5;
constexpr qreal SecondaryTextRatio = 3.5;
constexpr qreal GlyphRatio = 3.0;

inline QColor flatten(const QColor &color, const QColor &backdrop) {
  if (!color.isValid())
    return color;
  const qreal a = color.alphaF();
  if (a >= 1.0 || !backdrop.isValid())
    return QColor::fromRgbF(color.redF(), color.greenF(), color.blueF());
  return QColor::fromRgbF(color.redF() * a + backdrop.redF() * (1.0 - a),
                          color.greenF() * a + backdrop.greenF() * (1.0 - a),
                          color.blueF() * a + backdrop.blueF() * (1.0 - a));
}

inline qreal luminance(const QColor &color) {
  auto channel = [](qreal c) {
    return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
  };
  return 0.2126 * channel(color.redF()) + 0.7152 * channel(color.greenF()) +
         0.0722 * channel(color.blueF());
}

inline qreal ratio(const QColor &a, const QColor &b) {
  const qreal la = luminance(a);
  const qreal lb = luminance(b);
  return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

inline bool isDark(const QColor &color) { return luminance(color) < 0.179; }

inline QColor mix(const QColor &from, const QColor &to, qreal t) {
  t = qBound(0.0, t, 1.0);
  return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t,
                          from.greenF() + (to.greenF() - from.greenF()) * t,
                          from.blueF() + (to.blueF() - from.blueF()) * t);
}

inline QColor ensure(const QColor &foreground, const QColor &background,
                     qreal minRatio = TextRatio) {
  if (!foreground.isValid() || !background.isValid())
    return foreground;
  const QColor bg = flatten(background, QColor(Qt::black));
  const QColor fg = flatten(foreground, bg);
  if (ratio(fg, bg) >= minRatio)
    return foreground;

  const QColor black(Qt::black);
  const QColor white(Qt::white);
  const QColor target = ratio(black, bg) >= ratio(white, bg) ? black : white;
  if (ratio(target, bg) < minRatio)
    return target;

  qreal lo = 0.0;
  qreal hi = 1.0;
  for (int i = 0; i < 18; ++i) {
    const qreal mid = (lo + hi) / 2.0;
    if (ratio(mix(fg, target, mid), bg) >= minRatio)
      hi = mid;
    else
      lo = mid;
  }
  return mix(fg, target, hi);
}

inline QColor ensureOnAll(QColor foreground,
                          std::initializer_list<QColor> backgrounds,
                          qreal minRatio = TextRatio) {
  for (int pass = 0; pass < 3; ++pass) {
    bool changed = false;
    for (const QColor &bg : backgrounds) {
      const QColor adjusted = ensure(foreground, bg, minRatio);
      if (adjusted != foreground) {
        foreground = adjusted;
        changed = true;
      }
    }
    if (!changed)
      break;
  }
  return foreground;
}

inline QColor bestOf(const QColor &background,
                     std::initializer_list<QColor> candidates,
                     qreal minRatio = TextRatio) {
  const QColor bg = flatten(background, QColor(Qt::black));
  QColor best;
  qreal bestRatio = -1.0;
  for (const QColor &candidate : candidates) {
    if (!candidate.isValid())
      continue;
    const qreal r = ratio(flatten(candidate, bg), bg);
    if (r >= minRatio)
      return candidate;
    if (r > bestRatio) {
      bestRatio = r;
      best = candidate;
    }
  }
  return ensure(best.isValid() ? best : QColor(Qt::white), bg, minRatio);
}

} // namespace ColorContrast

#endif
