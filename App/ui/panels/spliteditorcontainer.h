#ifndef SPLITEDITORCONTAINER_H
#define SPLITEDITORCONTAINER_H

#include <QList>
#include <QPointer>
#include <QSplitter>
#include <QWidget>

class LightpadTabWidget;
class MainWindow;

enum class SplitOrientation { Horizontal, Vertical };

class SplitEditorContainer : public QWidget {
  Q_OBJECT

public:
  explicit SplitEditorContainer(QWidget *parent = nullptr);
  ~SplitEditorContainer();
  void adoptTabWidget(LightpadTabWidget *tabWidget);

  void setMainWindow(MainWindow *window);

  LightpadTabWidget *currentTabWidget() const;

  QList<LightpadTabWidget *> allTabWidgets() const;

  int groupCount() const;

  LightpadTabWidget *split(SplitOrientation orientation);

  LightpadTabWidget *splitHorizontal();

  LightpadTabWidget *splitVertical();

  bool closeCurrentGroup();

  void focusNextGroup();

  void focusPreviousGroup();

  bool hasSplits() const;

  void unsplitAll();

signals:

  void currentGroupChanged(LightpadTabWidget *tabWidget);

  void splitCountChanged(int count);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  void onTabWidgetFocused();

private:
  void setupUI();
  LightpadTabWidget *createTabWidget();
  void updateFocus(LightpadTabWidget *tabWidget);
  void cleanupEmptySplitters();
  QSplitter *findParentSplitter(QWidget *widget) const;
  int findTabWidgetIndex(LightpadTabWidget *tabWidget) const;

  MainWindow *m_mainWindow;
  QSplitter *m_rootSplitter;
  QList<QPointer<LightpadTabWidget>> m_tabWidgets;
  QPointer<LightpadTabWidget> m_currentTabWidget;
};

#endif
