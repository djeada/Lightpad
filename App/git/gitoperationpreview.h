#ifndef GITOPERATIONPREVIEW_H
#define GITOPERATIONPREVIEW_H

#include "gitcommittypes.h"
#include "gitsyncmodel.h"
#include <QList>
#include <QString>
#include <QStringList>

class GitIntegration;

enum class GitOperationKind {
  Merge,
  Rebase,
  Reset,
  Revert,
  CherryPick,
  Pull,
  BranchDelete,
  StashApply,
  StashPop,
};

enum class GitOperationRisk {
  Safe,
  RewritesLocalHistory,
  MayDiscardUncommitted,
  AffectsSharedHistory,
};

struct GitOperationRequest {
  GitOperationKind kind = GitOperationKind::Merge;

  QString target;

  QString resetMode = QStringLiteral("mixed");
  GitPullStrategy pullStrategy = GitPullStrategy::Merge;
  int stashIndex = 0;
};

struct PreviewNode {
  enum class State {
    Unchanged,

    Added,

    Rewritten,

    Unreachable,
  };

  QString shortHash;
  QString subject;
  State state = State::Unchanged;

  int lane = 0;
  bool isHead = false;
};

struct GitOperationEffect {
  bool valid = false;
  QString error;

  QList<GitCommitInfo> commitsAdded;
  QList<GitCommitInfo> commitsRewritten;
  QList<GitCommitInfo> commitsUnreachable;

  bool changesWorkingTree = false;
  bool changesIndex = false;
  bool movesBranch = false;
  bool detachesHead = false;

  QStringList atRiskPaths;

  QStringList conflictPaths;
  bool conflictsKnown = false;

  GitOperationRisk risk = GitOperationRisk::Safe;
  QString rationale;

  QStringList commands;
};

struct GitOperationPreview {
  GitOperationRequest request;
  GitOperationEffect effect;
  QList<PreviewNode> before;
  QList<PreviewNode> after;
};

QString gitOperationKindName(GitOperationKind kind);
QString gitOperationRiskName(GitOperationRisk risk);

QString gitOperationHeadline(const GitOperationPreview &preview);

QStringList parseMergeTreeConflicts(const QString &output);

GitOperationPreview previewGitOperation(GitIntegration *git,
                                        const GitOperationRequest &request,
                                        int contextCommits = 8);

#endif
