#pragma once
#include <QWidget>
#include <QTableView>
#include <QAbstractTableModel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QHeaderView>
#include <vector>
#include "../../backend/scheduler/scheduler.h"

namespace BBRender {

// ─── Meter delegate (CPU/RAM/GPU bars) ───────────────────────────────────────
class MeterDelegate : public QStyledItemDelegate {
    Q_OBJECT
    int m_colCpu, m_colRam, m_colGpu;
public:
    MeterDelegate(int colCpu, int colRam, int colGpu, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_colCpu(colCpu), m_colRam(colRam), m_colGpu(colGpu) {}

    void drawMeter(QPainter* p, const QStyleOptionViewItem& opt,
                   double value, QColor color) const {
        p->save();
        if (opt.state & QStyle::State_Selected)
            p->fillRect(opt.rect, QColor("#1e3460"));
        else
            p->fillRect(opt.rect, (opt.features & QStyleOptionViewItem::Alternate)
                ? QColor("#161820") : QColor("#13151a"));

        QRect track = opt.rect.adjusted(4, 10, -4, -10);
        p->setBrush(QColor("#1e2128")); p->setPen(Qt::NoPen);
        p->drawRoundedRect(track, 3, 3);

        int w = static_cast<int>(track.width() * value / 100.0);
        if (w > 0) {
            // Color gradient: green → yellow → red
            QColor barColor = value < 70 ? QColor("#4ec94e")
                            : value < 90 ? QColor("#f0a030")
                                         : QColor("#e05555");
            QRect fill(track.x(), track.y(), w, track.height());
            p->setBrush(barColor); p->drawRoundedRect(fill, 3, 3);
        }

        p->setPen(QColor("#c0c4d0"));
        p->setFont(QFont("Segoe UI", 9));
        p->drawText(opt.rect, Qt::AlignCenter, QString("%1%").arg((int)value));
        p->restore();
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        int col = idx.column();
        if (col == m_colCpu || col == m_colRam || col == m_colGpu) {
            double v = idx.data(Qt::UserRole).toDouble();
            QColor c = col == m_colGpu ? QColor("#9060ff") : QColor("#4a9eff");
            drawMeter(p, opt, v, c);
        } else {
            QStyledItemDelegate::paint(p, opt, idx);
        }
    }
};

// ─── Worker status dot delegate ───────────────────────────────────────────────
class WorkerStatusDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit WorkerStatusDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        auto status = static_cast<WorkerStatus>(idx.data(Qt::UserRole).toInt());
        QString text; QColor dot;
        switch (status) {
            case WorkerStatus::Idle:        text="IDLE";       dot=QColor("#4ec94e"); break;
            case WorkerStatus::Rendering:   text="RENDERING";  dot=QColor("#4a9eff"); break;
            case WorkerStatus::Offline:     text="OFFLINE";    dot=QColor("#4a4d56"); break;
            case WorkerStatus::Paused:      text="PAUSED";     dot=QColor("#f0a030"); break;
            case WorkerStatus::Error:       text="ERROR";      dot=QColor("#e05555"); break;
            case WorkerStatus::Maintenance: text="MAINT";      dot=QColor("#a060ff"); break;
        }
        p->save();
        if (opt.state & QStyle::State_Selected)
            p->fillRect(opt.rect, QColor("#1e3460"));
        else
            p->fillRect(opt.rect, (opt.features & QStyleOptionViewItem::Alternate)
                ? QColor("#161820") : QColor("#13151a"));

        // Dot
        int cy = opt.rect.center().y();
        int x  = opt.rect.x() + 8;
        p->setBrush(dot); p->setPen(Qt::NoPen);
        p->drawEllipse(x, cy - 4, 8, 8);

        p->setPen(dot);
        p->setFont(QFont("Segoe UI", 9, QFont::Bold));
        p->drawText(opt.rect.adjusted(20, 0, 0, 0), Qt::AlignVCenter, text);
        p->restore();
    }
};

