#include "gitintegrationadvice.h"
#include "gitintegration.h"
#include <QObject>

namespace {

QString commitCount(int count) {
  return count == 1 ? QObject::tr("1 commit")
                    : QObject::tr("%1 commits").arg(count);
}

} // namespace

QString gitIntegrationIntentName(GitIntegrationIntent intent) {
  switch (intent) {
  case GitIntegrationIntent::BringEverythingIn:
    return QObject::tr("Merge");
  case GitIntegrationIntent::ReplayMineOnTop:
    return QObject::tr("Rebase");
  case GitIntegrationIntent::BringOneCommit:
    return QObject::tr("Cherry-pick");
  case GitIntegrationIntent::ApplySelectedChanges:
    return QObject::tr("Apply without committing");
  }
  return QString();
}

GitIntegrationAdvice adviseGitIntegration(GitIntegration *git,
                                          const QString &sourceRef,
                                          const QString &singleCommit) {
  GitIntegrationAdvice advice;

  if (!git || !git->isValidRepository()) {
    advice.error = QObject::tr("No Git repository.");
    return advice;
  }
  if (sourceRef.isEmpty()) {
    advice.error = QObject::tr("Pick where the changes should come from.");
    return advice;
  }

  const GitRepositoryState state = git->repositoryState();
  if (state.branch.isEmpty()) {
    advice.error = QObject::tr(
        "HEAD is detached, so there is no branch to bring changes into.");
    return advice;
  }

  advice.valid = true;
  advice.sourceRef = sourceRef;
  advice.targetBranch = state.branch;
  advice.singleCommit = singleCommit;

  const QList<GitCommitInfo> incoming =
      git->getCommitLogPage(QStringLiteral("HEAD..%1").arg(sourceRef), 0, 200);
  const QList<GitCommitInfo> mine =
      git->getCommitLogPage(QStringLiteral("%1..HEAD").arg(sourceRef), 0, 200);

  const bool minePublished =
      state.hasUpstream && state.ahead == 0 && !mine.isEmpty();

  GitIntegrationOption merge;
  merge.intent = GitIntegrationIntent::BringEverythingIn;
  merge.question =
      QObject::tr("Bring all of %1 into %2").arg(sourceRef, state.branch);
  merge.commandName = QStringLiteral("git merge %1").arg(sourceRef);
  merge.operationKind = GitOperationKind::Merge;
  merge.consequence =
      mine.isEmpty()
          ? QObject::tr("Your branch has nothing of its own, so this just "
                        "moves it forward onto the %1 waiting for it. No "
                        "merge commit, no rewriting.")
                .arg(commitCount(incoming.size()))
          : QObject::tr("Adds one merge commit joining your %1 to the %2 "
                        "coming in. Both histories are kept exactly as they "
                        "are.")
                .arg(commitCount(mine.size()), commitCount(incoming.size()));
  if (incoming.isEmpty()) {
    merge.unavailableReason =
        QObject::tr("%1 has nothing you do not already have.").arg(sourceRef);
  }
  advice.options.append(merge);

  GitIntegrationOption rebase;
  rebase.intent = GitIntegrationIntent::ReplayMineOnTop;
  rebase.question = QObject::tr("Put my %1 on top of the latest %2")
                        .arg(commitCount(mine.size()), sourceRef);
  rebase.commandName = QStringLiteral("git rebase %1").arg(sourceRef);
  rebase.operationKind = GitOperationKind::Rebase;
  rebase.rewritesHistory = !mine.isEmpty();
  rebase.affectsSharedHistory = minePublished;
  rebase.consequence =
      mine.isEmpty()
          ? QObject::tr("You have no commits to replay, so this would be the "
                        "same as a fast-forward.")
          : QObject::tr("Recreates each of your %1 on top of %2. They get "
                        "new hashes, so they are different commits from the "
                        "ones you have now.")
                .arg(commitCount(mine.size()), sourceRef);
  if (mine.isEmpty()) {
    rebase.unavailableReason =
        QObject::tr("You have no commits of your own to replay.");
  }
  advice.options.append(rebase);

  GitIntegrationOption pick;
  pick.intent = GitIntegrationIntent::BringOneCommit;
  pick.question =
      QObject::tr("Bring only one commit from %1 over").arg(sourceRef);
  pick.commandName =
      QStringLiteral("git cherry-pick %1")
          .arg(singleCommit.isEmpty() ? QStringLiteral("<commit>")
                                      : singleCommit.left(7));
  pick.operationKind = GitOperationKind::CherryPick;
  pick.consequence = QObject::tr(
      "Copies that one change into a new commit here. The original stays "
      "where it is, and the copy has a different hash — this is copying a "
      "change, not moving history.");
  if (singleCommit.isEmpty()) {
    pick.unavailableReason = QObject::tr("Choose which commit to copy.");
  }
  advice.options.append(pick);

  GitIntegrationOption apply;
  apply.intent = GitIntegrationIntent::ApplySelectedChanges;
  apply.question = QObject::tr("Apply only some files or hunks from a commit");
  apply.commandName =
      QStringLiteral("git cherry-pick -n %1")
          .arg(singleCommit.isEmpty() ? QStringLiteral("<commit>")
                                      : singleCommit.left(7));
  apply.createsCommit = false;
  apply.hasOperationPreview = false;
  apply.consequence = QObject::tr(
      "Puts the change into your working tree and index without committing, "
      "so you can keep only the parts you want. No commit history is copied "
      "at all — this is a file-level operation, not a history one.");
  if (singleCommit.isEmpty()) {
    apply.unavailableReason = QObject::tr("Choose which commit to take from.");
  }
  advice.options.append(apply);

  if (!incoming.isEmpty()) {
    advice.recommended = GitIntegrationIntent::BringEverythingIn;
    advice.recommendationReason =
        mine.isEmpty()
            ? QObject::tr("Nothing of yours is at stake, so merging is a plain "
                          "fast-forward.")
            : QObject::tr("Merging keeps every commit exactly as it is. Rebase "
                          "would give a straighter history, at the cost of "
                          "recreating your %1.")
                  .arg(commitCount(mine.size()));
  } else if (!singleCommit.isEmpty()) {
    advice.recommended = GitIntegrationIntent::BringOneCommit;
    advice.recommendationReason =
        QObject::tr("There is nothing new on %1 as a whole, so copying the one "
                    "commit you "
                    "care about is the smaller change.")
            .arg(sourceRef);
  } else {
    advice.recommended = GitIntegrationIntent::BringEverythingIn;
    advice.recommendationReason =
        QObject::tr("Nothing to bring over: %1 has no commits you lack.")
            .arg(sourceRef);
  }

  return advice;
}
