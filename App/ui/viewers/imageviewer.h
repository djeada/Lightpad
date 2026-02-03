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

/**
 * @brief Image viewer widget for displaying image files
 *
 * Supports common image formats: PNG, JPG, JPEG, GIF, BMP, WEBP, SVG
 * Provides zoom in/out, fit to window, and actual size viewing modes
 */
class ImageViewer : public QWidget {
  Q_OBJECT

public:
  explicit ImageViewer(QWidget *parent = nullptr);
  ~ImageViewer() = default;

  /**
   * @brief Load and display an image from the given file path
   * @param filePath Path to the image file
   * @return true if image was loaded successfully, false otherwise
   */
  bool loadImage(const QString &filePath);

  /**
   * @brief Get the currently loaded file path
   * @return The file path of the loaded image
   */
  QString getFilePath() const { return m_filePath; }

  /**
   * @brief Check if a file extension is a supported image format
   * @param extension File extension (without dot)
   * @return true if the extension is a supported image format
   */
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

#endif // IMAGEVIEWER_H
