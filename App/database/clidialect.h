#ifndef CLIDIALECT_H
#define CLIDIALECT_H

#include "dbtypes.h"
#include "sqlstatementsplitter.h"

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>
#include <memory>

struct CliLaunch {
  QString program;
  QStringList arguments;
  QMap<QString, QString> environment;
  QByteArray stdinPreamble;
  QString error;
};

class CliDialect {
public:
  virtual ~CliDialect() = default;
  static std::unique_ptr<CliDialect> create(DbEngine engine);

  virtual DbEngine engine() const = 0;

  CliLaunch launch(const DbConnectionProfile &profile,
                   const QString &password) const;

  virtual QByteArray frame(const QString &statement, SqlStatementKind kind,
                           const QString &marker) const = 0;

  virtual QString probeStatement() const { return QStringLiteral("SELECT 1"); }

  virtual DbStatementResult parse(const QString &sql, SqlStatementKind kind,
                                  const QString &out, const QString &err,
                                  int maxRows) const = 0;

  virtual bool marksStderr() const { return true; }

  virtual QStringList clientCandidates() const = 0;
  virtual QString passwordVariable() const = 0;

  virtual QStringList clientArguments(const DbConnectionProfile &profile,
                                      bool inContainer) const = 0;
  virtual QMap<QString, QString>
  clientEnvironment(const DbConnectionProfile &profile) const {
    Q_UNUSED(profile)
    return {};
  }

  virtual QString startupError(const QString &err, const QString &out) const;
};

namespace CliParsing {

QVector<QStringList> parseCsv(const QString &text);

QVector<QStringList> parseBatchTsv(const QString &text);

QStringList splitLines(const QString &text);

DbResultSet buildResultSet(const QVector<QStringList> &records,
                           const QString &nullToken, int maxRows);

QString joinNonEmpty(const QStringList &lines);

} // namespace CliParsing

#endif
