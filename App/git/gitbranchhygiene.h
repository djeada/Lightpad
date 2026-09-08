#ifndef GITBRANCHHYGIENE_H
#define GITBRANCHHYGIENE_H

#include "gitcommittypes.h"
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

class GitIntegration;

enum class GitBranchState {
  Current,
  MergedIntoBase,
  HasUniqueCommits,
  UpstreamGone,
  LocalOnly,
  RemoteOnly,
  CheckedOutInWorktree,
  Pinned,
};

struct GitBranchHealth {
  QString name;
  bool isRemote = false;
  bool isCurrent = false;

  QString upstream;

  bool upstreamGone = false;
  int ahead = 0;
  int behind = 0;

  int uniqueCommits = 0;
  bool mergedIntoBase = false;

  QString lastActivity;
  QString tipHash;

  QString worktreePath;
  bool pinned = false;

  QList<GitBranchState> states;

  bool hasState(GitBranchState state) const { return states.contains(state); }

  bool safeToDelete() const;

  QString deleteWarning() const;
};

struct GitBranchHygieneReport {
  bool valid = false;

  QString baseRef;
  QList<GitBranchHealth> branches;

  QList<GitBranchHealth> cleanupCandidates() const;
};

QString gitBranchStateName(GitBranchState state);
QString gitBranchStateExplanation(GitBranchState state);

QList<GitBranchHealth> parseBranchRefs(const QString &output);

void parseUpstreamTrack(const QString &track, int *ahead, int *behind,
                        bool *gone);

GitBranchHygieneReport buildBranchHygieneReport(GitIntegration *git,
                                                const QString &baseRef,
                                                const QSet<QString> &pinned);

#endif
