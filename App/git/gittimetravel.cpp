#include "gittimetravel.h"
#include "gitintegration.h"
#include <QObject>

QString gitTimeTravelModeName(GitTimeTravelMode mode) {
  switch (mode) {
  case GitTimeTravelMode::InspectSnapshot:
    return QObject::tr("Inspect snapshot");
  case GitTimeTravelMode::RunnableWorktree:
    return QObject::tr("Open runnable snapshot");
  case GitTimeTravelMode::StartBranch:
    return QObject::tr("Start work from here");
  }
  return QString();
}

QList<GitTimeTravelOption> gitTimeTravelOptions(const QString &commitHash,
                                                const QString &shortHash) {
  const QString label = shortHash.isEmpty() ? commitHash.left(7) : shortHash;
  QList<GitTimeTravelOption> options;

  GitTimeTravelOption inspect;
  inspect.mode = GitTimeTravelMode::InspectSnapshot;
  inspect.title = QObject::tr("Just look at the files as they were");
  inspect.explanation =
      QObject::tr("Opens the files from %1 read-only. Your checkout does not "
                  "move, nothing on disk changes, and there is nothing to undo "
                  "afterwards.")
          .arg(label);
  inspect.command = QStringLiteral("git show %1:<file>").arg(label);
  inspect.changesCheckout = false;
  options.append(inspect);

  GitTimeTravelOption worktree;
  worktree.mode = GitTimeTravelMode::RunnableWorktree;
  worktree.title = QObject::tr("Run or debug that version");
  worktree.explanation =
      QObject::tr("Creates a second working directory checked out at %1. You "
                  "can build and run it there while your current checkout "
                  "stays exactly as it is — this is what worktrees are for.")
          .arg(label);
  worktree.command =
      QStringLiteral("git worktree add --detach <path> %1").arg(label);
  worktree.changesCheckout = false;
  options.append(worktree);

  GitTimeTravelOption branch;
  branch.mode = GitTimeTravelMode::StartBranch;
  branch.title = QObject::tr("Start new work from that point");
  branch.explanation =
      QObject::tr("Creates a named branch at %1 and switches to it. Because it "
                  "has a name, anything you commit is easy to find again — "
                  "which is exactly what a bare checkout of a commit does not "
                  "give you.")
          .arg(label);
  branch.command = QStringLiteral("git switch -c <name> %1").arg(label);
  branch.changesCheckout = true;
  options.append(branch);

  return options;
}

GitDetachedHeadState gitDetachedHeadState(GitIntegration *git, int maxCommits) {
  GitDetachedHeadState state;
  if (!git || !git->isValidRepository()) {
    return state;
  }

  state.valid = true;
  const GitRepositoryState repo = git->repositoryState();
  state.detached = repo.detachedHead;
  state.headShortHash = repo.headShortHash;
  state.headSubject = repo.headSubject;
  state.headHash = git->getCommitDetails(QStringLiteral("HEAD")).hash;

  if (state.detached) {
    state.unreferencedCommits = git->getCommitsWithoutRef(maxCommits);
  }

  return state;
}

QString gitDetachedHeadExplanation(const GitDetachedHeadState &state) {
  if (!state.valid || !state.detached) {
    return QString();
  }

  QString text =
      QObject::tr("HEAD is on the commit %1 directly, not on a branch. Commits "
                  "you make here are real commits, but nothing names them.")
          .arg(state.headShortHash.isEmpty() ? QObject::tr("you checked out")
                                             : state.headShortHash);

  if (state.hasUnreferencedWork()) {
    text += state.unreferencedCommits.size() == 1
                ? QObject::tr(" You already have 1 such commit.")
                : QObject::tr(" You already have %1 such commits.")
                      .arg(state.unreferencedCommits.size());
  }
  return text;
}

QString gitDetachedHeadLeaveWarning(const GitDetachedHeadState &state) {
  if (!state.valid || !state.detached || !state.hasUnreferencedWork()) {
    return QString();
  }

  QStringList subjects;
  for (const GitCommitInfo &commit : state.unreferencedCommits) {
    subjects << QStringLiteral("%1 %2").arg(commit.shortHash, commit.subject);
  }

  return QObject::tr("Leaving this commit would leave %1 commit(s) with no "
                     "branch or tag pointing at them:\n%2\n\nThey stay in the "
                     "reflog for a while, but the reliable fix is to give them "
                     "a name first.")
      .arg(state.unreferencedCommits.size())
      .arg(subjects.join(QStringLiteral("\n")));
}
