#include "mainwindow.h"
#include "previewwidget.h"
#include "compiler.h"
#include "highlighter.h"
#include "preamble.h"
#include "history.h"
#include "historywindow.h"

#include <QPlainTextEdit>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QFont>
#include <QPalette>
#include <QDir>
#include <QFile>
#include <QStatusBar>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QTimer>
#include <QShortcut>
#include <QColorDialog>
#include <QPixmap>
#include <QPainter>
#include <QClipboard>
#include <QMimeData>
#include <QApplication>
#include <QMenuBar>
#include <QAction>
#include "clipboard_mac.h"
#include <QStandardPaths>
#include <QRegularExpression>
#include <QFontDatabase>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Typstit");
    setMinimumSize(560, 400);

    setupWorkspace();

    m_store    = new HistoryStore(this);
    m_pdfDoc   = new QPdfDocument(this);
    m_compiler = new Compiler(this);

    connect(m_compiler, &Compiler::availabilityChecked, this, &MainWindow::onAvailabilityChecked);
    connect(m_compiler, &Compiler::compileFinished,     this, &MainWindow::onCompileFinished);
    connect(m_compiler, &Compiler::compileFailed,       this, &MainWindow::onCompileFailed);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(500);
    connect(m_debounce, &QTimer::timeout, this, &MainWindow::triggerTypeset);

    // ── Central layout ──────────────────────────────────────────────────────
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *vLayout = new QVBoxLayout(central);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    vLayout->addWidget(buildToolbar());

    auto *sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: #d0d0d0; border: none;");
    vLayout->addWidget(sep);

    auto *splitter = new QSplitter(Qt::Vertical);
    splitter->setHandleWidth(4);
    splitter->setStyleSheet(
        "QSplitter::handle { background: #d0d0d0; }"
        "QSplitter::handle:hover { background: #aaa; }"
    );
    vLayout->addWidget(splitter);

    // ── Editor ──────────────────────────────────────────────────────────────
    m_editor = new QPlainTextEdit();
    m_editor->setPlainText("$ integral_0^infinity e^(-x^2) d x = sqrt(pi) / 2 $");
    m_editor->setMinimumHeight(36);
    m_editor->setFrameShape(QFrame::NoFrame);
    {
        QFont f;
        f.setFamily("Menlo");
        f.setStyleHint(QFont::Monospace);
        f.setPointSize(13);
        m_editor->setFont(f);
        m_editor->document()->setDefaultFont(f);
    }
    {
        QPalette p = m_editor->palette();
        p.setColor(QPalette::Base, QColor("#ffffff"));
        p.setColor(QPalette::Text, QColor("#1d1d1f"));
        m_editor->setPalette(p);
    }
    m_highlighter = new SyntaxHighlighter(m_editor->document());
    connect(m_editor, &QPlainTextEdit::textChanged, this, &MainWindow::onTextChanged);
    splitter->addWidget(m_editor);

    // ── Preview ─────────────────────────────────────────────────────────────
    m_preview = new PreviewWidget(m_pdfDoc);
    m_preview->setMinimumHeight(60);
    splitter->addWidget(m_preview);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    // ── Status bar ──────────────────────────────────────────────────────────
    m_status = new QLabel("Checking for typst…");
    m_status->setStyleSheet("color: #6e6e73; font-size: 11px;");
    statusBar()->addWidget(m_status);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setStyleSheet(
        "QStatusBar { background: #f4f4f4; border-top: 1px solid #d0d0d0; padding: 0 6px; }"
    );

    // ── Keyboard shortcuts ───────────────────────────────────────────────────
    new QShortcut(QKeySequence("Ctrl+T"),       this, [this] { triggerTypeset(); });
    new QShortcut(QKeySequence("Shift+Ctrl+V"), this, [this] { pasteBack(); });

    // ── Menu bar ─────────────────────────────────────────────────────────────
    QMenu *editMenu = menuBar()->addMenu("Edit");

    QAction *typesetAct = editMenu->addAction("Typeset", this, &MainWindow::triggerTypeset);
    typesetAct->setShortcut(QKeySequence("Ctrl+T"));

    QAction *autoAct = editMenu->addAction("Auto-Typeset");
    autoAct->setCheckable(true);
    connect(autoAct, &QAction::toggled, m_autoCheck, &QCheckBox::setChecked);
    connect(m_autoCheck, &QCheckBox::toggled, autoAct, &QAction::setChecked);

    editMenu->addSeparator();

    QAction *pasteBackAct = editMenu->addAction("Paste Back Typst Source",
                                                this, &MainWindow::pasteBack);
    pasteBackAct->setShortcut(QKeySequence("Shift+Ctrl+V"));

    m_compiler->findTypst();
}

