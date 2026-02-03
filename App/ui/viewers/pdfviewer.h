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

/**
 * @brief PDF viewer widget for displaying PDF files
 *
 * Provides page navigation, zoom controls, and various viewing modes
 */
class PdfViewer : public QWidget {
  Q_OBJECT

public:
  explicit PdfViewer(QWidget *parent = nullptr);
  ~PdfViewer() = default;

  /**
   * @brief Load and display a PDF from the given file path
   * @param filePath Path to the PDF file
   * @return true if PDF was loaded successfully, false otherwise
   */
  bool loadPdf(const QString &filePath);

  /**
   * @brief Get the currently loaded file path
   * @return The file path of the loaded PDF
   */
  QString getFilePath() const { return m_filePath; }

  /**
   * @brief Check if a file extension is a supported PDF format
   * @param extension File extension (without dot)
   * @return true if the extension is pdf
   */
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

#endif // PDFVIEWER_H
