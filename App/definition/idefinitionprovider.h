#ifndef IDEFINITIONPROVIDER_H
#define IDEFINITIONPROVIDER_H

#include <QList>
#include <QObject>
#include <QString>

struct DefinitionRequest {
  QString filePath;
  int line;
  int column;
  QString languageId;
};

struct DefinitionTarget {
  QString filePath;
  int line;
  int column;
  QString label;

  bool isValid() const { return !filePath.isEmpty(); }
};

class IDefinitionProvider : public QObject {
  Q_OBJECT

public:
  explicit IDefinitionProvider(QObject *parent = nullptr) : QObject(parent) {}
  ~IDefinitionProvider() override = default;

  virtual QString id() const = 0;
  virtual bool supports(const QString &languageId) const = 0;
  virtual int requestDefinition(const DefinitionRequest &req) = 0;

signals:
  void definitionReady(int requestId, QList<DefinitionTarget> targets);
  void definitionFailed(int requestId, QString error);
};

#endif
