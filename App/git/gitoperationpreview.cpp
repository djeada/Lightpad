#include "gitoperationpreview.h"
#include "gitintegration.h"
#include <QObject>

namespace {

PreviewNode toNode(const GitCommitInfo &commit, PreviewNode::State state,
                   int lane = 0, bool isHead = false) {
  PreviewNode node;
  node.shortHash =
      commit.shortHash.isEmpty() ? commit.hash.left(7) : commit.shortHash;
  node.subject = commit.subject;
  node.state = state;
  node.lane = lane;
  node.isHead = isHead;
  return node;
}

QList<PreviewNode> toNodes(const QList<GitCommitInfo> &commits,
                           PreviewNode::State state, int lane = 0) {
  QList<PreviewNode> nodes;
  for (const GitCommitInfo &commit : commits) {
    nodes.append(toNode(commit, state, lane));
  }
  return nodes;
}

QStringList dirtyPaths(GitIntegration *git) {
  QStringList paths;
  for (const GitFileInfo &info : git->getStatus()) {
    if (info.workTreeStatus != GitFileStatus::Clean ||
        info.indexStatus != GitFileStatus::Clean) {
      paths << info.filePath;
    }
  }
  return paths;
}

} // namespace

QString gitOperationKindName(GitOperationKind kind) {
  switch (kind) {
  case GitOperationKind::Merge:
    return QObject::tr("Merge");
  case GitOperationKind::Rebase:
    return QObject::tr("Rebase");
  case GitOperationKind::Reset:
    return QObject::tr("Reset");
  case GitOperationKind::Revert:
    return QObject::tr("Revert");
  case GitOperationKind::CherryPick:
    return QObject::tr("Cherry-pick");
  case GitOperationKind::Pull:
    return QObject::tr("Pull");
  case GitOperationKind::BranchDelete:
    return QObject::tr("Delete branch");
  case GitOperationKind::StashApply:
    return QObject::tr("Apply stash");
  case GitOperationKind::StashPop:
    return QObject::tr("Pop stash");
  }
  return QString();
}

QString gitOperationRiskName(GitOperationRisk risk) {
  switch (risk) {
  case GitOperationRisk::Safe:
    return QObject::tr("Safe");
  case GitOperationRisk::RewritesLocalHistory:
    return QObject::tr("Rewrites local history");
  case GitOperationRisk::MayDiscardUncommitted:
    return QObject::tr("May discard uncommitted work");
  case GitOperationRisk::AffectsSharedHistory:
    return QObject::tr("Affects shared history");
  }
  return QString();
}

QString gitOperationHeadline(const GitOperationPreview &preview) {
  const GitOperationEffect &effect = preview.effect;
  if (!effect.valid) {
    return effect.error;
  }

  QStringList parts;
  if (!effect.commitsAdded.isEmpty()) {
    parts << (effect.commitsAdded.size() == 1
                  ? QObject::tr("1 new commit")
                  : QObject::tr("%1 new commits")
                        .arg(effect.commitsAdded.size()));
  }
  if (!effect.commitsRewritten.isEmpty()) {
    parts << QObject::tr("%1 commits recreated with new hashes")
                 .arg(effect.commitsRewritten.size());
  }
  if (!effect.commitsUnreachable.isEmpty()) {
    parts << QObject::tr("%1 commits leave the branch")
                 .arg(effect.commitsUnreachable.size());
  }
  if (effect.changesIndex) {
    parts << QObject::tr("the index changes");
  }
  if (effect.changesWorkingTree) {
    parts << QObject::tr("your files change");
  }
  if (parts.isEmpty()) {
    parts << QObject::tr("nothing changes");
  }

  return QObject::tr("%1: %2.").arg(gitOperationKindName(preview.request.kind),
                                    parts.join(", "));
}

QStringList parseMergeTreeConflicts(const QString &output) {

  const QStringList lines = output.split(QLatin1Char('\n'));
  QStringList conflicts;
  for (int i = 1; i < lines.size(); ++i) {
    const QString line = lines.at(i);
    if (line.trimmed().isEmpty()) {
      break;
    }
    conflicts << line;
  }
  return conflicts;
}

