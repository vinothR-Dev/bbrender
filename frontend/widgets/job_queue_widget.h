#pragma once
#include <QWidget>
#include <QTableView>
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QMenu>
#include <QAction>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QHeaderView>
#include <QDateTime>
#include <QTimer>
#include <vector>
#include "../../backend/scheduler/scheduler.h"

namespace BBRender {

// ─── Progress bar delegate ────────────────────────────────────────────────────
class ProgressDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ProgressDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        if (idx.column() == 5) { // Progress column
            float progress = idx.data(Qt::UserRole).toFloat();
            auto  status   = static_cast<JobStatus>(idx.data(Qt::UserRole + 1).toInt());

            p->save();
            // Background
            p->fillRect(opt.rect.adjusted(4, 6, -4, -6), QColor("#1e2128"));

            // Bar color by status
            QColor barColor;
            switch (status) {
                case JobStatus::Running:   barColor = QColor("#4a9eff"); break;
                case JobStatus::Completed: barColor = QColor("#4ec94e"); break;
                case JobStatus::Failed:    barColor = QColor("#e05555"); break;
                case JobStatus::Paused:    barColor = QColor("#f0a030"); break;
                default:                   barColor = QColor("#3a3d46"); break;
            }

            QRect bar = opt.rect.adjusted(4, 6, -4, -6);
            int w = static_cast<int>(bar.width() * progress);
            QRect filled(bar.x(), bar.y(), w, bar.height());
            p->fillRect(filled, barColor);

            // Percentage text
            p->setPen(QColor("#e8eaf0"));
            p->setFont(QFont("Segoe UI", 9));
            p->drawText(opt.rect, Qt::AlignCenter,
                        QString("%1%").arg(static_cast<int>(progress * 100)));
            p->restore();
        } else {
            QStyledItemDelegate::paint(p, opt, idx);
        }
    }
};

// ─── Status badge delegate ────────────────────────────────────────────────────
class StatusDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit StatusDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        if (idx.column() == 3) {
            auto status = static_cast<JobStatus>(idx.data(Qt::UserRole).toInt());
            QString text = QString::fromStdString(statusStr(status));

            QColor bg, fg;
            switch (status) {
                case JobStatus::Running:   bg = QColor("#1a3a6a"); fg = QColor("#4a9eff"); break;
                case JobStatus::Completed: bg = QColor("#1a3a1a"); fg = QColor("#4ec94e"); break;
                case JobStatus::Failed:    bg = QColor("#3a1a1a"); fg = QColor("#e05555"); break;
                case JobStatus::Paused:    bg = QColor("#3a2a0a"); fg = QColor("#f0a030"); break;
                case JobStatus::Queued:    bg = QColor("#1e2840"); fg = QColor("#8ab8ff"); break;
                case JobStatus::Cancelled: bg = QColor("#252830"); fg = QColor("#6e7280"); break;
                default:                   bg = QColor("#1e2128"); fg = QColor("#8a8f9e"); break;
            }

            p->save();
            if (opt.state & QStyle::State_Selected)
                p->fillRect(opt.rect, QColor("#1e3460"));
            else
                p->fillRect(opt.rect, QColor("#13151a"));

            QRect badge = opt.rect.adjusted(6, 5, -6, -5);
            p->setBrush(bg);
            p->setPen(Qt::NoPen);
            p->drawRoundedRect(badge, 3, 3);

            p->setPen(fg);
            p->setFont(QFont("Segoe UI", 9, QFont::Bold));
            p->drawText(badge, Qt::AlignCenter, text.toUpper());
            p->restore();
        } else {
            QStyledItemDelegate::paint(p, opt, idx);
        }
    }
};

