#include "gitsyncmodel.h"
#include <QObject>

QString gitPullStrategyName(GitPullStrategy strategy) {
  switch (strategy) {
  case GitPullStrategy::Merge:
    return QObject::tr("Merge");
  case GitPullStrategy::Rebase:
    return QObject::tr("Rebase");
  case GitPullStrategy::FastForwardOnly:
    return QObject::tr("Fast-forward only");
  }
  return QString();
}

QString gitPullStrategyPreview(GitPullStrategy strategy,
                               const GitSyncState &state) {
  if (!state.hasUpstream) {
    return QObject::tr("This branch tracks nothing, so there is nothing to "
                       "pull. Set an upstream first.");
  }
  if (state.incoming.isEmpty()) {
    return QObject::tr("Nothing to pull — the upstream has no commits you do "
                       "not already have.");
  }

  switch (strategy) {
  case GitPullStrategy::Merge:
    if (state.pullCanFastForward()) {
      return QObject::
          tr("Your branch has no commits of its own, so this simply moves it "
             "forward onto the %1 incoming commits. No merge commit is "
             "created.")
              .arg(state.incoming.size());
    }
    return QObject::
        tr("Creates a merge commit joining your %1 commits to the %2 incoming "
           "ones. Both histories are kept exactly as they are.")
            .arg(state.outgoing.size())
            .arg(state.incoming.size());

  case GitPullStrategy::Rebase:
    if (state.outgoing.isEmpty()) {
      return QObject::
          tr("Nothing of yours to replay, so this behaves like a fast-forward "
             "onto the %1 incoming commits.")
              .arg(state.incoming.size());
    }
    return QObject::tr(
               "Replays your %1 commits on top of the %2 incoming ones. They "
               "get new "
               "hashes, so anyone who already has the old versions will see a "
               "conflict later.")
        .arg(state.outgoing.size())
        .arg(state.incoming.size());

  case GitPullStrategy::FastForwardOnly:
    if (state.pullCanFastForward()) {
      return QObject::
          tr("Moves your branch forward onto the %1 incoming commits and does "
             "nothing else.")
              .arg(state.incoming.size());
    }
    return QObject::tr(
               "Refuses to run: your branch has %1 commits the upstream does "
               "not, so "
               "there is no way to move forward without merging or rebasing.")
        .arg(state.outgoing.size());
  }
  return QString();
}

QString gitPushPreview(const GitSyncState &state, GitPushForce force) {
  if (!state.hasUpstream) {
    return QObject::tr("This branch has no upstream yet. Pushing will create "
                       "one and send your %1 commits.")
        .arg(state.outgoing.size());
  }
  if (state.outgoing.isEmpty() && force == GitPushForce::None) {
    return QObject::tr("Nothing to push — the upstream already has every "
                       "commit you have.");
  }

  switch (force) {
  case GitPushForce::None:
    if (state.pushIsFastForward()) {
      return QObject::tr("Sends your %1 commits. The upstream only moves "
                         "forward, so nothing can be lost.")
          .arg(state.outgoing.size());
    }
    return QObject::
        tr("Will be rejected: the upstream has %1 commits you do not have, so "
           "this is not a fast-forward. Pull first, or push with lease.")
            .arg(state.incoming.size());

  case GitPushForce::WithLease:
    return QObject::
        tr("Overwrites the upstream with your %1 commits, but only if it still "
           "points where your last fetch saw it. If someone pushed in the "
           "meantime, the push is refused instead of discarding their work.")
            .arg(state.outgoing.size());

  case GitPushForce::Force:
    return QObject::
        tr("Overwrites the upstream unconditionally. Any commit only the "
           "remote "
           "has — including %1 you can see here and anything pushed since your "
           "last fetch — is discarded.")
            .arg(state.incoming.size());
  }
  return QString();
}

QString gitSyncSummary(const GitSyncState &state) {
  if (!state.valid) {
    return QObject::tr("No Git repository.");
  }
  if (state.detachedHead) {
    return QObject::tr("HEAD is detached, so there is no branch to "
                       "synchronise.");
  }
  if (!state.hasUpstream) {
    return QObject::tr("%1 tracks no upstream branch yet.").arg(state.branch);
  }
  if (state.inSync()) {
    return QObject::tr("%1 and %2 point at the same commit.")
        .arg(state.branch, state.upstream);
  }
  if (state.diverged()) {
    return QObject::tr(
               "%1 and %2 diverged: %3 commits are only yours, %4 only theirs.")
        .arg(state.branch, state.upstream)
        .arg(state.outgoing.size())
        .arg(state.incoming.size());
  }
  if (state.outgoing.isEmpty()) {
    return QObject::tr("%1 is %2 commits behind %3.")
        .arg(state.branch)
        .arg(state.incoming.size())
        .arg(state.upstream);
  }
  return QObject::tr("%1 is %2 commits ahead of %3.")
      .arg(state.branch)
      .arg(state.outgoing.size())
      .arg(state.upstream);
}

GitPullStrategy parsePullStrategyConfig(const QString &pullRebase,
                                        const QString &pullFf) {
  const QString rebase = pullRebase.trimmed().toLower();
  if (rebase == QLatin1String("true") || rebase == QLatin1String("yes") ||
      rebase == QLatin1String("1") || rebase == QLatin1String("interactive") ||
      rebase == QLatin1String("merges")) {
    return GitPullStrategy::Rebase;
  }

  if (pullFf.trimmed().toLower() == QLatin1String("only")) {
    return GitPullStrategy::FastForwardOnly;
  }

  return GitPullStrategy::Merge;
}
