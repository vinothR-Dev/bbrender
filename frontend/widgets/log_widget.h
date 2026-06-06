#pragma once
#include <QWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollBar>
#include <QDateTime>
#include <QColor>
#include <QTextCharFormat>

namespace BBRender {

// ─── Log Widget ───────────────────────────────────────────────────────────────
class LogWidget : public QWidget {
    Q_OBJECT
public:
    explicit LogWidget(QWidget* parent = nullptr) : QWidget(parent) {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // Toolbar
        auto* bar = new QWidget();
        bar->setFixedHeight(36);
        bar->setStyleSheet("background:#1e2128; border-bottom:1px solid #2a2d36;");
        auto* barLay = new QHBoxLayout(bar);
        barLay->setContentsMargins(8, 4, 8, 4);

        auto* title = new QLabel("SYSTEM LOG");
        title->setStyleSheet("color:#4a9eff; font-size:11px; font-weight:bold; letter-spacing:2px;");
        barLay->addWidget(title);
        barLay->addStretch();

        auto* clearBtn = new QPushButton("Clear");
        clearBtn->setFixedSize(60, 24);
        barLay->addWidget(clearBtn);
        root->addWidget(bar);

        // Log area
        m_log = new QPlainTextEdit();
        m_log->setReadOnly(true);
        m_log->setMaximumBlockCount(5000);
        m_log->setFont(QFont("Courier New", 10));
        m_log->setStyleSheet("QPlainTextEdit { background:#0d0f14; color:#a0a8b8; "
                             "border:none; padding:8px; }");
        root->addWidget(m_log);

        connect(clearBtn, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);

        appendLog("[INFO] BB Render Farm started.");
        appendLog("[INFO] Scheduler initialized with 4 threads.");
    }

    void appendLog(const QString& msg) {
        QString ts = QDateTime::currentDateTime().toString("[hh:mm:ss] ");
        QColor color = QColor("#a0a8b8");
        if (msg.contains("[ERROR]") || msg.contains("✗") || msg.contains("failed"))
            color = QColor("#e05555");
        else if (msg.contains("[WARN]"))
            color = QColor("#f0a030");
        else if (msg.contains("✓") || msg.contains("completed") || msg.contains("complete"))
            color = QColor("#4ec94e");
        else if (msg.contains("[INFO]"))
            color = QColor("#4a9eff");
        else if (msg.contains("[FARM]"))
            color = QColor("#9060ff");

        QTextCharFormat fmt;
        fmt.setForeground(color);
        auto cursor = m_log->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(ts + msg + "\n", fmt);
        m_log->setTextCursor(cursor);
        m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
    }

private:
    QPlainTextEdit* m_log = nullptr;
};

} // namespace BBRender
