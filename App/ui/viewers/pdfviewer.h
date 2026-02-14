#ifndef PDFVIEWER_H
#define PDFVIEWER_H

#include <QHBoxLayout>
#include <QLabel>
#include <QPdfDocument>
#include <QPdfView>
#include <QSpinBox>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

class PdfViewer : public QWidget {
  Q_OBJECT

public:
  explicit PdfViewer(QWidget *parent = nullptr);
  ~PdfViewer() = default;

  bool loadPdf(const QString &filePath);

  QString getFilePath() const { return m_filePath; }

  static bool isSupportedPdfFormat(const QString &extension);

public slots:
  void zoomIn();
  void zoomOut();
  void goToPage(int page);
  void previousPage();
  void nextPage();
  void fitWidth();
  void fitPage();

private:
  void setupUi();
  void setupToolbar();
  void updatePageLabel();
  void updateZoomLabel();

  QPdfDocument *m_document;
  QPdfView *m_pdfView;
  QToolBar *m_toolbar;
  QLabel *m_pageLabel;
  QLabel *m_zoomLabel;
  QSpinBox *m_pageSpinBox;
  QString m_filePath;
  double m_zoomFactor;

  static constexpr double ZOOM_STEP = 1.25;
  static constexpr double MIN_ZOOM = 0.25;
  static constexpr double MAX_ZOOM = 5.0;
};

#endif
