#pragma once
#include <QMainWindow>
#include <QColor>

class QPlainTextEdit;
class QLabel;
class QToolButton;
class QPushButton;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QTimer;
class PreviewWidget;
class QPdfDocument;
class Compiler;
class SyntaxHighlighter;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    // UI
    QPlainTextEdit   *m_editor      = nullptr;
    PreviewWidget    *m_preview     = nullptr;
    QLabel           *m_status      = nullptr;
    QPdfDocument     *m_pdfDoc      = nullptr;
    QPushButton      *m_typesetBtn  = nullptr;
    QCheckBox        *m_autoCheck   = nullptr;
    QComboBox        *m_fontCombo   = nullptr;
    QSpinBox         *m_sizeBox     = nullptr;
    QToolButton      *m_colorBtn    = nullptr;

    // State
    QString   m_fontName   = "Libertinus Serif";
    int       m_fontSize   = 36;
    QColor    m_textColor  = Qt::black;
    QString   m_workspace;
    QString   m_outputPath;

    // Compiler
    Compiler          *m_compiler      = nullptr;
    SyntaxHighlighter *m_highlighter   = nullptr;
    QTimer            *m_debounce      = nullptr;
    bool               m_isCompiling   = false;

    QWidget *buildToolbar();
    void     setupWorkspace();
    void     triggerTypeset();
    void     onCompileFinished(const QByteArray &pdfData, qint64 elapsedMs);
    void     onCompileFailed(const QString &error);
    void     onAvailabilityChecked(bool available);
    void     onTextChanged();
    void     pickColor();
    void     pasteBack();
    void     updateColorBtn();
    void     setStatus(const QString &msg, bool error = false);
};
