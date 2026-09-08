#ifndef GITREPOSITORYSTATE_H
#define GITREPOSITORYSTATE_H

#include <QString>

enum class GitOperation {
  None,
  Merge,
  Rebase,
  CherryPick,
  Revert,
  Bisect,
  ApplyMailbox,
};

struct GitRepositoryState {
  bool valid = false;
  QString repositoryRoot;

  QString branch;
  bool detachedHead = false;
  bool unbornBranch = false;
  QString headShortHash;
  QString headSubject;

  QString upstream;
  bool hasUpstream = false;
  int ahead = 0;
  int behind = 0;

  int stagedCount = 0;
  int modifiedCount = 0;
  int untrackedCount = 0;
  int conflictedCount = 0;
  int stashCount = 0;

  GitOperation operation = GitOperation::None;

  QString operationDetail;

  int workingTreeCount() const { return modifiedCount + untrackedCount; }

  bool hasChanges() const {
    return stagedCount > 0 || modifiedCount > 0 || untrackedCount > 0 ||
           conflictedCount > 0;
  }

  bool isClean() const { return valid && !hasChanges(); }

  bool operator==(const GitRepositoryState &other) const;
  bool operator!=(const GitRepositoryState &other) const {
    return !(*this == other);
  }
};

void parseGitStatusPorcelainV2(const QString &output,
                               GitRepositoryState &state);

QString gitOperationName(GitOperation operation);

QString gitOperationExitHint(GitOperation operation);

QString gitRepositoryStateSummary(const GitRepositoryState &state);

QString gitRepositoryStateOneLine(const GitRepositoryState &state);

#endif