// ─── Job table model ──────────────────────────────────────────────────────────
class JobTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit JobTableModel(Scheduler* sched, QObject* parent = nullptr)
        : QAbstractTableModel(parent), m_scheduler(sched) {}

    void refresh() {
        beginResetModel();
        m_jobs = m_scheduler->allJobs();
        // Sort: running first, then queued, then by priority
        std::sort(m_jobs.begin(), m_jobs.end(), [](const JobInfo& a, const JobInfo& b) {
            auto rank = [](JobStatus s) {
                switch(s) {
                    case JobStatus::Running:   return 0;
                    case JobStatus::Queued:    return 1;
                    case JobStatus::Paused:    return 2;
                    case JobStatus::Pending:   return 3;
                    case JobStatus::Failed:    return 4;
                    case JobStatus::Completed: return 5;
                    default: return 6;
                }
            };
            if (rank(a.status) != rank(b.status)) return rank(a.status) < rank(b.status);
            return static_cast<int>(a.priority) < static_cast<int>(b.priority);
        });
        endResetModel();
    }

    int rowCount(const QModelIndex&) const override { return (int)m_jobs.size(); }
    int columnCount(const QModelIndex&) const override { return 10; }

    QVariant headerData(int section, Qt::Orientation orient, int role) const override {
        if (orient != Qt::Horizontal || role != Qt::DisplayRole) return {};
        static const char* headers[] = {
            "ID", "Job Name", "Engine", "Status", "Priority",
            "Progress", "Frames", "Worker", "ETA", "Submitted"
        };
        return headers[section];
    }

    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid() || idx.row() >= (int)m_jobs.size()) return {};
        const auto& j = m_jobs[idx.row()];

        if (role == Qt::DisplayRole) {
            switch (idx.column()) {
                case 0: return QString("#%1").arg(j.id);
                case 1: return QString::fromStdString(j.name);
                case 2: return QString::fromStdString(engineStr(j.engine));
                case 3: return QString::fromStdString(statusStr(j.status));
                case 4: {
                    static const char* p[] = {"CRITICAL","HIGH","NORMAL","LOW","BG"};
                    return p[static_cast<int>(j.priority)];
                }
                case 5: return QString("%1%").arg((int)(j.progress * 100));
                case 6: return QString("%1-%2 (%3f)")
                    .arg(j.frames.start).arg(j.frames.end)
                    .arg(j.frames.totalFrames());
                case 7: return j.runningTasks > 0
                    ? QString("%1 workers").arg(j.runningTasks) : "-";
                case 8: {
                    if (j.status == JobStatus::Running && j.etaSec > 0) {
                        int s = (int)j.etaSec;
                        return QString("%1h %2m").arg(s/3600).arg((s%3600)/60);
                    }
                    return "-";
                }
                case 9: return QString::fromStdString(j.submittedBy) + " / " +
                               QString::fromStdString(j.department);
            }
        }

        if (role == Qt::UserRole) {
            if (idx.column() == 5) return j.progress;
            if (idx.column() == 3) return static_cast<int>(j.status);
            return (qulonglong)j.id;
        }
        if (role == Qt::UserRole + 1 && idx.column() == 5)
            return static_cast<int>(j.status);

        if (role == Qt::ForegroundRole) {
            switch (idx.column()) {
                case 4:
                    switch (j.priority) {
                        case Priority::Critical: return QColor("#ff5555");
                        case Priority::High:     return QColor("#f0a030");
                        case Priority::Normal:   return QColor("#c0c4d0");
                        case Priority::Low:      return QColor("#6e7280");
                        case Priority::Background: return QColor("#4a4d56");
                    }
                default: return QColor("#c0c4d0");
            }
        }

        if (role == Qt::TextAlignmentRole) {
            if (idx.column() == 0) return (int)(Qt::AlignRight | Qt::AlignVCenter);
            if (idx.column() == 5) return (int)Qt::AlignCenter;
        }

        return {};
    }

    JobID jobIdAt(int row) const {
        if (row < 0 || row >= (int)m_jobs.size()) return 0;
        return m_jobs[row].id;
    }

    const JobInfo* jobAt(int row) const {
        if (row < 0 || row >= (int)m_jobs.size()) return nullptr;
        return &m_jobs[row];
    }

