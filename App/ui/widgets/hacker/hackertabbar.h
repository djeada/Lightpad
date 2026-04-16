#ifndef HACKERTABBAR_H
#define HACKERTABBAR_H

#include "hackerwidget.h"
#include <QIcon>

class HackerTabBar : public HackerWidget {
  Q_OBJECT
  Q_PROPERTY(qreal indicatorX READ indicatorX WRITE setIndicatorX)

public:
  explicit HackerTabBar(QWidget *parent = nullptr);

  int addTab(const QString &text);
  int addTab(const QIcon &icon, const QString &text);
  void removeTab(int index);
  int count() const;
  int currentIndex() const;
  void setCurrentIndex(int index);
  QString tabText(int index) const;
  void setTabText(int index, const QString &text);
  void setTabIcon(int index, const QIcon &icon);
  void setTabToolTip(int index, const QString &tip);
  void setTabsClosable(bool closable);
  void setMovable(bool movable);
  void setTabModified(int index, bool modified);

  QSize sizeHint() const override;

signals:
  void currentChanged(int index);
  void tabCloseRequested(int index);
  void tabMoved(int from, int to);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  struct TabInfo {
    QString text;
    QIcon icon;
    QString toolTip;
    bool modified = false;
  };
  QList<TabInfo> m_tabs;
  int m_currentIndex = -1;
  int m_hoveredIndex = -1;
  int m_pressedIndex = -1;
  bool m_closable = true;
  bool m_movable = true;
  int m_scrollOffset = 0;

  qreal m_indicatorX = 0;
  qreal indicatorX() const { return m_indicatorX; }
  void setIndicatorX(qreal x);
  QPropertyAnimation *m_indicatorAnim = nullptr;

  int tabWidth() const;
  int tabAtPosition(const QPoint &pos) const;
  QRect tabRect(int index) const;
  QRect closeButtonRect(int index) const;
  bool isOverCloseButton(const QPoint &pos, int tabIndex) const;
  void animateIndicatorTo(int index);
};

#endif
