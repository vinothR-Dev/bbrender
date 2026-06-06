#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QScrollArea>
#include <QHeaderView>
#include <QPushButton>
#include <QFrame>
#include "../../backend/scheduler/scheduler.h"

namespace BBRender {

class JobDetailWidget : public QWidget {
    Q_OBJECT
public:
    explicit JobDetailWidget(Scheduler* sched, QWidget* parent = nullptr)
        : QWidget(parent), m_scheduler(sched) {
        setMinimumWidth(300);
        setStyleSheet("background:#13151a;");

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // Header
        auto* hdr = new QWidget();
        hdr->setFixedHeight(40);
        hdr->setStyleSheet("background:#1e2128; border-bottom:1px solid #2a2d36;");
        auto* hdrLay = new QHBoxLayout(hdr);
        hdrLay->setContentsMargins(12, 0, 12, 0);
        m_titleLabel = new QLabel("JOB DETAILS");
        m_titleLabel->setStyleSheet("color:#4a9eff; font-size:11px; font-weight:bold; letter-spacing:2px;");
        hdrLay->addWidget(m_titleLabel);
        hdrLay->addStretch();

        // Pause / Cancel buttons
        m_pauseBtn  = new QPushButton("⏸");
        m_cancelBtn = new QPushButton("✕");
        m_pauseBtn->setFixedSize(28, 24); m_cancelBtn->setFixedSize(28, 24);
        hdrLay->addWidget(m_pauseBtn);
        hdrLay->addWidget(m_cancelBtn);
        root->addWidget(hdr);

        auto* scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        auto* content = new QWidget();
        auto* cLay = new QVBoxLayout(content);
        cLay->setContentsMargins(12, 12, 12, 12);
        cLay->setSpacing(10);
        scroll->setWidget(content);

        // Job info section
        m_jobName = makeInfoLabel("—", 14, QColor("#e8eaf0"), true);
        cLay->addWidget(m_jobName);

        auto* infoGrid = new QWidget();
        auto* gLay = new QVBoxLayout(infoGrid);
        gLay->setSpacing(4);
        gLay->setContentsMargins(0,0,0,0);

        auto addRow = [&](const QString& key, QLabel** val) {
            auto* row = new QWidget();
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(0,0,0,0);
            auto* kl = new QLabel(key);
            kl->setStyleSheet("color:#4a4d56; font-size:10px; min-width:80px;");
            *val = new QLabel("—");
            (*val)->setStyleSheet("color:#a0a8b8; font-size:11px;");
            (*val)->setWordWrap(true);
            rl->addWidget(kl);
            rl->addWidget(*val, 1);
            gLay->addWidget(row);
        };

        addRow("Engine:",    &m_infoEngine);
        addRow("Status:",    &m_infoStatus);
        addRow("Priority:",  &m_infoPriority);
        addRow("Frames:",    &m_infoFrames);
        addRow("By:",        &m_infoUser);
        addRow("Output:",    &m_infoOutput);
        addRow("Submitted:", &m_infoSubmitted);
        cLay->addWidget(infoGrid);

        // Progress
        auto* progLabel = new QLabel("PROGRESS");
        progLabel->setStyleSheet("color:#4a4d56; font-size:10px; font-weight:bold;");
        cLay->addWidget(progLabel);

        m_progressBar = new QProgressBar();
        m_progressBar->setRange(0, 100);
        m_progressBar->setFixedHeight(8);
        m_progressBar->setTextVisible(false);
        cLay->addWidget(m_progressBar);

        m_progressLabel = new QLabel("0 / 0 tasks");
        m_progressLabel->setStyleSheet("color:#4a9eff; font-size:11px;");
        cLay->addWidget(m_progressLabel);

        // Task breakdown divider
        auto* sep = new QFrame();
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color:#2a2d36;");
        cLay->addWidget(sep);

        auto* taskLabel = new QLabel("TASKS");
        taskLabel->setStyleSheet("color:#4a4d56; font-size:10px; font-weight:bold;");
        cLay->addWidget(taskLabel);

        // Tasks table
        m_taskTable = new QTableWidget(0, 4);
        m_taskTable->setHorizontalHeaderLabels({"Frames", "Status", "Worker", "Time"});
        m_taskTable->horizontalHeader()->setStretchLastSection(true);
        m_taskTable->horizontalHeader()->setHighlightSections(false);
        m_taskTable->verticalHeader()->hide();
        m_taskTable->setShowGrid(false);
        m_taskTable->setAlternatingRowColors(true);
        m_taskTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_taskTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_taskTable->verticalHeader()->setDefaultSectionSize(28);
        m_taskTable->setColumnWidth(0, 80);
        m_taskTable->setColumnWidth(1, 80);
        m_taskTable->setColumnWidth(2, 80);
        m_taskTable->setMinimumHeight(200);
        cLay->addWidget(m_taskTable, 1);
        cLay->addStretch();

        root->addWidget(scroll, 1);

        connect(m_pauseBtn, &QPushButton::clicked, [this]{
            if (m_jobId) m_scheduler->pauseJob(m_jobId);
        });
        connect(m_cancelBtn, &QPushButton::clicked, [this]{
            if (m_jobId) m_scheduler->cancelJob(m_jobId);
        });
    }