private:
    Scheduler* m_scheduler;
    std::vector<JobInfo> m_jobs;
};

// ─── Job Queue Widget ─────────────────────────────────────────────────────────
class JobQueueWidget : public QWidget {
    Q_OBJECT
public:
    explicit JobQueueWidget(Scheduler* sched, QWidget* parent = nullptr)
        : QWidget(parent), m_scheduler(sched) {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // Toolbar row
        auto* bar = new QWidget();
        bar->setFixedHeight(44);
        bar->setStyleSheet("background:#1e2128; border-bottom:1px solid #2a2d36;");
        auto* barLayout = new QHBoxLayout(bar);
        barLayout->setContentsMargins(8, 4, 8, 4);
        barLayout->setSpacing(8);

        // Search
        auto* searchEdit = new QLineEdit();
        searchEdit->setPlaceholderText("  🔍  Search jobs...");
        searchEdit->setFixedWidth(260);
        searchEdit->setFixedHeight(28);

        // Filter by status
        auto* statusFilter = new QComboBox();
        statusFilter->addItems({"All Status", "Running", "Queued", "Paused", "Completed", "Failed"});
        statusFilter->setFixedWidth(130);
        statusFilter->setFixedHeight(28);

        // Filter by engine
        auto* engineFilter = new QComboBox();
        engineFilter->addItems({"All Engines", "Nuke", "Silhouette", "Blender", "Houdini", "Maya"});
        engineFilter->setFixedWidth(130);
        engineFilter->setFixedHeight(28);

        barLayout->addWidget(searchEdit);
        barLayout->addWidget(statusFilter);
        barLayout->addWidget(engineFilter);
        barLayout->addStretch();

        // Queue stats badges
        auto addBadge = [&](const QString& label, const QString& color) -> QLabel* {
            auto* lb = new QLabel(label);
            lb->setStyleSheet(QString("background:%1; color:white; "
                "border-radius:10px; padding:2px 10px; font-size:10px; font-weight:bold;").arg(color));
            barLayout->addWidget(lb);
            return lb;
        };
        m_badgeRunning   = addBadge("0 Running",   "#2a5fa5");
        m_badgeQueued    = addBadge("0 Queued",    "#3a4a6a");
        m_badgeCompleted = addBadge("0 Done",      "#1a4a1a");
        m_badgeFailed    = addBadge("0 Failed",    "#5a1a1a");

        root->addWidget(bar);

        // Table
        m_model = new JobTableModel(sched, this);

        m_proxy = new QSortFilterProxyModel(this);
        m_proxy->setSourceModel(m_model);
        m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_proxy->setFilterKeyColumn(-1); // all columns

        m_table = new QTableView();
        m_table->setModel(m_proxy);
        m_table->setItemDelegateForColumn(3, new StatusDelegate(this));
        m_table->setItemDelegateForColumn(5, new ProgressDelegate(this));
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setAlternatingRowColors(true);
        m_table->setShowGrid(false);
        m_table->verticalHeader()->hide();
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->horizontalHeader()->setHighlightSections(false);
        m_table->setSortingEnabled(true);
        m_table->setContextMenuPolicy(Qt::CustomContextMenu);
        m_table->setWordWrap(false);

        // Column widths
        m_table->setColumnWidth(0, 60);
        m_table->setColumnWidth(1, 220);
        m_table->setColumnWidth(2, 100);
        m_table->setColumnWidth(3, 110);
        m_table->setColumnWidth(4, 80);
        m_table->setColumnWidth(5, 130);
        m_table->setColumnWidth(6, 120);
        m_table->setColumnWidth(7, 100);
        m_table->setColumnWidth(8, 80);
        m_table->verticalHeader()->setDefaultSectionSize(36);

        root->addWidget(m_table);

        // Connections
        connect(searchEdit, &QLineEdit::textChanged,
                m_proxy,    &QSortFilterProxyModel::setFilterFixedString);
        connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged,
                this, [this](const QModelIndex& curr, const QModelIndex&) {
                    auto src = m_proxy->mapToSource(curr);
                    JobID id = m_model->jobIdAt(src.row());
                    if (id) emit jobSelected(id);
                });
        connect(m_table, &QTableView::customContextMenuRequested,
                this, &JobQueueWidget::onContextMenu);
    }

    void refresh() {
        m_model->refresh();

        // Update badges
        int running = 0, queued = 0, completed = 0, failed = 0;
        auto jobs = m_scheduler->allJobs();
        for (auto& j : jobs) {
            switch (j.status) {
                case JobStatus::Running:   ++running;   break;
                case JobStatus::Queued:    ++queued;    break;
                case JobStatus::Completed: ++completed; break;
                case JobStatus::Failed:    ++failed;    break;
                default: break;
            }
        }
        m_badgeRunning->setText(QString("%1 Running").arg(running));
        m_badgeQueued->setText(QString("%1 Queued").arg(queued));
        m_badgeCompleted->setText(QString("%1 Done").arg(completed));
        m_badgeFailed->setText(QString("%1 Failed").arg(failed));
    }

