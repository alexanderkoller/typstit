#pragma once
#include <QObject>
#include <QProcess>
#include <QElapsedTimer>

class Compiler : public QObject
{
    Q_OBJECT
public:
    explicit Compiler(QObject *parent = nullptr);

    void findTypst();
    void compile(const QString &wrappedSource, const QString &workspacePath,
                 const QString &outputPath);

    bool isAvailable() const { return !m_typstPath.isEmpty(); }

signals:
    void availabilityChecked(bool available);
    void compileFinished(QByteArray pdfData, qint64 elapsedMs);
    void compileFailed(QString errorMessage);

private slots:
    void onFindFinished(int exitCode, QProcess::ExitStatus);
    void onCompileFinished(int exitCode, QProcess::ExitStatus);

private:
    QString       m_typstPath;
    QString       m_outputPath;
    QProcess     *m_process = nullptr;
    QElapsedTimer m_timer;
};
