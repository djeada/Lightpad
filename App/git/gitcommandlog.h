#ifndef GITCOMMANDLOG_H
#define GITCOMMANDLOG_H

#include "gitoperationpreview.h"
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

enum class GitCommandMirrorMode {

  Hidden,

  Learn,

  Preview,

  Expert,
};

struct GitCommandRecord {
  QStringList args;
  QString workingDirectory;
  QString output;
  QString error;
  int exitCode = 0;
  bool succeeded = false;
  QDateTime when;

  QString commandLine() const;
};

QString gitCommandMirrorModeName(GitCommandMirrorMode mode);

QString formatGitCommand(const QStringList &args);

QString redactSecrets(const QString &text);

QString gitCommandExplanation(const QStringList &args);

GitOperationRisk gitCommandRisk(const QStringList &args);

bool gitCommandIsReadOnly(const QStringList &args);

#endif