GitOperationPreview previewGitOperation(GitIntegration *git,
                                        const GitOperationRequest &request,
                                        int contextCommits) {
  GitOperationPreview preview;
  preview.request = request;

  if (!git || !git->isValidRepository()) {
    preview.effect.error = QObject::tr("No Git repository.");
    return preview;
  }

  GitOperationEffect &effect = preview.effect;
  effect.valid = true;

  const GitRepositoryState state = git->repositoryState();
  const QString head = QStringLiteral("HEAD");
  const QList<GitCommitInfo> headCommits =
      git->getCommitLogPage(head, 0, contextCommits);
  preview.before = toNodes(headCommits, PreviewNode::State::Unchanged);
  if (!preview.before.isEmpty()) {
    preview.before.first().isHead = true;
  }

  const QString target = request.target;

  switch (request.kind) {
  case GitOperationKind::Merge:
  case GitOperationKind::Pull: {
    const QString source =
        request.kind == GitOperationKind::Pull && target.isEmpty()
            ? state.upstream
            : target;
    if (source.isEmpty()) {
      effect.valid = false;
      effect.error = QObject::tr("Nothing to merge from.");
      return preview;
    }

    const QList<GitCommitInfo> incoming = git->getCommitLogPage(
        QStringLiteral("HEAD..%1").arg(source), 0, contextCommits);
    const QList<GitCommitInfo> outgoing = git->getCommitLogPage(
        QStringLiteral("%1..HEAD").arg(source), 0, contextCommits);

    const bool rebasing = request.kind == GitOperationKind::Pull &&
                          request.pullStrategy == GitPullStrategy::Rebase;
    const bool fastForward = outgoing.isEmpty();

    effect.movesBranch = true;
    effect.changesWorkingTree = !incoming.isEmpty();
    effect.conflictsKnown = true;
    effect.conflictPaths = git->predictMergeConflicts(head, source);

    if (rebasing) {
      effect.commitsRewritten = outgoing;
      effect.risk = outgoing.isEmpty() ? GitOperationRisk::Safe
                                       : GitOperationRisk::RewritesLocalHistory;
      effect.rationale =
          outgoing.isEmpty()
              ? QObject::tr("Nothing of yours to replay, so this is just a "
                            "fast-forward.")
              : QObject::tr("Your %1 commits are recreated on top of the "
                            "incoming ones, so they get new hashes.")
                    .arg(outgoing.size());
    } else if (fastForward) {
      effect.risk = GitOperationRisk::Safe;
      effect.rationale = QObject::tr(
          "Your branch has no commits of its own, so this only moves the "
          "branch pointer forward. No history is rewritten.");
    } else {
      GitCommitInfo mergeCommit;
      mergeCommit.shortHash = QObject::tr("new");
      mergeCommit.subject =
          QObject::tr("Merge %1 into %2").arg(source, state.branch);
      effect.commitsAdded << mergeCommit;
      effect.risk = GitOperationRisk::Safe;
      effect.rationale = QObject::tr(
          "A merge commit is added joining both histories. Nothing existing "
          "is rewritten, so both sides stay exactly as they are.");
    }

    if (!effect.conflictPaths.isEmpty()) {
      effect.rationale +=
          effect.conflictPaths.size() == 1
              ? QObject::tr(" Git predicts a conflict in 1 file; you will be "
                            "asked to resolve it.")
              : QObject::tr(" Git predicts conflicts in %1 files; you will be "
                            "asked to resolve them.")
                    .arg(effect.conflictPaths.size());
    }

    preview.after = toNodes(incoming, PreviewNode::State::Unchanged, 1);
    if (rebasing) {
      preview.after =
          toNodes(outgoing, PreviewNode::State::Rewritten) + preview.after;
    } else {
      if (!effect.commitsAdded.isEmpty()) {
        preview.after.prepend(toNode(effect.commitsAdded.first(),
                                     PreviewNode::State::Added, 0, true));
      }
      preview.after += toNodes(outgoing, PreviewNode::State::Unchanged);
    }
    if (!preview.after.isEmpty()) {
      preview.after.first().isHead = true;
    }

    effect.commands << (request.kind == GitOperationKind::Pull
                            ? QStringLiteral("git pull %1")
                                  .arg(rebasing ? QStringLiteral("--rebase")
                                                : QStringLiteral("--no-rebase"))
                            : QStringLiteral("git merge %1").arg(source));
    break;
  }

  case GitOperationKind::Rebase: {
    if (target.isEmpty()) {
      effect.valid = false;
      effect.error = QObject::tr("No branch to rebase onto.");
      return preview;
    }

    const QList<GitCommitInfo> replayed = git->getCommitLogPage(
        QStringLiteral("%1..HEAD").arg(target), 0, contextCommits);
    const QList<GitCommitInfo> onto = git->getCommitLogPage(
        QStringLiteral("HEAD..%1").arg(target), 0, contextCommits);

    effect.commitsRewritten = replayed;
    effect.movesBranch = true;
    effect.changesWorkingTree = true;
    effect.risk = replayed.isEmpty() ? GitOperationRisk::Safe
                                     : GitOperationRisk::RewritesLocalHistory;
    effect.rationale =
        replayed.isEmpty()
            ? QObject::tr("Nothing to replay: this branch is already on top "
                          "of %1.")
                  .arg(target)
            : QObject::tr(
                  "Each of your %1 commits is recreated on top of %2 with a "
                  "new hash. Anyone who already fetched the old ones will see "
                  "them as different commits.")
                  .arg(replayed.size())
                  .arg(target);

    effect.conflictsKnown = false;

    preview.after = toNodes(replayed, PreviewNode::State::Rewritten) +
                    toNodes(onto, PreviewNode::State::Unchanged, 1);
    if (!preview.after.isEmpty()) {
      preview.after.first().isHead = true;
    }
    effect.commands << QStringLiteral("git rebase %1").arg(target);
    break;
  }

  case GitOperationKind::Reset: {
    if (target.isEmpty()) {
      effect.valid = false;
      effect.error = QObject::tr("No commit to reset to.");
      return preview;
    }

    effect.commitsUnreachable = git->getCommitLogPage(
        QStringLiteral("%1..HEAD").arg(target), 0, contextCommits);
    effect.movesBranch = true;

    const QString mode = request.resetMode;
    if (mode == QLatin1String("hard")) {
      effect.changesIndex = true;
      effect.changesWorkingTree = true;
      effect.atRiskPaths = dirtyPaths(git);
      effect.risk = GitOperationRisk::MayDiscardUncommitted;
      effect.rationale = QObject::tr(
          "A hard reset rewrites the branch, the index and your files. The "
          "commits it drops stay in the reflog for a while; uncommitted edits "
          "do not exist anywhere else and are gone.");
    } else if (mode == QLatin1String("soft")) {
      effect.risk = effect.commitsUnreachable.isEmpty()
                        ? GitOperationRisk::Safe
                        : GitOperationRisk::RewritesLocalHistory;
      effect.rationale = QObject::tr(
          "A soft reset moves only the branch pointer. Everything those "
          "commits contained stays staged, ready to be recommitted.");
    } else {
      effect.changesIndex = true;
      effect.risk = effect.commitsUnreachable.isEmpty()
                        ? GitOperationRisk::Safe
                        : GitOperationRisk::RewritesLocalHistory;
      effect.rationale = QObject::tr(
          "A mixed reset moves the branch and clears the index, but leaves "
          "your files untouched. Nothing on disk is lost.");
    }

    const QList<GitCommitInfo> remaining =
        git->getCommitLogPage(target, 0, contextCommits);
    preview.after = toNodes(remaining, PreviewNode::State::Unchanged);
    if (!preview.after.isEmpty()) {
      preview.after.first().isHead = true;
    }
    for (const GitCommitInfo &commit : effect.commitsUnreachable) {
      preview.after.prepend(toNode(commit, PreviewNode::State::Unreachable, 1));
    }
    effect.commands << QStringLiteral("git reset --%1 %2").arg(mode, target);
    break;
  }

  case GitOperationKind::Revert:
  case GitOperationKind::CherryPick: {
    if (target.isEmpty()) {
      effect.valid = false;
      effect.error = QObject::tr("No commit selected.");
      return preview;
    }

    const GitCommitInfo source = git->getCommitDetails(target);
    GitCommitInfo created;
    created.shortHash = QObject::tr("new");
    created.subject = request.kind == GitOperationKind::Revert
                          ? QObject::tr("Revert \"%1\"").arg(source.subject)
                          : source.subject;
    effect.commitsAdded << created;
    effect.movesBranch = true;
    effect.changesWorkingTree = true;
    effect.risk = GitOperationRisk::Safe;
    effect.rationale =
        request.kind == GitOperationKind::Revert
            ? QObject::tr(
                  "A revert adds a new commit that undoes %1. The original "
                  "commit stays exactly where it is, which is why this is the "
                  "safe way to undo something others already have.")
                  .arg(source.shortHash)
            : QObject::tr(
                  "A cherry-pick copies the change of %1 into a new commit on "
                  "this branch. The original stays where it is, and the copy "
                  "has a different hash.")
                  .arg(source.shortHash);
    effect.conflictsKnown = false;

    preview.after = preview.before;
    preview.after.prepend(toNode(created, PreviewNode::State::Added, 0, true));
    if (preview.after.size() > 1) {
      preview.after[1].isHead = false;
    }
    effect.commands << (request.kind == GitOperationKind::Revert
                            ? QStringLiteral("git revert %1").arg(target)
                            : QStringLiteral("git cherry-pick %1").arg(target));
    break;
  }

  case GitOperationKind::BranchDelete: {
    if (target.isEmpty()) {
      effect.valid = false;
      effect.error = QObject::tr("No branch selected.");
      return preview;
    }

    effect.commitsUnreachable = git->getUnreachableAfterDelete(target);
    effect.risk = effect.commitsUnreachable.isEmpty()
                      ? GitOperationRisk::Safe
                      : GitOperationRisk::RewritesLocalHistory;
    effect.rationale =
        effect.commitsUnreachable.isEmpty()
            ? QObject::tr(
                  "Every commit on %1 is reachable from another ref, so "
                  "deleting the branch only removes a name.")
                  .arg(target)
            : QObject::tr(
                  "%1 commits exist only on this branch. Deleting it leaves "
                  "them with no name; they survive in the reflog for a while, "
                  "but nothing points at them.")
                  .arg(effect.commitsUnreachable.size());

    preview.after = preview.before;
    preview.after +=
        toNodes(effect.commitsUnreachable, PreviewNode::State::Unreachable, 1);
    effect.commands << QStringLiteral("git branch -d %1").arg(target);
    break;
  }

  case GitOperationKind::StashApply:
  case GitOperationKind::StashPop: {
    effect.changesWorkingTree = true;
    effect.changesIndex = true;
    effect.atRiskPaths = dirtyPaths(git);
    effect.risk = effect.atRiskPaths.isEmpty()
                      ? GitOperationRisk::Safe
                      : GitOperationRisk::MayDiscardUncommitted;
    effect.rationale =
        effect.atRiskPaths.isEmpty()
            ? QObject::tr("Your working tree is clean, so the stash applies "
                          "onto it without touching anything of yours.")
            : QObject::tr(
                  "You have uncommitted changes in %1 files. Applying a "
                  "stash on top of them can conflict, and %2")
                  .arg(effect.atRiskPaths.size())
                  .arg(request.kind == GitOperationKind::StashPop
                           ? QObject::tr("a pop drops the stash even so.")
                           : QObject::tr("the stash is kept either way."));
    preview.after = preview.before;
    effect.commands << QStringLiteral("git stash %1 stash@{%2}")
                           .arg(request.kind == GitOperationKind::StashPop
                                    ? QStringLiteral("pop")
                                    : QStringLiteral("apply"))
                           .arg(request.stashIndex);
    break;
  }
  }

  if ((effect.risk == GitOperationRisk::RewritesLocalHistory ||
       !effect.commitsRewritten.isEmpty() ||
       !effect.commitsUnreachable.isEmpty()) &&
      state.hasUpstream && state.ahead == 0 && state.behind == 0 &&
      !effect.commitsRewritten.isEmpty()) {
    effect.risk = GitOperationRisk::AffectsSharedHistory;
    effect.rationale +=
        QObject::tr(" These commits are already on %1, so rewriting them "
                    "forces everyone else to reconcile.")
            .arg(state.upstream);
  }

  return preview;
}
