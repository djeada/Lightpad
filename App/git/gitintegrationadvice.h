#ifndef GITINTEGRATIONADVICE_H
#define GITINTEGRATIONADVICE_H

#include "gitoperationpreview.h"
#include <QList>
#include <QString>

class GitIntegration;

enum class GitIntegrationIntent {
  BringEverythingIn,
  ReplayMineOnTop,
  BringOneCommit,
  ApplySelectedChanges,
};

struct GitIntegrationOption {
  GitIntegrationIntent intent = GitIntegrationIntent::BringEverythingIn;

  QString question;

  QString commandName;

  QString consequence;

  bool rewritesHistory = false;
  bool affectsSharedHistory = false;

  bool createsCommit = true;

  GitOperationKind operationKind = GitOperationKind::Merge;
  bool hasOperationPreview = true;

  QString unavailableReason;

  bool isAvailable() const { return unavailableReason.isEmpty(); }
};

struct GitIntegrationAdvice {
  bool valid = false;
  QString error;
  QString sourceRef;
  QString targetBranch;

  QString singleCommit;

  QList<GitIntegrationOption> options;

  GitIntegrationIntent recommended = GitIntegrationIntent::BringEverythingIn;
  QString recommendationReason;
};

QString gitIntegrationIntentName(GitIntegrationIntent intent);

GitIntegrationAdvice
adviseGitIntegration(GitIntegration *git, const QString &sourceRef,
                     const QString &singleCommit = QString());

#endif
