#ifndef CODEFOLDING_H
#define CODEFOLDING_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QTextCursor>

class QTextDocument;
class QTextBlock;

class CodeFoldingManager {
public:
  explicit CodeFoldingManager(QTextDocument *document);

  const QSet<int> &foldedBlocks() const;

  bool isFolded(int blockNumber) const;

  bool isFoldable(int blockNumber) const;

  int getFoldingLevel(int blockNumber) const;

  int findFoldEndBlock(int startBlock) const;

  bool foldBlock(int blockNumber);

  bool unfoldBlock(int blockNumber);

  void toggleFoldAtLine(int line);

  void foldAll();

  void unfoldAll();

  void foldToLevel(int level);

  void foldComments();

  void unfoldComments();

  bool isRegionStart(int blockNumber) const;
  bool isRegionEnd(int blockNumber) const;
  int findRegionEndBlock(int startBlock) const;

  bool isCommentBlockStart(int blockNumber) const;
  int findCommentBlockEnd(int startBlock) const;

  QJsonObject saveFoldState() const;

  void restoreFoldState(const QJsonObject &state);

private:
  static bool isSingleLineComment(const QString &trimmedText);

  void addFold(int blockNumber);
  void removeFold(int blockNumber);
  void clearFolds();
  void syncFolds() const;

  QTextDocument *m_document;
  mutable QSet<int> m_foldedBlocks;
  mutable QList<QTextCursor> m_foldAnchors;
};

#endif