// ─── Worker table model ───────────────────────────────────────────────────────
class WorkerTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit WorkerTableModel(Scheduler* sched, QObject* parent = nullptr)
        : QAbstractTableModel(parent), m_scheduler(sched) {}

    void refresh() {
        beginResetModel();
        m_workers = m_scheduler->allWorkers();
        std::sort(m_workers.begin(), m_workers.end(), [](const WorkerInfo& a, const WorkerInfo& b){
            if (a.status != b.status) {
                auto rank = [](WorkerStatus s) {
                    if (s == WorkerStatus::Rendering)   return 0;
                    if (s == WorkerStatus::Idle)         return 1;
                    if (s == WorkerStatus::Paused)       return 2;
                    if (s == WorkerStatus::Error)        return 3;
                    if (s == WorkerStatus::Maintenance)  return 4;
                    return 5;
                };
                return rank(a.status) < rank(b.status);
            }
            return a.hostname < b.hostname;
        });
        endResetModel();
    }

    int rowCount(const QModelIndex&) const override { return (int)m_workers.size(); }
    int columnCount(const QModelIndex&) const override { return 11; }

    QVariant headerData(int section, Qt::Orientation orient, int role) const override {
        if (orient != Qt::Horizontal || role != Qt::DisplayRole) return {};
        static const char* h[] = {
            "Status", "Hostname", "Platform", "CPU Cores", "RAM",
            "GPU", "CPU %", "RAM %", "GPU %", "Current Job", "Rendered"
        };
        return h[section];
    }

    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid() || idx.row() >= (int)m_workers.size()) return {};
        const auto& w = m_workers[idx.row()];
        int col = idx.column();

        if (role == Qt::DisplayRole) {
            switch (col) {
                case 0: return QString::fromStdString(statusStr(w.status));
                case 1: return QString::fromStdString(w.hostname);
                case 2: {
                    QString p = QString::fromStdString(w.platform);
                    if (p == "linux")   return "🐧 Linux";
                    if (p == "windows") return "🪟 Windows";
                    if (p == "macos")   return "🍎 macOS";
                    return p;
                }
                case 3: return QString("%1 cores").arg(w.cpuCores);
                case 4: return QString("%1 GB").arg(w.ramTotalMB / 1024);
                case 5: return w.gpuCount > 0
                    ? QString("%1x %2").arg(w.gpuCount).arg(QString::fromStdString(w.gpuModel))
                    : "—";
                case 6: return QString("%1%").arg((int)w.stats.cpuPercent);
                case 7: {
                    double pct = w.ramTotalMB > 0
                        ? 100.0 * w.stats.ramUsedMB / w.ramTotalMB : 0.0;
                    return QString("%1%").arg((int)pct);
                }
                case 8: return QString("%1%").arg((int)w.stats.gpuPercent);
                case 9: {
                    if (w.currentJobId)
                        return QString("Job #%1 [%2%]")
                            .arg(w.currentJobId)
                            .arg((int)(w.currentProgress * 100));
                    return "—";
                }
                case 10: return QString("%1 frames").arg(w.totalFramesRendered);
            }
        }

        if (role == Qt::UserRole) {
            switch (col) {
                case 0:  return static_cast<int>(w.status);
                case 6:  return w.stats.cpuPercent;
                case 7:  return w.ramTotalMB > 0
                    ? 100.0 * w.stats.ramUsedMB / w.ramTotalMB : 0.0;
                case 8:  return w.stats.gpuPercent;
                default: return (qulonglong)w.id;
            }
        }

        if (role == Qt::TextAlignmentRole) {
            if (col == 3 || col == 4 || col == 6 || col == 7 || col == 8 || col == 10)
                return (int)Qt::AlignCenter;
        }

        return {};
    }

    WorkerID workerIdAt(int row) const {
        if (row < 0 || row >= (int)m_workers.size()) return 0;
        return m_workers[row].id;
    }

private:
    Scheduler* m_scheduler;
    std::vector<WorkerInfo> m_workers;
};

