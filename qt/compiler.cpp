#include "compiler.h"
#include <QFileInfo>
#include <QFile>

static const QStringList kCandidates = {
    "/opt/homebrew/bin/typst",  // Apple Silicon Homebrew
    "/usr/local/bin/typst",     // Intel Homebrew
    "/usr/bin/typst",
};

Compiler::Compiler(QObject *parent) : QObject(parent) {}

void Compiler::findTypst()
{
    for (const QString &path : kCandidates) {
        if (QFileInfo(path).isExecutable()) {
            m_typstPath = path;
            emit availabilityChecked(true);
            return;
        }
    }
    // Fall back to `which typst` for custom installs / terminal launches
    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Compiler::onFindFinished);
    m_process->start("/usr/bin/env", {"which", "typst"});
}

void Compiler::onFindFinished(int exitCode, QProcess::ExitStatus)
{
    QString path = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
    m_process->deleteLater();
    m_process = nullptr;

    if (exitCode == 0 && !path.isEmpty()) {
        m_typstPath = path;
        emit availabilityChecked(true);
    } else {
        emit availabilityChecked(false);
    }
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
