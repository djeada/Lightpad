#include "gitundoplan.h"
#include "gitintegration.h"
#include <QObject>

namespace {

const QString kUnchanged = QStringLiteral("unchanged");

}

QString gitUndoGoalName(GitUndoGoal goal) {
  switch (goal) {
  case GitUndoGoal::DiscardWorkingEdits:
    return QObject::tr("Discard working edits");
  case GitUndoGoal::UnstageKeepEdits:
    return QObject::tr("Unstage but keep edits");
  case GitUndoGoal::MoveBranchKeepStaged:
    return QObject::tr("Move branch back, keep changes staged");
  case GitUndoGoal::MoveBranchKeepUnstaged:
    return QObject::tr("Move branch back, keep changes unstaged");
  case GitUndoGoal::ResetEverything:
    return QObject::tr("Reset everything to an earlier commit");
  case GitUndoGoal::RevertPublishedCommit:
    return QObject::tr("Undo with a new inverse commit");
  case GitUndoGoal::AmendLastCommit:
    return QObject::tr("Change the most recent commit");
  }
  return QString();
}

GitUndoPlan buildGitUndoPlan(GitIntegration *git, const QString &targetCommit,
                             const QStringList &selectedPaths) {
  GitUndoPlan plan;
  if (!git || !git->isValidRepository()) {
    plan.error = QObject::tr("No Git repository.");
    return plan;
  }

  plan.valid = true;
  plan.selectedPaths = selectedPaths;

  const GitRepositoryState state = git->repositoryState();
  const GitSyncState sync = git->syncState();

  const QString commit =
      targetCommit.isEmpty() ? QStringLiteral("HEAD") : targetCommit;
  plan.targetCommit = commit;

  if (sync.hasUpstream) {
    const QString resolved = git->getCommitDetails(commit).hash;
    bool localOnly = false;
    for (const GitCommitInfo &outgoing : sync.outgoing) {
      if (!resolved.isEmpty() && outgoing.hash == resolved) {
        localOnly = true;
        break;
      }
    }
    plan.targetIsPublished = !localOnly;
  }

  QStringList dirtyPaths;
  QStringList stagedPaths;
  for (const GitFileInfo &info : git->getStatus()) {
    if (info.workTreeStatus != GitFileStatus::Clean) {
      dirtyPaths << info.filePath;
    }
    if (info.indexStatus != GitFileStatus::Clean &&
        info.indexStatus != GitFileStatus::Untracked) {
      stagedPaths << info.filePath;
    }
  }
  const QStringList discardTargets =
      selectedPaths.isEmpty() ? dirtyPaths : selectedPaths;

  GitUndoOption discard;
  discard.goal = GitUndoGoal::DiscardWorkingEdits;
  discard.question =
      selectedPaths.isEmpty()
          ? QObject::tr("Throw away my uncommitted edits")
          : QObject::tr(
                "Throw away my uncommitted edits in %1 selected file(s)")
                .arg(selectedPaths.size());
  discard.command =
      selectedPaths.isEmpty()
          ? QStringLiteral("git restore .")
          : QStringLiteral("git restore -- %1").arg(selectedPaths.join(' '));
  discard.explanation = QObject::tr(
      "Rewrites the files on disk from the index. These edits exist nowhere "
      "else, so Git cannot bring them back — this is the one undo with no "
      "safety net.");
  discard.matrix = {QObject::tr("reset to the index"), kUnchanged, kUnchanged,
                    kUnchanged};
  discard.destructive = true;
  discard.atRiskPaths = discardTargets;
  if (discardTargets.isEmpty()) {
    discard.unavailableReason =
        QObject::tr("There are no uncommitted edits to discard.");
  }
  plan.options.append(discard);

  GitUndoOption unstage;
  unstage.goal = GitUndoGoal::UnstageKeepEdits;
  unstage.question = QObject::tr("Take things back out of the staging area");
  unstage.command = selectedPaths.isEmpty()
                        ? QStringLiteral("git restore --staged .")
                        : QStringLiteral("git restore --staged -- %1")
                              .arg(selectedPaths.join(' '));
  unstage.explanation = QObject::tr(
      "Clears the index back to HEAD and leaves every edit on disk exactly as "
      "it is. Unstaging and discarding are different operations; this is the "
      "one that keeps your work.");
  unstage.matrix = {kUnchanged, QObject::tr("reset to HEAD"), kUnchanged,
                    kUnchanged};
  if (stagedPaths.isEmpty()) {
    unstage.unavailableReason = QObject::tr("Nothing is staged.");
  }
  plan.options.append(unstage);

  const QString parent = QStringLiteral("%1~1").arg(commit);

  GitUndoOption softReset;
  softReset.goal = GitUndoGoal::MoveBranchKeepStaged;
  softReset.question =
      QObject::tr("Undo the commit but keep its changes staged");
  softReset.command = QStringLiteral("git reset --soft %1").arg(parent);
  softReset.explanation = QObject::tr(
      "Moves the branch back one commit. Everything that commit contained is "
      "still staged, ready to be committed again — this is how you redo a "
      "commit message or split a commit.");
  softReset.matrix = {kUnchanged, QObject::tr("keeps the commit's content"),
                      QObject::tr("moves back"), QObject::tr("moves back")};
  softReset.hasOperationPreview = true;
  softReset.previewKind = GitOperationKind::Reset;
  softReset.previewTarget = parent;
  softReset.resetMode = QStringLiteral("soft");
  plan.options.append(softReset);

  GitUndoOption mixedReset;
  mixedReset.goal = GitUndoGoal::MoveBranchKeepUnstaged;
  mixedReset.question =
      QObject::tr("Undo the commit and unstage its changes too");
  mixedReset.command = QStringLiteral("git reset --mixed %1").arg(parent);
  mixedReset.explanation = QObject::tr(
      "Moves the branch back and clears the index, but leaves every file on "
      "disk untouched. Nothing is lost; you decide again what to stage.");
  mixedReset.matrix = {kUnchanged, QObject::tr("cleared to HEAD"),
                       QObject::tr("moves back"), QObject::tr("moves back")};
  mixedReset.hasOperationPreview = true;
  mixedReset.previewKind = GitOperationKind::Reset;
  mixedReset.previewTarget = parent;
  mixedReset.resetMode = QStringLiteral("mixed");
  plan.options.append(mixedReset);

  GitUndoOption hardReset;
  hardReset.goal = GitUndoGoal::ResetEverything;
  hardReset.question =
      QObject::tr("Put everything back the way it was at an earlier commit");
  hardReset.command = QStringLiteral("git reset --hard %1").arg(commit);
  hardReset.explanation = QObject::tr(
      "Moves the branch and rewrites the index and your files to match that "
      "commit. Commits it drops stay in the reflog for a while; uncommitted "
      "edits do not exist anywhere else.");
  hardReset.matrix = {QObject::tr("rewritten"), QObject::tr("rewritten"),
                      QObject::tr("moves"), QObject::tr("moves")};
  hardReset.destructive = true;
  hardReset.atRiskPaths = dirtyPaths;
  hardReset.hasOperationPreview = true;
  hardReset.previewKind = GitOperationKind::Reset;
  hardReset.previewTarget = commit;
  hardReset.resetMode = QStringLiteral("hard");
  plan.options.append(hardReset);

  GitUndoOption revert;
  revert.goal = GitUndoGoal::RevertPublishedCommit;
  revert.question = QObject::tr("Undo it without rewriting history");
  revert.command = QStringLiteral("git revert %1").arg(commit);
  revert.explanation = QObject::tr(
      "Adds a new commit that applies the opposite change. The original "
      "commit stays exactly where it is, so anyone who already has it is "
      "unaffected — this is the collaboration-safe undo.");
  revert.matrix = {QObject::tr("gets the inverse change"),
                   QObject::tr("gets the inverse change"),
                   QObject::tr("gains a commit"), QObject::tr("moves forward")};
  revert.hasOperationPreview = true;
  revert.previewKind = GitOperationKind::Revert;
  revert.previewTarget = commit;
  if (state.unbornBranch) {
    revert.unavailableReason = QObject::tr("There are no commits yet.");
  }
  plan.options.append(revert);

  GitUndoOption amend;
  amend.goal = GitUndoGoal::AmendLastCommit;
  amend.question =
      QObject::tr("Fix the most recent commit instead of adding another");
  amend.command = QStringLiteral("git commit --amend");
  amend.explanation = QObject::tr(
      "Replaces the last commit with a new one containing the staged changes "
      "and, if you want, a new message. It is a rewrite: the commit gets a "
      "new hash.");
  amend.matrix = {kUnchanged, QObject::tr("folded into the commit"),
                  QObject::tr("replaced"), QObject::tr("moves to the new one")};
  if (state.unbornBranch) {
    amend.unavailableReason = QObject::tr("There are no commits yet.");
  }
  plan.options.append(amend);

  if (!targetCommit.isEmpty()) {
    if (plan.targetIsPublished) {
      plan.recommended = GitUndoGoal::RevertPublishedCommit;
      plan.recommendationReason =
          QObject::tr(
              "This commit is already on %1. Rewriting it would force everyone "
              "else to reconcile, so the safe undo is a new inverse commit.")
              .arg(sync.upstream);
    } else {
      plan.recommended = GitUndoGoal::MoveBranchKeepStaged;
      plan.recommendationReason = QObject::tr(
          "This commit exists only here, so it can be undone without anyone "
          "else noticing. Keeping its content staged loses nothing.");
    }
  } else if (!stagedPaths.isEmpty()) {
    plan.recommended = GitUndoGoal::UnstageKeepEdits;
    plan.recommendationReason = QObject::tr(
        "You have staged changes and no commit selected, so the likely goal "
        "is to unstage without losing the edits.");
  } else if (!dirtyPaths.isEmpty()) {
    plan.recommended = GitUndoGoal::DiscardWorkingEdits;
    plan.recommendationReason = QObject::tr(
        "Only uncommitted edits exist. This is the one undo Git cannot take "
        "back, so check the file list before running it.");
  } else {
    plan.recommended = GitUndoGoal::MoveBranchKeepStaged;
    plan.recommendationReason = QObject::tr(
        "Nothing is uncommitted, so undoing means moving the branch off its "
        "last commit. Keeping the content staged loses nothing.");
  }

  return plan;
}
