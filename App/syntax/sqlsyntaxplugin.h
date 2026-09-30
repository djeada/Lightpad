#ifndef SQLSYNTAXPLUGIN_H
#define SQLSYNTAXPLUGIN_H

#include "basesyntaxplugin.h"
#include <QVector>

class SqlSyntaxPlugin : public BaseSyntaxPlugin {
public:
  QString languageId() const override { return "sql"; }
  QString languageName() const override { return "SQL"; }
  QStringList fileExtensions() const override {
    return {"sql", "ddl", "dml", "psql", "tsql"};
  }

  QVector<SyntaxRule> syntaxRules() const override;
  QVector<MultiLineBlock> multiLineBlocks() const override;
  QStringList keywords() const override;

  QPair<QString, QPair<QString, QString>> commentStyle() const override {
    return {"--", {"/*", "*/"}};
  }
};

#endif
