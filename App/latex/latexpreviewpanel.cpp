#include "latexpreviewpanel.h"
#include "../core/logging/logger.h"
#include "latextools.h"
#include <QAction>
#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QScrollBar>

LatexPreviewPanel::LatexPreviewPanel(QWidget *parent)
    : QWidget(parent), m_logBrowser(nullptr), m_toolbar(nullptr),
      m_engineCombo(nullptr), m_statusLabel(nullptr), m_buildProcess(nullptr),
      m_autoBuild(false), m_building(false) {
  setupUi();
  m_buildTimer.setSingleShot(true);
  m_buildTimer.setInterval(500);
  connect(&m_buildTimer, &QTimer::timeout, this, &LatexPreviewPanel::build);
}

LatexPreviewPanel::~LatexPreviewPanel() {
  if (m_buildProcess) {
    m_buildProcess->disconnect();
    if (m_buildProcess->state() != QProcess::NotRunning) {
      m_buildProcess->kill();
      m_buildProcess->waitForFinished(3000);
    }
    delete m_buildProcess;
    m_buildProcess = nullptr;
  }
}

void LatexPreviewPanel::setupUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  setupToolbar();
  layout->addWidget(m_toolbar);

  m_logBrowser = new QTextBrowser(this);
  m_logBrowser->setObjectName("latexPreviewLogBrowser");
  m_logBrowser->setOpenLinks(false);
  m_logBrowser->setReadOnly(true);
  layout->addWidget(m_logBrowser, 1);

  setLayout(layout);
}

void LatexPreviewPanel::setupToolbar() {
  m_toolbar = new QToolBar(this);
  m_toolbar->setObjectName("latexPreviewToolbar");
  m_toolbar->setMovable(false);

  QAction *buildAction = m_toolbar->addAction(QString::fromUtf8("▶ Build"));
  buildAction->setToolTip("Build LaTeX Document");
  connect(buildAction, &QAction::triggered, this, &LatexPreviewPanel::build);

  QAction *cleanAction = m_toolbar->addAction(QString::fromUtf8("🗑 Clean"));
  cleanAction->setToolTip("Clean Auxiliary Files");
  connect(cleanAction, &QAction::triggered, this, &LatexPreviewPanel::clean);

  m_toolbar->addSeparator();

  m_engineCombo = new QComboBox(this);
  m_engineCombo->setObjectName("latexEngineCombo");
  m_engineCombo->addItem("pdflatex");
  m_engineCombo->addItem("xelatex");
  m_engineCombo->addItem("lualatex");
  m_engineCombo->addItem("latexmk");
  m_engineCombo->setToolTip("Select LaTeX Engine");
  m_toolbar->addWidget(m_engineCombo);

  m_toolbar->addSeparator();

  m_statusLabel = new QLabel("Ready", this);
  m_statusLabel->setObjectName("latexStatusLabel");
  m_toolbar->addWidget(m_statusLabel);

  m_toolbar->addSeparator();

  auto *autoBuildCheck = new QCheckBox("Auto-build", this);
  autoBuildCheck->setToolTip("Automatically build on save");
  autoBuildCheck->setChecked(m_autoBuild);
  connect(autoBuildCheck, &QCheckBox::toggled, this,
          &LatexPreviewPanel::setAutoBuild);
  m_toolbar->addWidget(autoBuildCheck);
}

void LatexPreviewPanel::setFilePath(const QString &filePath) {
  m_filePath = filePath;
  QFileInfo fi(filePath);
  m_basePath = fi.absolutePath();
}

bool LatexPreviewPanel::isLatexFile(const QString &extension) {
  return LatexTools::isLatexFile(extension);
}

void LatexPreviewPanel::setAutoBuild(bool enabled) { m_autoBuild = enabled; }

void LatexPreviewPanel::build() {
  if (m_building) {
    Logger::instance().warning("Build already in progress");
    return;
  }

  if (m_filePath.isEmpty()) {
    appendLog("No file set for building.", "red");
    return;
  }

  m_building = true;
  m_buildLog.clear();
  m_logBrowser->clear();

  m_statusLabel->setText("Building...");
  emit buildStarted();

  appendLog(QString("Building %1 with %2...")
                .arg(QFileInfo(m_filePath).fileName(), selectedEngine()),
            "#888888");

  if (m_buildProcess) {
    m_buildProcess->disconnect();
    delete m_buildProcess;
  }

  m_buildProcess = new QProcess(this);
  m_buildProcess->setWorkingDirectory(m_basePath);
  m_buildProcess->setProcessChannelMode(QProcess::MergedChannels);

  connect(m_buildProcess,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &LatexPreviewPanel::onBuildProcessFinished);
  connect(m_buildProcess, &QProcess::readyReadStandardOutput, this,
          &LatexPreviewPanel::onBuildProcessOutput);

  QString engine = selectedEngine();
  QStringList args = engineArgs();

  Logger::instance().info(
      QString("Starting LaTeX build: %1 %2").arg(engine, args.join(" ")));

  m_buildProcess->start(engine, args);
}

