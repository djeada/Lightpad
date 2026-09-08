#include "gitbranchhygiene.h"
#include "gitintegration.h"
#include <QObject>
#include <QRegularExpression>

bool GitBranchHealth::safeToDelete() const {
  return !isCurrent && worktreePath.isEmpty() && !pinned && mergedIntoBase;
}

QString GitBranchHealth::deleteWarning() const {
  if (isCurrent) {
    return QObject::tr("This is the branch you are on; switch away first.");
  }
  if (!worktreePath.isEmpty()) {
    return QObject::tr("Checked out in the worktree at %1, so Git will refuse "
                       "to delete it.")
        .arg(worktreePath);
  }
  if (pinned) {
    return QObject::tr("Pinned, so it is kept out of cleanup suggestions.");
  }
  if (!mergedIntoBase) {
    return uniqueCommits > 0
               ? QObject::tr("%1 commits are only on this branch. Deleting it "
                             "leaves them with no name — they survive in the "
                             "reflog for a while, and no longer.")
                     .arg(uniqueCommits)
               : QObject::tr("Not merged into the chosen base, so Git will ask "
                             "for confirmation.");
  }
  return QString();
}

QList<GitBranchHealth> GitBranchHygieneReport::cleanupCandidates() const {
  QList<GitBranchHealth> candidates;
  for (const GitBranchHealth &branch : branches) {

    if (!branch.isRemote && branch.safeToDelete()) {
      candidates.append(branch);
    }
  }
  return candidates;
}

QString gitBranchStateName(GitBranchState state) {
  switch (state) {
  case GitBranchState::Current:
    return QObject::tr("current");
  case GitBranchState::MergedIntoBase:
    return QObject::tr("merged");
  case GitBranchState::HasUniqueCommits:
    return QObject::tr("has unique commits");
  case GitBranchState::UpstreamGone:
    return QObject::tr("upstream gone");
  case GitBranchState::LocalOnly:
    return QObject::tr("local only");
  case GitBranchState::RemoteOnly:
    return QObject::tr("remote only");
  case GitBranchState::CheckedOutInWorktree:
    return QObject::tr("in a worktree");
  case GitBranchState::Pinned:
    return QObject::tr("pinned");
  }
  return QString();
}

QString gitBranchStateExplanation(GitBranchState state) {
  switch (state) {
  case GitBranchState::Current:
    return QObject::tr("The branch HEAD is on right now.");
  case GitBranchState::MergedIntoBase:
    return QObject::tr(
        "Every commit on it is reachable from the base you chose, so deleting "
        "it removes only a name.");
  case GitBranchState::HasUniqueCommits:
    return QObject::tr(
        "It holds commits the base cannot reach. Deleting it leaves those "
        "commits unnamed.");
  case GitBranchState::UpstreamGone:
    return QObject::tr(
        "It tracks a remote branch that no longer exists — usually because "
        "the pull request was merged and the remote branch was deleted.");
  case GitBranchState::LocalOnly:
    return QObject::tr("It exists here and nowhere else.");
  case GitBranchState::RemoteOnly:
    return QObject::tr(
        "A remote-tracking ref. It is a local copy of what the remote had at "
        "the last fetch, not a branch you can commit to.");
  case GitBranchState::CheckedOutInWorktree:
    return QObject::tr(
        "Another worktree has it checked out. Git will not let a branch be "
        "checked out twice, and will not delete it while it is.");
  case GitBranchState::Pinned:
    return QObject::tr("You marked it as never to suggest for cleanup.");
  }
  return QString();
}

void parseUpstreamTrack(const QString &track, int *ahead, int *behind,
                        bool *gone) {
  *ahead = 0;
  *behind = 0;
  *gone = false;

  if (track.contains(QLatin1String("gone"))) {
    *gone = true;
    return;
  }

  static const QRegularExpression aheadPattern(QStringLiteral("ahead (\\d+)"));
  static const QRegularExpression behindPattern(
      QStringLiteral("behind (\\d+)"));

  const QRegularExpressionMatch aheadMatch = aheadPattern.match(track);
  if (aheadMatch.hasMatch()) {
    *ahead = aheadMatch.captured(1).toInt();
  }
  const QRegularExpressionMatch behindMatch = behindPattern.match(track);
  if (behindMatch.hasMatch()) {
    *behind = behindMatch.captured(1).toInt();
  }
}

QList<GitBranchHealth> parseBranchRefs(const QString &output) {
  QList<GitBranchHealth> branches;

  for (const QString &line :
       output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
    const QStringList fields = line.split(QChar(0));
    if (fields.size() < 6) {
      continue;
    }

    GitBranchHealth branch;
    const QString refname = fields.at(0);
    branch.isRemote = refname.startsWith(QLatin1String("refs/remotes/"));
    branch.name = branch.isRemote ? refname.mid(13) : refname.mid(11);
    branch.tipHash = fields.at(1);
    branch.lastActivity = fields.at(2);
    branch.upstream = fields.at(3);
    parseUpstreamTrack(fields.at(4), &branch.ahead, &branch.behind,
                       &branch.upstreamGone);
    branch.isCurrent = fields.at(5) == QLatin1String("*");

    if (branch.name.endsWith(QLatin1String("/HEAD"))) {
      continue;
    }
    branches.append(branch);
  }

  return branches;
}

GitBranchHygieneReport buildBranchHygieneReport(GitIntegration *git,
                                                const QString &baseRef,
                                                const QSet<QString> &pinned) {
  GitBranchHygieneReport report;
  if (!git || !git->isValidRepository()) {
    return report;
  }

  report.valid = true;
  report.baseRef = baseRef;
  report.branches = parseBranchRefs(git->branchRefDetails());

  const QSet<QString> merged = git->mergedBranchNames(baseRef);
  const QMap<QString, QString> worktrees = git->branchWorktreePaths();

  for (GitBranchHealth &branch : report.branches) {
    branch.pinned = pinned.contains(branch.name);
    branch.mergedIntoBase = merged.contains(branch.name);
    branch.worktreePath = worktrees.value(branch.name);

    if (!branch.mergedIntoBase) {
      branch.uniqueCommits = git->countCommitsNotIn(branch.name, baseRef);
    }

    if (branch.isCurrent) {
      branch.states.append(GitBranchState::Current);
    }
    if (branch.mergedIntoBase) {
      branch.states.append(GitBranchState::MergedIntoBase);
    } else if (branch.uniqueCommits > 0) {
      branch.states.append(GitBranchState::HasUniqueCommits);
    }
    if (branch.upstreamGone) {
      branch.states.append(GitBranchState::UpstreamGone);
    }
    if (branch.isRemote) {
      branch.states.append(GitBranchState::RemoteOnly);
    } else if (branch.upstream.isEmpty()) {
      branch.states.append(GitBranchState::LocalOnly);
    }
    if (!branch.worktreePath.isEmpty()) {
      branch.states.append(GitBranchState::CheckedOutInWorktree);
    }
    if (branch.pinned) {
      branch.states.append(GitBranchState::Pinned);
    }
  }

  return report;
}
