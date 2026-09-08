#include "gitworktreemap.h"
#include "gitintegration.h"
#include <QDir>
#include <QObject>

QList<GitWorktreeCard> parseWorktreeList(const QString &output) {
  QList<GitWorktreeCard> cards;
  GitWorktreeCard current;
  bool have = false;

  const auto flush = [&]() {
    if (have && !current.path.isEmpty()) {
      current.name = QDir(current.path).dirName();
      cards.append(current);
    }
    current = GitWorktreeCard();
    have = false;
  };

  for (const QString &line : output.split(QLatin1Char('\n'))) {
    if (line.startsWith(QLatin1String("worktree "))) {
      flush();
      current.path = line.mid(9).trimmed();

      current.isMain = cards.isEmpty();
      have = true;
    } else if (line.startsWith(QLatin1String("HEAD "))) {
      current.headHash = line.mid(5).trimmed();
    } else if (line.startsWith(QLatin1String("branch "))) {
      const QString ref = line.mid(7).trimmed();
      current.branch =
          ref.startsWith(QLatin1String("refs/heads/")) ? ref.mid(11) : ref;
    } else if (line.trimmed() == QLatin1String("detached")) {
      current.detached = true;
    } else if (line.startsWith(QLatin1String("prunable"))) {
      current.prunable = true;
      current.prunableReason = line.mid(8).trimmed();
    } else if (line.startsWith(QLatin1String("locked"))) {
      current.lockedReason = line.mid(6).trimmed();
    }
  }
  flush();

  return cards;
}

QString gitWorktreeRemovalWarning(const GitWorktreeCard &card) {
  if (card.isMain) {
    return QObject::tr(
        "This is the main working directory. It cannot be removed as a "
        "worktree.");
  }
  if (!card.lockedReason.isEmpty()) {
    return QObject::tr("Locked: %1. Unlock it before removing.")
        .arg(card.lockedReason);
  }
  if (card.hasUncommittedWork()) {
    QStringList parts;
    if (card.staged > 0) {
      parts << QObject::tr("%1 staged").arg(card.staged);
    }
    if (card.modified > 0) {
      parts << QObject::tr("%1 modified").arg(card.modified);
    }
    if (card.untracked > 0) {
      parts << QObject::tr("%1 untracked").arg(card.untracked);
    }
    if (card.conflicted > 0) {
      parts << QObject::tr("%1 conflicted").arg(card.conflicted);
    }
    return QObject::tr(
               "It still holds uncommitted work (%1). Removing the worktree "
               "deletes that directory, and those changes exist nowhere else.")
        .arg(parts.join(QStringLiteral(", ")));
  }
  return QString();
}

QString gitWorktreeSummary(const GitWorktreeCard &card) {
  QStringList parts;
  parts << (card.detached ? QObject::tr("detached at %1").arg(card.shortHead())
                          : QObject::tr("on %1").arg(card.branch));

  if (card.hasUncommittedWork()) {
    parts << QObject::tr("%1 uncommitted changes")
                 .arg(card.staged + card.modified + card.untracked +
                      card.conflicted);
  } else {
    parts << QObject::tr("clean");
  }

  if (!card.upstream.isEmpty() && (card.ahead > 0 || card.behind > 0)) {
    parts << QObject::tr("↑%1 ↓%2").arg(card.ahead).arg(card.behind);
  }
  if (card.prunable) {
    parts << QObject::tr("stale entry");
  }
  return parts.join(QStringLiteral(" · "));
}

QList<GitWorktreeCard> buildWorktreeMap(GitIntegration *git) {
  if (!git || !git->isValidRepository()) {
    return {};
  }

  QList<GitWorktreeCard> cards = parseWorktreeList(git->worktreeListRaw());
  const QString openPath = QDir(git->repositoryPath()).absolutePath();

  for (GitWorktreeCard &card : cards) {
    card.isCurrent = QDir(card.path).absolutePath() == openPath;

    if (card.prunable) {

      continue;
    }

    const GitRepositoryState state = git->worktreeState(card.path);
    if (!state.valid) {
      continue;
    }
    card.staged = state.stagedCount;
    card.modified = state.modifiedCount;
    card.untracked = state.untrackedCount;
    card.conflicted = state.conflictedCount;
    card.ahead = state.ahead;
    card.behind = state.behind;
    card.upstream = state.upstream;
    if (card.headSubject.isEmpty()) {
      card.headSubject = state.headSubject;
    }
  }

  return cards;
}
