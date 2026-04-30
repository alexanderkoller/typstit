#include "compiler.h"
#include <QFileInfo>
#include <QFile>
#include <QStandardPaths>

// Well-known install locations per platform (fallback before PATH search).
static QStringList typstCandidates()
{
    QStringList c;
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    c << "/opt/homebrew/bin/typst"   // Apple Silicon Homebrew
      << "/usr/local/bin/typst"      // Intel Homebrew
      << "/usr/bin/typst";
    if (const char *h = qgetenv("HOME"))
        c << QString(h) + "/.cargo/bin/typst";
#elif defined(Q_OS_WIN)
    if (const char *lad = qgetenv("LOCALAPPDATA"))
        c << QString(lad) + "\\Programs\\typst\\typst.exe";
    if (const char *up = qgetenv("USERPROFILE"))
        c << QString(up) + "\\.cargo\\bin\\typst.exe";
#else // Linux / other Unix
    c << "/usr/bin/typst"
      << "/usr/local/bin/typst";
    if (const char *h = qgetenv("HOME"))
        c << QString(h) + "/.cargo/bin/typst";
#endif
    return c;
}

Compiler::Compiler(QObject *parent) : QObject(parent) {}

void Compiler::findTypst()
{
    // 1. Check well-known paths
    for (const QString &path : typstCandidates()) {
        if (QFileInfo(path).isExecutable()) {
            m_typstPath = path;
            emit availabilityChecked(true);
            return;
        }
    }
    // 2. Search PATH (cross-platform, no subprocess needed)
    QString found = QStandardPaths::findExecutable("typst");
    if (!found.isEmpty()) {
        m_typstPath = found;
        emit availabilityChecked(true);
        return;
    }
    emit availabilityChecked(false);
}

void Compiler::compile(const QString &wrappedSource, const QString &workspacePath,
                       const QString &outputPath)
{
    if (m_typstPath.isEmpty()) {
        emit compileFailed("typst not found");
        return;
    }
    if (m_process && m_process->state() != QProcess::NotRunning) {
        return; // already compiling; caller should debounce
    }

    m_outputPath = outputPath;
    QString inputPath = workspacePath + "/input.typ";

    QFile input(inputPath);
    if (!input.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit compileFailed("Could not write " + inputPath);
        return;
    }
    input.write(wrappedSource.toUtf8());
    input.close();

    m_timer.restart();

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(workspacePath);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Compiler::onCompileFinished);
    m_process->start(m_typstPath, {"compile", inputPath, outputPath});
}

void Compiler::onCompileFinished(int exitCode, QProcess::ExitStatus)
{
    qint64  elapsed = m_timer.elapsed();
    QString errOut  = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
    m_process->deleteLater();
    m_process = nullptr;

    if (exitCode == 0) {
        QFile f(m_outputPath);
        if (f.open(QIODevice::ReadOnly)) {
            emit compileFinished(f.readAll(), elapsed);
        } else {
            emit compileFailed("Could not read output PDF");
        }
    } else {
        emit compileFailed(errOut.isEmpty()
            ? QString("Compilation failed (exit %1)").arg(exitCode)
            : errOut);
    }
}
