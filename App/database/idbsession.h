#ifndef IDBSESSION_H
#define IDBSESSION_H

#include "dbtypes.h"

#include <QObject>
#include <QStringList>

class IDbSession : public QObject {
  Q_OBJECT

public:
  explicit IDbSession(QObject *parent = nullptr) : QObject(parent) {}
  ~IDbSession() override = default;

  virtual void open(const QString &password) = 0;
  virtual void close() = 0;
  virtual bool isOpen() const = 0;
  virtual bool isBusy() const = 0;

  virtual void execute(quint64 requestId, const QStringList &statements,
                       bool stopOnError) = 0;

  virtual void cancel() = 0;

  void setMaxRows(int rows) { m_maxRows = rows; }
  int maxRows() const { return m_maxRows; }

signals:
  void opened();
  void openFailed(const QString &message);
  void closed(const QString &reason);
  void statementFinished(quint64 requestId, int index,
                         const DbStatementResult &result);
  void requestFinished(quint64 requestId, bool cancelled);

protected:
  int m_maxRows = 10000;
};

#endif
