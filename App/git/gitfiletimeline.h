#ifndef GITFILETIMELINE_H
#define GITFILETIMELINE_H

#include "gitcommittypes.h"
#include <QList>
#include <QString>

struct GitFileRevision {
  GitCommitInfo commit;

  QString pathAtRevision;

  QString previousPath;

  QChar status = QLatin1Char('M');
  int additions = 0;
  int deletions = 0;

  bool isWorkingTree = false;

  bool isRename() const {
    return status == QLatin1Char('R') || status == QLatin1Char('C');
  }
};

struct GitFileTimelineOptions {
  bool allBranches = false;
  bool firstParentOnly = false;

  bool followRenames = true;

  QString author;

  QString since;
  QString until;

  int lineRangeStart = -1;
  int lineRangeEnd = -1;

  bool hasLineRange() const {
    return lineRangeStart > 0 && lineRangeEnd >= lineRangeStart;
  }
};

QList<GitFileRevision> parseFileTimeline(const QString &nameStatusOutput,
                                         const QString &numstatOutput);

QString gitFileRevisionSummary(const GitFileRevision &revision);

#endif
