#ifndef GITREBASEPLAN_H
#define GITREBASEPLAN_H

#include "gitcommittypes.h"
#include <QList>
#include <QString>
#include <QStringList>

enum class GitRebaseAction {
  Pick,
  Reword,
  Edit,
  Squash,
  Fixup,
  Drop,
};

struct GitRebaseEntry {
  GitCommitInfo commit;
  GitRebaseAction action = GitRebaseAction::Pick;

  QString newMessage;

  bool published = false;

  bool isAutosquash = false;
  QString autosquashFor;
};

class GitRebasePlan {
public:
  void setEntries(const QList<GitRebaseEntry> &entries);
  const QList<GitRebaseEntry> &entries() const { return m_entries; }
  bool isEmpty() const { return m_entries.isEmpty(); }

  bool moveUp(int index);
  bool moveDown(int index);
  void setAction(int index, GitRebaseAction action);
  void setMessage(int index, const QString &message);

  int applyAutosquash();

  QString todoText() const;

  QStringList pendingMessages() const;

  QStringList validationProblems() const;

  QList<GitCommitInfo> resultingCommits() const;

  bool touchesPublishedHistory() const;

private:
  QList<GitRebaseEntry> m_entries;
};

QString gitRebaseActionName(GitRebaseAction action);
QString gitRebaseActionKeyword(GitRebaseAction action);
GitRebaseAction gitRebaseActionFromKeyword(const QString &keyword);
QString gitRebaseActionExplanation(GitRebaseAction action);

QString autosquashTargetSubject(const QString &subject);

#endif