// ─── Worker Monitor Widget ────────────────────────────────────────────────────
class WorkerMonitorWidget : public QWidget {
    Q_OBJECT
public:
    explicit WorkerMonitorWidget(Scheduler* sched, QWidget* parent = nullptr)
        : QWidget(parent), m_scheduler(sched) {

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // Toolbar
        auto* bar = new QWidget();
        bar->setFixedHeight(44);
        bar->setStyleSheet("background:#1e2128; border-bottom:1px solid #2a2d36;");
        auto* barLayout = new QHBoxLayout(bar);
        barLayout->setContentsMargins(8, 4, 8, 4);
        barLayout->setSpacing(8);

        auto* filterCombo = new QComboBox();
        filterCombo->addItems({"All Workers", "Online", "Rendering", "Idle", "Offline", "Error"});
        filterCombo->setFixedWidth(140); filterCombo->setFixedHeight(28);
        barLayout->addWidget(filterCombo);

        barLayout->addStretch();

        // Summary labels
        auto addStat = [&](const QString& label, QColor color) -> QLabel* {
            auto* lb = new QLabel(label);
            lb->setStyleSheet(QString("color:%1; font-size:12px; font-weight:bold; padding:0 8px;")
                              .arg(color.name()));
            barLayout->addWidget(lb);
            return lb;
        };
        m_lblOnline    = addStat("0 Online",    QColor("#4ec94e"));
        m_lblBusy      = addStat("0 Rendering", QColor("#4a9eff"));
        m_lblIdle      = addStat("0 Idle",      QColor("#8a8f9e"));
        m_lblOffline   = addStat("0 Offline",   QColor("#4a4d56"));

        root->addWidget(bar);

        // Table
        m_model = new WorkerTableModel(sched, this);
        m_table = new QTableView();
        m_table->setModel(m_model);

        auto* meterDel = new MeterDelegate(6, 7, 8, this);
        auto* statusDel = new WorkerStatusDelegate(this);
        m_table->setItemDelegateForColumn(0, statusDel);
        m_table->setItemDelegateForColumn(6, meterDel);
        m_table->setItemDelegateForColumn(7, meterDel);
        m_table->setItemDelegateForColumn(8, meterDel);

        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setAlternatingRowColors(true);
        m_table->setShowGrid(false);
        m_table->verticalHeader()->hide();
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->horizontalHeader()->setHighlightSections(false);
        m_table->setSortingEnabled(false);
        m_table->verticalHeader()->setDefaultSectionSize(40);

        m_table->setColumnWidth(0, 100);
        m_table->setColumnWidth(1, 160);
        m_table->setColumnWidth(2, 90);
        m_table->setColumnWidth(3, 90);
        m_table->setColumnWidth(4, 70);
        m_table->setColumnWidth(5, 200);
        m_table->setColumnWidth(6, 90);
        m_table->setColumnWidth(7, 90);
        m_table->setColumnWidth(8, 90);
        m_table->setColumnWidth(9, 160);

        root->addWidget(m_table);

        connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged,
                this, [this](const QModelIndex& curr, const QModelIndex&) {
                    WorkerID id = m_model->workerIdAt(curr.row());
                    if (id) emit workerSelected(id);
                });
    }

    void refresh() {
        m_model->refresh();
        auto workers = m_scheduler->allWorkers();
        int online = 0, busy = 0, idle = 0, offline = 0;
        for (auto& w : workers) {
            if (w.status == WorkerStatus::Offline) ++offline;
            else { ++online;
                if (w.status == WorkerStatus::Rendering) ++busy;
                else if (w.status == WorkerStatus::Idle) ++idle;
            }
        }
        m_lblOnline->setText(QString("%1 Online").arg(online));
        m_lblBusy->setText(QString("%1 Rendering").arg(busy));
        m_lblIdle->setText(QString("%1 Idle").arg(idle));
        m_lblOffline->setText(QString("%1 Offline").arg(offline));
    }

signals:
    void workerSelected(WorkerID id);

private:
    Scheduler*         m_scheduler;
    WorkerTableModel*  m_model  = nullptr;
    QTableView*        m_table  = nullptr;
    QLabel* m_lblOnline  = nullptr;
    QLabel* m_lblBusy    = nullptr;
    QLabel* m_lblIdle    = nullptr;
    QLabel* m_lblOffline = nullptr;
};

} // namespace BBRender
