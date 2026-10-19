#include "gitdialog.h"
#include "../uimetrics.h"
#include <QBoxLayout>
#include <QShowEvent>

namespace {
QPushButton *dismissButton(QWidget *surface) {
  for (auto *button : surface->findChildren<QPushButton *>())
    if (button->property("gitDismiss").toBool())
      return button;
  return nullptr;
}
} // namespace

GitDialog::GitDialog(QWidget *parent) : StyledDialog(parent) {
  setProperty("gitUiSurface", true);
}

void GitDialog::setGitDescription(const QString &description) {
  m_description = description;
  if (m_gitDescription)
    m_gitDescription->setText(description);
}

void GitDialog::useGitHeading(QLabel *heading, QLabel *description) {
  m_gitHeading = heading;
  m_gitDescription = description;
  heading->setProperty("gitHeading", true);
  heading->setTextFormat(Qt::PlainText);
  heading->setWordWrap(true);
  heading->setMinimumWidth(0);
  if (description) {
    description->setProperty("gitDescription", true);
    description->setTextFormat(Qt::PlainText);
    description->setWordWrap(true);
    description->setMinimumWidth(0);
  }
}

void GitDialog::prepareLayout() {
  auto *outer = qobject_cast<QVBoxLayout *>(layout());
  if (!outer)
    return;
  const bool edgeToEdge = property("gitEdgeToEdge").toBool();
  const int margin = edgeToEdge ? 0 : UiMetrics::SpaceXl;
  outer->setContentsMargins(margin, margin, margin, margin);
  outer->setSpacing(edgeToEdge ? 0 : UiMetrics::SpaceLg);
  if (!m_gitHeading) {
    auto *header = new QWidget(this);
    header->setObjectName("gitDialogHeader");
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(edgeToEdge ? 16 : 0, edgeToEdge ? 12 : 0,
                                     edgeToEdge ? 16 : 0, edgeToEdge ? 12 : 0);
    headerLayout->setSpacing(UiMetrics::SpaceSm);
    auto *title = new QLabel(windowTitle(), header);
    auto *description = new QLabel(m_description, header);
    description->setVisible(!m_description.isEmpty());
    useGitHeading(title, description);
    headerLayout->addWidget(title);
    headerLayout->addWidget(description);
    outer->insertWidget(0, header);
  }
  if (!dismissButton(this)) {
    auto *footer = new QHBoxLayout;
    footer->addStretch();
    auto *close = new QPushButton(tr("Close"), this);
    close->setObjectName("gitDialogClose");
    close->setProperty("gitDismiss", true);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    footer->addWidget(close);
    outer->addLayout(footer);
  }
  for (auto *row : findChildren<QBoxLayout *>())
    GitUi::wrapActions(row);
}

void GitDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);

  prepareLayout();
  GitUi::polish(this, theme);
}

void GitDialog::showEvent(QShowEvent *event) {
  prepareLayout();
  GitUi::polish(this, m_theme);
  StyledDialog::showEvent(event);
}