    void showJob(const JobInfo& j) {
        m_jobId = j.id;
        m_titleLabel->setText(QString("JOB #%1").arg(j.id));
        m_jobName->setText(QString::fromStdString(j.name));
        m_infoEngine->setText(QString::fromStdString(engineStr(j.engine)));
        m_infoStatus->setText(QString::fromStdString(statusStr(j.status)));
        m_infoPriority->setText([] (Priority p) -> QString {
            switch(p) {
                case Priority::Critical:   return "🔴 Critical";
                case Priority::High:       return "🟠 High";
                case Priority::Normal:     return "🟡 Normal";
                case Priority::Low:        return "🟢 Low";
                case Priority::Background: return "⚫ Background";
            }
            return "Normal";
        }(j.priority));
        m_infoFrames->setText(QString("%1–%2 step %3 (%4 frames)")
            .arg(j.frames.start).arg(j.frames.end).arg(j.frames.step)
            .arg(j.frames.totalFrames()));
        m_infoUser->setText(QString("%1 / %2")
            .arg(QString::fromStdString(j.submittedBy))
            .arg(QString::fromStdString(j.department)));
        m_infoOutput->setText(QString::fromStdString(j.outputPath).isEmpty()
            ? "—" : QString::fromStdString(j.outputPath));

        int pct = (int)(j.progress * 100);
        m_progressBar->setValue(pct);
        m_progressLabel->setText(QString("%1 / %2 tasks (%3%)")
            .arg(j.completedTasks).arg(j.totalTasks).arg(pct));

        // Progress bar color
        QColor barColor = j.status == JobStatus::Completed ? QColor("#4ec94e")
                        : j.status == JobStatus::Failed    ? QColor("#e05555")
                        : QColor("#4a9eff");
        m_progressBar->setStyleSheet(QString(
            "QProgressBar { background:#1e2128; border:none; border-radius:4px; }"
            "QProgressBar::chunk { background:%1; border-radius:4px; }").arg(barColor.name()));

        // Tasks
        m_taskTable->setRowCount((int)j.tasks.size());
        for (int i = 0; i < (int)j.tasks.size(); ++i) {
            const auto& t = j.tasks[i];
            auto setCell = [&](int col, const QString& text, QColor fg = QColor("#a0a8b8")) {
                auto* item = new QTableWidgetItem(text);
                item->setForeground(fg);
                item->setTextAlignment(Qt::AlignCenter);
                m_taskTable->setItem(i, col, item);
            };

            setCell(0, QString("%1-%2").arg(t.frameStart).arg(t.frameEnd));

            QColor sc;
            switch (t.status) {
                case TaskStatus::Running:   sc=QColor("#4a9eff"); break;
                case TaskStatus::Completed: sc=QColor("#4ec94e"); break;
                case TaskStatus::Failed:    sc=QColor("#e05555"); break;
                case TaskStatus::Retrying:  sc=QColor("#f0a030"); break;
                default:                    sc=QColor("#6e7280"); break;
            }
            QString statusText;
            switch (t.status) {
                case TaskStatus::Pending:   statusText="Pending";   break;
                case TaskStatus::Queued:    statusText="Queued";    break;
                case TaskStatus::Running:   statusText="Running";   break;
                case TaskStatus::Completed: statusText="Done";      break;
                case TaskStatus::Failed:    statusText="Failed";    break;
                case TaskStatus::Retrying:  statusText="Retry";     break;
                case TaskStatus::Cancelled: statusText="Cancelled"; break;
            }
            setCell(1, statusText, sc);
            setCell(2, t.workerHost.empty() ? "—" : QString::fromStdString(t.workerHost));
            setCell(3, t.elapsedSec > 0
                ? QString("%1s").arg((int)t.elapsedSec) : "—");
        }
    }

private:
    QLabel* makeInfoLabel(const QString& text, int size, QColor color, bool bold = false) {
        auto* l = new QLabel(text);
        l->setStyleSheet(QString("color:%1; font-size:%2px; %3")
            .arg(color.name()).arg(size)
            .arg(bold ? "font-weight:bold;" : ""));
        l->setWordWrap(true);
        return l;
    }

    Scheduler*    m_scheduler = nullptr;
    JobID         m_jobId     = 0;
    QLabel*       m_titleLabel    = nullptr;
    QLabel*       m_jobName       = nullptr;
    QLabel*       m_infoEngine    = nullptr;
    QLabel*       m_infoStatus    = nullptr;
    QLabel*       m_infoPriority  = nullptr;
    QLabel*       m_infoFrames    = nullptr;
    QLabel*       m_infoUser      = nullptr;
    QLabel*       m_infoOutput    = nullptr;
    QLabel*       m_infoSubmitted = nullptr;
    QProgressBar* m_progressBar   = nullptr;
    QLabel*       m_progressLabel = nullptr;
    QTableWidget* m_taskTable     = nullptr;
    QPushButton*  m_pauseBtn      = nullptr;
    QPushButton*  m_cancelBtn     = nullptr;
};

} // namespace BBRender
