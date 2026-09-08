#ifndef GITCOMMITTYPES_H
#define GITCOMMITTYPES_H

#include <QString>
#include <QStringList>

struct GitCommitInfo {
  QString hash;
  QString shortHash;
  QString author;
  QString authorEmail;
  QString date;
  QString relativeDate;
  QString subject;
  QString body;
  QStringList parents;
};

#endif