signals:
    void jobSelected(JobID id);

private slots:
    void onContextMenu(const QPoint& pos) {
        auto idx = m_table->indexAt(pos);
        if (!idx.isValid()) return;
        auto src = m_proxy->mapToSource(idx);
        const JobInfo* job = m_model->jobAt(src.row());
        if (!job) return;

        QMenu menu(this);
        menu.setStyleSheet("QMenu { background:#1e2128; color:#c0c4d0; border:1px solid #3a3d46; }"
                           "QMenu::item:selected { background:#2a6496; }");

        auto* titleAct = menu.addAction(QString("Job #%1: %2").arg(job->id)
                             .arg(QString::fromStdString(job->name)));
        titleAct->setEnabled(false);
        menu.addSeparator();

        if (job->status == JobStatus::Running || job->status == JobStatus::Queued) {
            menu.addAction("⏸  Pause Job", [this, id=job->id]{
                m_scheduler->pauseJob(id); refresh();
            });
            menu.addAction("✕  Cancel Job", [this, id=job->id]{
                m_scheduler->cancelJob(id); refresh();
            });
        }
        if (job->status == JobStatus::Paused) {
            menu.addAction("▶  Resume Job", [this, id=job->id]{
                m_scheduler->resumeJob(id); refresh();
            });
        }
        if (job->status == JobStatus::Failed) {
            menu.addAction("↩  Retry Failed Tasks", [this, id=job->id]{
                m_scheduler->retryFailedTasks(id); refresh();
            });
        }
        menu.addSeparator();

        auto* prioMenu = menu.addMenu("⚡  Set Priority");
        for (auto [name, p] : std::vector<std::pair<QString,Priority>>{
            {"Critical", Priority::Critical},{"High",Priority::High},
            {"Normal",Priority::Normal},{"Low",Priority::Low},{"Background",Priority::Background}
        }) {
            prioMenu->addAction(name, [this, id=job->id, p]{
                m_scheduler->reprioritizeJob(id, p); refresh();
            });
        }
        menu.addSeparator();
        menu.addAction("📋  View Details", [this, id=job->id]{ emit jobSelected(id); });

        menu.exec(m_table->viewport()->mapToGlobal(pos));
    }

private:
    Scheduler*              m_scheduler;
    JobTableModel*          m_model  = nullptr;
    QSortFilterProxyModel*  m_proxy  = nullptr;
    QTableView*             m_table  = nullptr;
    QLabel* m_badgeRunning   = nullptr;
    QLabel* m_badgeQueued    = nullptr;
    QLabel* m_badgeCompleted = nullptr;
    QLabel* m_badgeFailed    = nullptr;
};

} // namespace BBRender
