#ifndef GITSTASHSHELF_H
#define GITSTASHSHELF_H

#include "gitcomparison.h"
#include <QList>
#include <QString>

class GitIntegration;

struct GitStashCard {
  int index = 0;

  QString selector;
  QString message;

  QString branch;

  QString commitHash;
  QString baseHash;
  QString baseSubject;
  QString relativeDate;

  bool includesUntracked = false;

  QList<GitComparisonFile> files;

  int additions() const;
  int deletions() const;
};

QString gitStashApplyExplanation(bool pop);

QString gitStashConflictSummary(const QStringList &conflicts,
                                bool baseIsCurrent);

QList<GitStashCard> buildStashShelf(GitIntegration *git);

QStringList predictStashConflicts(GitIntegration *git,
                                  const GitStashCard &card);

#endif
