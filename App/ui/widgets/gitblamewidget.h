#ifndef GITBLAMEWIDGET_H
#define GITBLAMEWIDGET_H

#include "../../git/gitintegration.h"
#include "../../settings/theme.h"
#include <QPointer>
#include <QVector>
#include <QWidget>

class QFrame;

// The editor supplies block geometry, so annotations stay aligned with wrapped
// and folded source lines without maintaining a second scroll position.
class GitBlameWidget : public QWidget {
  Q_OBJECT
public:
  struct VisibleLine {
    int number;
    int top;
    int height;
    bool operator==(const VisibleLine &other) const {
      return number == other.number && top == other.top &&
             height == other.height;
    }
  };

  explicit GitBlameWidget(QWidget *parent = nullptr);
  void setData(const QMap<int, GitBlameLineInfo> &data);
  void markOutdated();
  void setVisibleLines(const QVector<VisibleLine> &lines, int currentLine);
  void applyTheme(const Theme &theme);
  void setEditorFont(const QFont &font);
  bool hasData() const { return !m_data.isEmpty(); }
  bool hasContent() const { return m_outdated || hasData(); }

signals:
  void openCommitRequested(const QString &hash);
  void fileHistoryRequested(int line);
  void lineSelected(int line);
  void hideRequested();
  void returnToEditorRequested();

protected:
  void paintEvent(QPaintEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void leaveEvent(QEvent *) override;
  void keyPressEvent(QKeyEvent *) override;
  void focusInEvent(QFocusEvent *) override;
  void focusOutEvent(QFocusEvent *) override;
  void contextMenuEvent(QContextMenuEvent *) override;

private:
  struct Group {
    int first;
    int last;
    QRect rect;
  };
  QVector<Group> visibleGroups() const;
  int lineAt(int y) const;
  void showDetails(int line);
  QString attribution(int line) const;
  static QString identity(const GitBlameLineInfo &info);
  static bool committed(const GitBlameLineInfo &info);

  QMap<int, GitBlameLineInfo> m_data;
  QVector<VisibleLine> m_lines;
  QPointer<QFrame> m_popup;
  Theme m_theme;
  QFont m_labelFont;
  QFont m_detailFont;
  int m_currentLine = 0;
  int m_hoveredLine = 0;
  int m_keyboardLine = 0;
  bool m_outdated = false;
};

#endif
