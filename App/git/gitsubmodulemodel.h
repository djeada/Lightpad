#ifndef GITSUBMODULEMODEL_H
#define GITSUBMODULEMODEL_H

#include <QList>
#include <QString>
#include <QStringList>

enum class GitSubmoduleState {
  Uninitialized,
  Current,
  OutOfDate,
  Conflicted,
};

struct GitSubmoduleInfo {
  QString path;
  QString hash;
  QString describe;
  GitSubmoduleState state = GitSubmoduleState::Current;

  bool isInitialized() const {
    return state != GitSubmoduleState::Uninitialized;
  }
};

QList<GitSubmoduleInfo> parseSubmoduleStatus(const QString &output);

QString gitSubmoduleStateName(GitSubmoduleState state);

QString gitSubmoduleSummary(const QList<GitSubmoduleInfo> &modules);

#endif
