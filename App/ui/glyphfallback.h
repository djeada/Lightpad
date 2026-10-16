#ifndef GLYPHFALLBACK_H
#define GLYPHFALLBACK_H

#include <QHash>
#include <QObject>
#include <QString>

class GlyphFallback : public QObject {
  Q_OBJECT

public:
  explicit GlyphFallback(QObject *parent = nullptr);

  QString sanitize(const QString &text);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  bool canRender(uint codePoint);

  QHash<uint, bool> m_cache;
};

#endif
