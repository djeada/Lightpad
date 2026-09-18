#ifndef CONFLICTRESOLVERVIEW_H
#define CONFLICTRESOLVERVIEW_H

#include "../../git/gitconflictresolution.h"
#include "../../settings/theme.h"
#include <QHash>
#include <QWidget>

class GitIntegration;
class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

class ConflictResolverView : public QWidget {
  Q_OBJECT

public:
  ConflictResolverView(GitIntegration *git, const QString &filePath,
                       QWidget *parent = nullptr);

  QString relativePath() const { return m_relativePath; }
  QString absolutePath() const;

  int totalConflicts() const { return m_resolver.totalConflicts(); }
  int remainingConflicts() const { return m_resolver.remainingCount(); }
  int decidedConflicts() const { return m_resolver.resolvedCount(); }
  bool hasUnsavedDecisions() const { return m_dirty; }

  bool reload();

  void applyTheme(const Theme &theme);

  const ConflictFileResolver &resolver() const { return m_resolver; }

public slots:
  void goToNextConflict();
  void goToPreviousConflict();
  void undoDecision();
  void redoDecision();
  void keepAllOurs();
  void keepAllTheirs();

  bool saveProgress();

  bool markFileDone();

signals:
  void fileResolved(const QString &relativePath);
  void progressChanged();
  void rawFileRequested(const QString &absolutePath);

private:
  void buildUi();
  void buildHeader(QVBoxLayout *layout);
  void buildToolbar(QVBoxLayout *layout);
  void rebuildBody();
  QWidget *buildContextCard(const ConflictDocumentItem &item, int itemIndex);
  QWidget *buildConflictCard(const ConflictRegion &region, int startLine);
  QWidget *buildSidePane(QWidget *parent, const QString &title,
                         const QString &subtitle, const QStringList &lines,
                         const QColor &tint, const QString &buttonText,
                         ConflictChoice choice, int regionId);
  QWidget *buildCodeBlock(QWidget *parent, const QStringList &lines,
                          const QColor &tint);

  void applyChoice(int regionId, ConflictChoice choice,
                   const QStringList &customLines = QStringList());
  void scrollToRegion(int regionId);
  void updateHeader();
  void updateToolbar();
  int firstUndecidedRegion() const;

  QString sideName(const QString &markerLabel, bool ours) const;

  GitIntegration *m_git = nullptr;
  QString m_relativePath;
  ConflictFileResolver m_resolver;
  Theme m_theme;
  bool m_themeReady = false;
  bool m_dirty = false;
  int m_currentRegionId = 0;
  QHash<int, QWidget *> m_regionCards;
  QHash<int, bool> m_expandedContext;
  QHash<int, bool> m_showBase;
  QHash<int, bool> m_editing;

  QLabel *m_fileLabel = nullptr;
  QLabel *m_branchesLabel = nullptr;
  QLabel *m_countLabel = nullptr;
  QProgressBar *m_progress = nullptr;

  QPushButton *m_previousButton = nullptr;
  QPushButton *m_nextButton = nullptr;
  QPushButton *m_undoButton = nullptr;
  QPushButton *m_redoButton = nullptr;
  QPushButton *m_keepAllOursButton = nullptr;
  QPushButton *m_keepAllTheirsButton = nullptr;
  QPushButton *m_rawButton = nullptr;
  QPushButton *m_doneButton = nullptr;

  QScrollArea *m_scroll = nullptr;
  QWidget *m_body = nullptr;
  QVBoxLayout *m_bodyLayout = nullptr;
};

#endif
