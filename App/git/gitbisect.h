#ifndef GITBISECT_H
#define GITBISECT_H

#include "gitcommittypes.h"
#include <QList>
#include <QString>
#include <QStringList>

class GitIntegration;

enum class GitBisectStatus {
  NotStarted,

  Searching,

  Found,
};

struct GitBisectState {
  bool valid = false;
  GitBisectStatus status = GitBisectStatus::NotStarted;

  QString currentHash;
  QString currentSubject;

  int revisionsLeft = 0;
  int estimatedSteps = 0;

  QString suspectHash;
  QString suspectSubject;

  QStringList log;
  int goodCount = 0;
  int badCount = 0;
  int skipCount = 0;
};

QString gitBisectStatusName(GitBisectStatus status);

QString gitBisectGuidance(const GitBisectState &state);

void parseBisectProgress(const QString &output, GitBisectState *state);

void parseBisectLog(const QString &log, GitBisectState *state);

QString gitBisectExitCodeMeaning(int exitCode);

GitBisectState buildBisectState(GitIntegration *git);

#endif
