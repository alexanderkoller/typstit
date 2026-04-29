#pragma once
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QList>

class SyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit SyntaxHighlighter(QTextDocument *doc);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule { QRegularExpression rx; QTextCharFormat fmt; };
    QList<Rule> m_rules;
};