// ── Toolbar ──────────────────────────────────────────────────────────────────

QWidget *MainWindow::buildToolbar()
{
    auto *bar = new QWidget();
    bar->setFixedHeight(38);
    bar->setAutoFillBackground(true);
    {
        QPalette p = bar->palette();
        p.setColor(QPalette::Window, QColor("#f4f4f4"));
        bar->setPalette(p);
    }

    auto *h = new QHBoxLayout(bar);
    h->setContentsMargins(8, 0, 8, 0);
    h->setSpacing(6);

    auto addC = [&](QWidget *w) { h->addWidget(w, 0, Qt::AlignVCenter); };

    m_typesetBtn = new QPushButton("▶  Typeset");
    m_typesetBtn->setEnabled(false);
    connect(m_typesetBtn, &QPushButton::clicked, this, &MainWindow::triggerTypeset);
    addC(m_typesetBtn);

    m_autoCheck = new QCheckBox("Auto");
    connect(m_autoCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (on) m_debounce->start();
    });
    addC(m_autoCheck);

    // Separator
    auto *vsep = new QFrame();
    vsep->setFrameShape(QFrame::VLine);
    vsep->setFixedSize(1, 16);
    vsep->setStyleSheet("background: #d0d0d0; border: none;");
    h->addSpacing(6);
    addC(vsep);
    h->addSpacing(2);

    m_fontCombo = new QComboBox();
    {
        // Typst-bundled fonts that may not be installed system-wide
        static const QStringList kBundled = {
            "Libertinus Serif", "Libertinus Sans", "Libertinus Mono",
            "New Computer Modern", "New Computer Modern Math", "DejaVu Sans Mono",
        };
        QStringList system = QFontDatabase::families();
        QSet<QString> systemSet(system.begin(), system.end());
        QStringList fonts;
        for (const QString &f : kBundled)
            if (!systemSet.contains(f)) fonts << f;
        fonts << system;
        m_fontCombo->addItems(fonts);
        m_fontCombo->setCurrentText("Libertinus Serif");
        m_fontName = m_fontCombo->currentText();
    }
    m_fontCombo->setMinimumWidth(155);
    connect(m_fontCombo, &QComboBox::currentTextChanged, this, [this](const QString &f) {
        m_fontName = f;
        if (m_autoCheck->isChecked()) m_debounce->start();
    });
    addC(m_fontCombo);

    m_sizeBox = new QSpinBox();
    m_sizeBox->setRange(6, 288);
    m_sizeBox->setValue(36);
    m_sizeBox->setFixedWidth(65);
    m_sizeBox->setAlignment(Qt::AlignCenter);
    connect(m_sizeBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_fontSize = v;
        if (m_autoCheck->isChecked()) m_debounce->start();
    });
    addC(m_sizeBox);

    m_colorBtn = new QToolButton();
    m_colorBtn->setFixedSize(26, 26);
    m_colorBtn->setAutoRaise(true);
    m_colorBtn->setToolTip("Text color");
    connect(m_colorBtn, &QToolButton::clicked, this, &MainWindow::pickColor);
    updateColorBtn();
    addC(m_colorBtn);

    h->addStretch();

    m_historyBtn = new QToolButton();
    m_historyBtn->setText("⏱");
    m_historyBtn->setFixedSize(30, 26);
    m_historyBtn->setAutoRaise(true);
    m_historyBtn->setToolTip("History");
    connect(m_historyBtn, &QToolButton::clicked, this, [this] {
        auto *win = new HistoryWindow(m_store, this);
        connect(win, &HistoryWindow::restoreRequested,
                this, [this](const HistoryEntry &e) { restoreEntry(e, false); });
        connect(win, &HistoryWindow::restoreAndCompileRequested,
                this, [this](const HistoryEntry &e) { restoreEntry(e, true); });
        win->setAttribute(Qt::WA_DeleteOnClose);
        win->show();
    });
    addC(m_historyBtn);

    return bar;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void MainWindow::setupWorkspace()
{
    m_workspace  = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/workspace";
    m_outputPath = m_workspace + "/output.pdf";
    QDir().mkpath(m_workspace);
}

void MainWindow::setStatus(const QString &msg, bool error)
{
    m_status->setText(msg);
    m_status->setStyleSheet(error
        ? "color: #c0392b; font-size: 11px;"
        : "color: #6e6e73; font-size: 11px;");
}

void MainWindow::updateColorBtn()
{
    QPixmap pm(16, 16);
    pm.fill(m_textColor);
    {
        QPainter p(&pm);
        p.setPen(QColor(0, 0, 0, 64));
        p.drawRect(0, 0, 15, 15);
    }
    m_colorBtn->setIcon(QIcon(pm));
    m_colorBtn->setText({});
}

