#ifndef GITSYNCMODEL_H
#define GITSYNCMODEL_H

#include "gitcommittypes.h"
#include <QList>
#include <QString>

enum class GitPullStrategy {
  Merge,
  Rebase,
  FastForwardOnly,
};

enum class GitPushForce {
  None,
  WithLease,
  Force,
};

struct GitSyncState {
  bool valid = false;
  QString branch;
  QString upstream;
  bool hasUpstream = false;
  bool detachedHead = false;

  QString mergeBase;

  QList<GitCommitInfo> incoming;
  QList<GitCommitInfo> outgoing;

  int ahead() const { return outgoing.size(); }
  int behind() const { return incoming.size(); }

  bool inSync() const { return incoming.isEmpty() && outgoing.isEmpty(); }
  bool diverged() const { return !incoming.isEmpty() && !outgoing.isEmpty(); }

  bool pullCanFastForward() const {
    return !incoming.isEmpty() && outgoing.isEmpty();
  }

  bool pushIsFastForward() const { return incoming.isEmpty(); }
};

QString gitPullStrategyName(GitPullStrategy strategy);

QString gitPullStrategyPreview(GitPullStrategy strategy,
                               const GitSyncState &state);

QString gitPushPreview(const GitSyncState &state, GitPushForce force);

QString gitSyncSummary(const GitSyncState &state);

GitPullStrategy parsePullStrategyConfig(const QString &pullRebase,
                                        const QString &pullFf);

#endif
