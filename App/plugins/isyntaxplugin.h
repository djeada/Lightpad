#ifndef ISYNTAXPLUGIN_H
#define ISYNTAXPLUGIN_H

#include "iplugin.h"
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QVector>

struct SyntaxRule {
  QRegularExpression pattern;
  QTextCharFormat format;
  QString name;
};

struct MultiLineBlock {
  QRegularExpression startPattern;
  QRegularExpression endPattern;
  QTextCharFormat format;
};

class ISyntaxPlugin : public IPlugin {
public:
  virtual ~ISyntaxPlugin() = default;

  virtual QString languageId() const = 0;

  virtual QString languageName() const = 0;

  virtual QStringList fileExtensions() const = 0;

  virtual QVector<SyntaxRule> syntaxRules() const = 0;

  virtual QVector<MultiLineBlock> multiLineBlocks() const { return {}; }

  virtual QStringList keywords() const { return {}; }

  virtual QPair<QString, QPair<QString, QString>> commentStyle() const {
    return {"//", {"/*", "*/"}};
  }
};

#define ISyntaxPlugin_iid "org.lightpad.ISyntaxPlugin/1.0"
Q_DECLARE_INTERFACE(ISyntaxPlugin, ISyntaxPlugin_iid)

#endif
