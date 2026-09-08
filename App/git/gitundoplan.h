#ifndef GITUNDOPLAN_H
#define GITUNDOPLAN_H

#include "gitoperationpreview.h"
#include <QList>
#include <QString>
#include <QStringList>

class GitIntegration;

enum class GitUndoGoal {
  DiscardWorkingEdits,
  UnstageKeepEdits,
  MoveBranchKeepStaged,
  MoveBranchKeepUnstaged,
  ResetEverything,
  RevertPublishedCommit,
  AmendLastCommit,
};

struct GitUndoLayerMatrix {
  QString workingTree;
  QString index;
  QString head;
  QString branch;
};

struct GitUndoOption {
  GitUndoGoal goal = GitUndoGoal::DiscardWorkingEdits;

  QString question;

  QString command;

  QString explanation;
  GitUndoLayerMatrix matrix;

  bool destructive = false;
  QStringList atRiskPaths;

  bool hasOperationPreview = false;
  GitOperationKind previewKind = GitOperationKind::Reset;
  QString previewTarget;
  QString resetMode = QStringLiteral("mixed");

  QString unavailableReason;
  bool isAvailable() const { return unavailableReason.isEmpty(); }
};

struct GitUndoPlan {
  bool valid = false;
  QString error;

  QString targetCommit;
  QStringList selectedPaths;

  bool targetIsPublished = false;

  QList<GitUndoOption> options;
  GitUndoGoal recommended = GitUndoGoal::DiscardWorkingEdits;
  QString recommendationReason;
};

QString gitUndoGoalName(GitUndoGoal goal);

GitUndoPlan buildGitUndoPlan(GitIntegration *git,
                             const QString &targetCommit = QString(),
                             const QStringList &selectedPaths = QStringList());

#endif
