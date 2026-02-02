#ifndef BREADCRUMBWIDGET_H
#define BREADCRUMBWIDGET_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include "../../settings/theme.h"

/**
 * @brief Breadcrumb navigation widget showing file path segments
 * 
 * Displays the current file path as clickable segments for easy navigation.
 */
class BreadcrumbWidget : public QWidget {
    Q_OBJECT

public:
    explicit BreadcrumbWidget(QWidget* parent = nullptr);
    ~BreadcrumbWidget();

    /**
     * @brief Set the current file path to display
     */
    void setFilePath(const QString& filePath);

    /**
     * @brief Set the project root directory (for relative display)
     */
    void setProjectRoot(const QString& rootPath);

    /**
     * @brief Clear the breadcrumb display
     */
    void clear();

    /**
     * @brief Apply theme to the widget
     */
    void applyTheme(const Theme& theme);

signals:
    /**
     * @brief Emitted when user clicks on a path segment
     */
    void pathSegmentClicked(const QString& fullPath);

    /**
     * @brief Emitted when user requests to open a sibling file/folder
     */
    void siblingRequested(const QString& siblingPath);

private slots:
    void onSegmentClicked();
    void onDropdownClicked();

private:
    void setupUI();
    void rebuildBreadcrumbs();
    QStringList getPathSegments(const QString& path) const;
    QString buildPathUpTo(int segmentIndex) const;

    QHBoxLayout* m_layout;
    QString m_filePath;
    QString m_projectRoot;
    QList<QPushButton*> m_segments;
    QList<QLabel*> m_separators;
    Theme m_theme;
};

#endif // BREADCRUMBWIDGET_H