// ── Compilation ───────────────────────────────────────────────────────────────

void MainWindow::onTextChanged()
{
    if (m_autoCheck->isChecked()) m_debounce->start();
}

void MainWindow::triggerTypeset()
{
    if (m_isCompiling || !m_compiler->isAvailable()) return;
    m_isCompiling = true;
    m_typesetBtn->setEnabled(false);
    setStatus("Compiling…");

    QString wrapped = Preamble::wrap(
        m_editor->toPlainText(), m_fontName, m_fontSize, m_textColor);
    m_compiler->compile(wrapped, m_workspace, m_outputPath);
}

void MainWindow::onAvailabilityChecked(bool available)
{
    m_typesetBtn->setEnabled(available);
    if (available) {
        setStatus("Ready");
    } else {
        setStatus("typst not found — install with: brew install typst", true);
    }
}

void MainWindow::onCompileFinished(const QByteArray &pdfData, qint64 elapsedMs)
{
    m_isCompiling = false;
    m_typesetBtn->setEnabled(true);

    // Write to temp path for drag-out, reload QPdfDocument, refresh preview
    QFile f(m_outputPath);
    if (f.open(QIODevice::WriteOnly)) { f.write(pdfData); f.close(); }

    m_pdfDoc->close();
    m_pdfDoc->load(m_outputPath);
    m_preview->setTempFilePath(m_outputPath);
    m_preview->refresh();

    // Persist to history
    QString colorHex = m_textColor.name(QColor::HexRgb).mid(1); // strip '#'
    m_store->add(m_editor->toPlainText(), pdfData, m_fontName, m_fontSize, colorHex);

    setStatus(QString("Compiled in %1s").arg(elapsedMs / 1000.0, 0, 'f', 2));
}

void MainWindow::onCompileFailed(const QString &error)
{
    m_isCompiling = false;
    m_typesetBtn->setEnabled(true);
    setStatus(error, true);
}

// ── Color picker ──────────────────────────────────────────────────────────────

void MainWindow::pickColor()
{
    QColor c = QColorDialog::getColor(m_textColor, this, "Text Color");
    if (!c.isValid()) return;
    m_textColor = c;
    updateColorBtn();
    if (m_autoCheck->isChecked()) m_debounce->start();
}

// ── Paste-back ────────────────────────────────────────────────────────────────

void MainWindow::pasteBack()
{
    // Use the native NSPasteboard bridge — Qt's MIME layer misses some macOS PDF types.
    QByteArray pdfBytes = macClipboardPdfData();

    if (pdfBytes.isEmpty()) {
        setStatus("No PDF found in clipboard", true);
        return;
    }

    QString tmpPdf = QDir::tempPath() + "/typstit_pasteback.pdf";
    { QFile f(tmpPdf); if (f.open(QIODevice::WriteOnly)) f.write(pdfBytes); }

    QPdfDocument doc;
    if (doc.load(tmpPdf) != QPdfDocument::Error::None || doc.pageCount() == 0) {
        setStatus("Could not open clipboard PDF", true);
        return;
    }

    QPdfSelection sel = doc.getSelectionAtIndex(0, 0, 1'000'000);
    QString text = sel.text();

    const QString marker = Preamble::sourceKeyword;
    int idx = text.indexOf(marker);
    if (idx < 0) {
        setStatus("No Typstit source found in clipboard PDF", true);
        return;
    }

    QString tail   = text.mid(idx + marker.size());
    int     endIdx = tail.indexOf('!');
    QString b64    = tail.left(endIdx < 0 ? tail.size() : endIdx)
                        .remove(QRegularExpression(R"(\s)"));

    QByteArray decoded = QByteArray::fromBase64(b64.toUtf8());
    if (decoded.isEmpty()) {
        setStatus("Source decode failed", true);
        return;
    }

    m_editor->setPlainText(QString::fromUtf8(decoded));
    setStatus("Source restored from PDF");
}

// ── History restore ───────────────────────────────────────────────────────────

void MainWindow::restoreEntry(const HistoryEntry &entry, bool compile)
{
    // Restore text
    m_editor->setPlainText(entry.source);

    // Restore font
    if (!entry.fontName.isEmpty()) {
        m_fontName = entry.fontName;
        m_fontCombo->setCurrentText(entry.fontName);
    }

    // Restore size
    m_fontSize = entry.fontSize;
    m_sizeBox->setValue(entry.fontSize);

    // Restore color
    if (!entry.colorHex.isEmpty()) {
        m_textColor = QColor("#" + entry.colorHex);
        updateColorBtn();
    }

    if (compile) triggerTypeset();
}
