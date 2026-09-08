#include "gitstashshelf.h"
#include "gitintegration.h"
#include <QObject>

int GitStashCard::additions() const {
  int total = 0;
  for (const GitComparisonFile &file : files) {
    total += file.additions;
  }
  return total;
}

int GitStashCard::deletions() const {
  int total = 0;
  for (const GitComparisonFile &file : files) {
    total += file.deletions;
  }
  return total;
}

QString gitStashApplyExplanation(bool pop) {
  return pop ? QObject::tr(
                   "Pop applies the changes and then deletes the stash entry — "
                   "but only if applying succeeded. If it conflicts, the entry "
                   "stays so nothing is lost.")
             : QObject::tr(
                   "Apply puts the changes into your working tree and leaves "
                   "the stash entry alone, so you can apply it again "
                   "elsewhere.");
}

QString gitStashConflictSummary(const QStringList &conflicts,
                                bool baseIsCurrent) {
  if (!conflicts.isEmpty()) {
    return conflicts.size() == 1
               ? QObject::tr("Git expects a conflict in %1.")
                     .arg(conflicts.first())
               : QObject::tr("Git expects conflicts in %1 files: %2.")
                     .arg(conflicts.size())
                     .arg(conflicts.join(QStringLiteral(", ")));
  }
  if (baseIsCurrent) {
    return QObject::tr(
        "This stash was taken on the commit you are on, and Git sees no "
        "conflict — it should apply cleanly.");
  }
  return QObject::tr(
      "This stash was taken on a different commit. Git sees no conflict from "
      "here, but a stash is tied to the state it was made in, so read the "
      "files before applying.");
}

QList<GitStashCard> buildStashShelf(GitIntegration *git) {
  QList<GitStashCard> cards;
  if (!git || !git->isValidRepository()) {
    return cards;
  }

  for (const GitStashDetail &detail : git->stashDetails()) {
    GitStashCard card;
    card.index = detail.index;
    card.selector = QStringLiteral("stash@{%1}").arg(detail.index);
    card.message = detail.message;
    card.branch = detail.branch;
    card.commitHash = detail.commitHash;
    card.baseHash = detail.baseHash;
    card.baseSubject = detail.baseSubject;
    card.relativeDate = detail.relativeDate;

    card.includesUntracked = detail.parentCount >= 3;

    card.files = parseComparisonFiles(git->stashNumstat(detail.index),
                                      git->stashNameStatus(detail.index));
    cards.append(card);
  }

  return cards;
}

QStringList predictStashConflicts(GitIntegration *git,
                                  const GitStashCard &card) {
  if (!git || card.selector.isEmpty()) {
    return {};
  }

  return git->predictMergeConflicts(QStringLiteral("HEAD"), card.selector);
}
