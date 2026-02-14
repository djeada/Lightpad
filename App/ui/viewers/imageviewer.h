#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QAction>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

class ImageViewer : public QWidget {
  Q_OBJECT

public:
  explicit ImageViewer(QWidget *parent = nullptr);
  ~ImageViewer() = default;

  bool loadImage(const QString &filePath);

  QString getFilePath() const { return m_filePath; }

  static bool isSupportedImageFormat(const QString &extension);

public slots:
  void zoomIn();
  void zoomOut();
  void fitToWindow();
  void actualSize();

protected:
  void wheelEvent(QWheelEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void setupUi();
  void setupToolbar();
  void updateZoomLabel();
  void applyZoom();

  QGraphicsView *m_graphicsView;
  QGraphicsScene *m_scene;
  QGraphicsPixmapItem *m_pixmapItem;
  QToolBar *m_toolbar;
  QLabel *m_zoomLabel;
  QLabel *m_infoLabel;
  QString m_filePath;
  QPixmap m_originalPixmap;
  double m_zoomFactor;

  static constexpr double ZOOM_STEP = 1.25;
  static constexpr double MIN_ZOOM = 0.1;
  static constexpr double MAX_ZOOM = 10.0;

  bool m_initialFitPending;
};

#endif