void LatexPreviewPanel::clean() {
  if (m_filePath.isEmpty()) {
    appendLog("No file set for cleaning.", "red");
    return;
  }

  QFileInfo fi(m_filePath);
  QString baseName = fi.completeBaseName();
  QDir dir(m_basePath);

  QStringList extensions = {"aux",  "log",          "out", "toc", "bbl",
                            "blg",  "fls",          "fdb_latexmk",
                            "synctex.gz"};

  int removed = 0;
  for (const QString &ext : extensions) {
    QString fileName = baseName + "." + ext;
    if (dir.exists(fileName)) {
      if (dir.remove(fileName)) {
        removed++;
      }
    }
  }

  appendLog(QString("Cleaned %1 auxiliary file(s).").arg(removed), "green");
  m_statusLabel->setText("Cleaned");
  Logger::instance().info(
      QString("Cleaned %1 auxiliary files for %2").arg(removed).arg(baseName));
}

void LatexPreviewPanel::onBuildProcessFinished(
    int exitCode, QProcess::ExitStatus exitStatus) {
  m_building = false;

  // Read any remaining output
  if (m_buildProcess) {
    QByteArray remaining = m_buildProcess->readAll();
    if (!remaining.isEmpty()) {
      m_buildLog += QString::fromUtf8(remaining);
      m_logBrowser->append(QString::fromUtf8(remaining));
    }
  }

  bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);

  // Parse log for diagnostics
  QList<LatexLogEntry> entries = LatexTools::parseLatexLog(m_buildLog);

  QList<LspDiagnostic> diagnostics;
  bool hasErrors = false;

  for (const LatexLogEntry &entry : entries) {
    LspDiagnostic diag;
    diag.range.start.line = qMax(0, entry.lineNumber - 1);
    diag.range.start.character = 0;
    diag.range.end.line = qMax(0, entry.lineNumber - 1);
    diag.range.end.character = 0;
    diag.message = entry.message;
    diag.source = "latex";

    if (entry.severity == 1) {
      diag.severity = LspDiagnosticSeverity::Error;
      hasErrors = true;
      appendLog(QString("Error (line %1): %2")
                    .arg(entry.lineNumber)
                    .arg(entry.message),
                "red");
    } else if (entry.severity == 2) {
      diag.severity = LspDiagnosticSeverity::Warning;
      appendLog(QString("Warning (line %1): %2")
                    .arg(entry.lineNumber)
                    .arg(entry.message),
                "#DAA520");
    } else {
      diag.severity = LspDiagnosticSeverity::Information;
    }

    diagnostics.append(diag);
  }

  if (!diagnostics.isEmpty()) {
    emit diagnosticsReady(diagnostics);
  }

  if (success && !hasErrors) {
    m_statusLabel->setText("Build Succeeded");
    appendLog("Build completed successfully.", "green");
  } else {
    m_statusLabel->setText("Build Failed");
    appendLog(
        QString("Build finished with exit code %1.").arg(exitCode), "red");
  }

  emit buildFinished(success && !hasErrors);

  Logger::instance().info(
      QString("LaTeX build finished: exit code %1, %2 diagnostic(s)")
          .arg(exitCode)
          .arg(diagnostics.size()));
}

void LatexPreviewPanel::onBuildProcessOutput() {
  if (!m_buildProcess)
    return;

  QByteArray data = m_buildProcess->readAll();
  QString text = QString::fromUtf8(data);
  m_buildLog += text;
  m_logBrowser->append(text);

  // Auto-scroll to bottom
  QScrollBar *sb = m_logBrowser->verticalScrollBar();
  sb->setValue(sb->maximum());
}

void LatexPreviewPanel::appendLog(const QString &text, const QString &color) {
  if (color.isEmpty()) {
    m_logBrowser->append(text);
  } else {
    m_logBrowser->append(
        QString("<span style=\"color: %1;\">%2</span>").arg(color, text.toHtmlEscaped()));
  }

  QScrollBar *sb = m_logBrowser->verticalScrollBar();
  sb->setValue(sb->maximum());
}

QString LatexPreviewPanel::selectedEngine() const {
  if (m_engineCombo)
    return m_engineCombo->currentText();
  return "pdflatex";
}

QStringList LatexPreviewPanel::engineArgs() const {
  QStringList args;
  QString engine = selectedEngine();

  if (engine == "latexmk") {
    args << "-pdf" << "-interaction=nonstopmode" << "-file-line-error"
         << QFileInfo(m_filePath).fileName();
  } else {
    args << "-interaction=nonstopmode" << "-file-line-error"
         << QFileInfo(m_filePath).fileName();
  }

  return args;
}

void LatexPreviewPanel::onSourceSaved() {
  if (m_autoBuild && !m_building) {
    m_buildTimer.start();
  }
}

void LatexPreviewPanel::updatePreview() { build(); }
