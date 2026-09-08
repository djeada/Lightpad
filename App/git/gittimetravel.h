#ifndef GITTIMETRAVEL_H
#define GITTIMETRAVEL_H

#include "gitcommittypes.h"
#include <QList>
#include <QString>

class GitIntegration;

enum class GitTimeTravelMode {
  InspectSnapshot,
  RunnableWorktree,
  StartBranch,
};

struct GitTimeTravelOption {
  GitTimeTravelMode mode = GitTimeTravelMode::InspectSnapshot;
  QString title;
  QString explanation;
  QString command;

  bool changesCheckout = false;
};

struct GitDetachedHeadState {
  bool valid = false;
  bool detached = false;
  QString headHash;
  QString headShortHash;
  QString headSubject;

  QList<GitCommitInfo> unreferencedCommits;

  bool hasUnreferencedWork() const { return !unreferencedCommits.isEmpty(); }
};

QString gitTimeTravelModeName(GitTimeTravelMode mode);

QList<GitTimeTravelOption> gitTimeTravelOptions(const QString &commitHash,
                                                const QString &shortHash);

GitDetachedHeadState gitDetachedHeadState(GitIntegration *git,
                                          int maxCommits = 100);

QString gitDetachedHeadExplanation(const GitDetachedHeadState &state);

QString gitDetachedHeadLeaveWarning(const GitDetachedHeadState &state);

#endif
