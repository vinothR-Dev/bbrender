#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QScrollArea>
#include <deque>
#include <algorithm>
#include "../../shared/types/bb_types.h"

namespace BBRender {

// ─── Sparkline chart ──────────────────────────────────────────────────────────
class SparklineWidget : public QWidget {
    Q_OBJECT
    std::deque<double> m_data;
    QColor m_color;
    QString m_label;
    double m_max;
    int m_maxPoints;
public:
    SparklineWidget(const QString& label, QColor color, double max = 100.0,
                    int maxPts = 60, QWidget* parent = nullptr)
        : QWidget(parent), m_color(color), m_label(label), m_max(max), m_maxPoints(maxPts) {
        setMinimumHeight(80);
    }

    void addValue(double v) {
        m_data.push_back(v);
        if ((int)m_data.size() > m_maxPoints) m_data.pop_front();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Background
        p.fillRect(rect(), QColor("#13151a"));

        if (m_data.size() < 2) return;

        int W = width(), H = height();
        double current = m_data.back();

        // Gradient fill
        QPainterPath path;
        double xStep = (double)W / (m_maxPoints - 1);

        auto yFor = [&](double v) {
            return H - (int)((v / m_max) * (H - 20)) - 4;
        };

        int startIdx = std::max(0, (int)m_data.size() - m_maxPoints);
        path.moveTo(0, H);
        for (int i = 0; i < (int)m_data.size(); ++i) {
            double x = i * xStep;
            double y = yFor(m_data[i]);
            if (i == 0) path.lineTo(x, y);
            else path.lineTo(x, y);
        }
        path.lineTo((m_data.size() - 1) * xStep, H);
        path.closeSubpath();

        QLinearGradient grad(0, 0, 0, H);
        QColor fill = m_color; fill.setAlpha(60);
        grad.setColorAt(0.0, fill);
        grad.setColorAt(1.0, Qt::transparent);
        p.fillPath(path, grad);

        // Line
        QPainterPath line;
        for (int i = 0; i < (int)m_data.size(); ++i) {
            double x = i * xStep;
            double y = yFor(m_data[i]);
            if (i == 0) line.moveTo(x, y);
            else line.lineTo(x, y);
        }
        p.setPen(QPen(m_color, 1.5));
        p.drawPath(line);

        // Label & value
        p.setPen(QColor("#8a8f9e"));
        p.setFont(QFont("Segoe UI", 9));
        p.drawText(8, 16, m_label);

        p.setPen(m_color);
        p.setFont(QFont("Segoe UI", 16, QFont::Bold));
        p.drawText(rect().adjusted(0, 0, -8, -4),
                   Qt::AlignRight | Qt::AlignBottom,
                   QString("%1%").arg((int)current));
    }
};

// ─── KPI Tile ─────────────────────────────────────────────────────────────────
class KpiTile : public QFrame {
    Q_OBJECT
    QLabel* m_valueLabel;
    QLabel* m_labelLabel;
    QLabel* m_subLabel;
    QColor  m_accent;
public:
    KpiTile(const QString& label, const QString& sub, QColor accent, QWidget* parent = nullptr)
        : QFrame(parent), m_accent(accent) {
        setFrameShape(QFrame::NoFrame);
        setMinimumSize(180, 90);
        setStyleSheet(QString("background:#1e2128; border-radius:8px; "
                              "border-left:3px solid %1;").arg(accent.name()));

        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(14, 10, 14, 10);
        lay->setSpacing(2);

        m_valueLabel = new QLabel("0");
        m_valueLabel->setStyleSheet(QString("color:%1; font-size:28px; font-weight:bold; border:none;")
                                    .arg(accent.name()));

        m_labelLabel = new QLabel(label);
        m_labelLabel->setStyleSheet("color:#8a8f9e; font-size:11px; font-weight:bold; border:none; text-transform:uppercase;");

        m_subLabel = new QLabel(sub);
        m_subLabel->setStyleSheet("color:#4a4d56; font-size:10px; border:none;");

        lay->addWidget(m_valueLabel);
        lay->addWidget(m_labelLabel);
        lay->addWidget(m_subLabel);
    }

    void setValue(const QString& v)   { m_valueLabel->setText(v); }
    void setSub(const QString& s)     { m_subLabel->setText(s); }
};

// ─── Farm Stats Widget ────────────────────────────────────────────────────────
class FarmStatsWidget : public QWidget {
    Q_OBJECT
public:
    explicit FarmStatsWidget(QWidget* parent = nullptr) : QWidget(parent) {
        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet("QScrollArea { border:none; background:#13151a; }");

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->addWidget(scroll);

        auto* content = new QWidget();
        scroll->setWidget(content);

        auto* lay = new QVBoxLayout(content);
        lay->setContentsMargins(16, 16, 16, 16);
        lay->setSpacing(16);

        // Header
        auto* header = new QLabel("FARM DASHBOARD");
        header->setStyleSheet("color:#4a9eff; font-size:13px; font-weight:bold; letter-spacing:3px;");
        lay->addWidget(header);

        // KPI row
        auto* kpiRow = new QWidget();
        auto* kpiLayout = new QHBoxLayout(kpiRow);
        kpiLayout->setContentsMargins(0,0,0,0);
        kpiLayout->setSpacing(10);

        m_kpiWorkers    = new KpiTile("Total Workers",   "Online nodes",      QColor("#4ec94e"));
        m_kpiBusy       = new KpiTile("Rendering",       "Active tasks",      QColor("#4a9eff"));
        m_kpiJobs       = new KpiTile("Active Jobs",     "In progress",       QColor("#f0a030"));
        m_kpiFrames     = new KpiTile("Frames Today",    "Rendered 24h",      QColor("#9060ff"));
        m_kpiEfficiency = new KpiTile("Efficiency",      "Farm utilization",  QColor("#ff6080"));
        m_kpiCompleted  = new KpiTile("Completed",       "Jobs today",        QColor("#40c080"));

        kpiLayout->addWidget(m_kpiWorkers);
        kpiLayout->addWidget(m_kpiBusy);
        kpiLayout->addWidget(m_kpiJobs);
        kpiLayout->addWidget(m_kpiFrames);
        kpiLayout->addWidget(m_kpiEfficiency);
        kpiLayout->addWidget(m_kpiCompleted);
        kpiLayout->addStretch();
        lay->addWidget(kpiRow);

        // Sparklines row
        auto* chartsRow = new QWidget();
        auto* chartsLayout = new QHBoxLayout(chartsRow);
        chartsLayout->setContentsMargins(0,0,0,0);
        chartsLayout->setSpacing(10);

        m_spkCpu = new SparklineWidget("AVG CPU", QColor("#4a9eff"));
        m_spkRam = new SparklineWidget("AVG RAM", QColor("#9060ff"));
        m_spkGpu = new SparklineWidget("AVG GPU", QColor("#f0a030"));

        auto wrapChart = [&](SparklineWidget* w, const QString& title) -> QWidget* {
            auto* fr = new QFrame();
            fr->setStyleSheet("background:#1e2128; border-radius:8px;");
            fr->setMinimumHeight(110);
            auto* fl = new QVBoxLayout(fr);
            fl->setContentsMargins(0,0,0,0);
            fl->addWidget(w);
            return fr;
        };
        chartsLayout->addWidget(wrapChart(m_spkCpu, "CPU"));
        chartsLayout->addWidget(wrapChart(m_spkRam, "RAM"));
        chartsLayout->addWidget(wrapChart(m_spkGpu, "GPU"));
        lay->addWidget(chartsRow);

        // Render engine breakdown (placeholder)
        auto* engSection = new QLabel("ENGINE DISTRIBUTION");
        engSection->setStyleSheet("color:#4a9eff; font-size:11px; font-weight:bold; "
                                  "letter-spacing:2px; margin-top:8px;");
        lay->addWidget(engSection);

        auto* engRow = new QWidget();
        auto* engLayout = new QHBoxLayout(engRow);
        engLayout->setContentsMargins(0,0,0,0); engLayout->setSpacing(10);

        auto addEngBadge = [&](const QString& name, QColor c) -> QLabel* {
            auto* lb = new QLabel(name + "  0%");
            lb->setStyleSheet(QString("background:#1e2128; border-left:3px solid %1; "
                "color:#c0c4d0; font-size:11px; padding:8px 14px; border-radius:4px;")
                .arg(c.name()));
            engLayout->addWidget(lb);
            return lb;
        };
        m_engNuke    = addEngBadge("Nuke",       QColor("#ff6040"));
        m_engSil     = addEngBadge("Silhouette", QColor("#40a0ff"));
        m_engBlender = addEngBadge("Blender",    QColor("#f06030"));
        m_engHoudini = addEngBadge("Houdini",    QColor("#ffa040"));
        m_engMaya    = addEngBadge("Maya",        QColor("#40c090"));
        engLayout->addStretch();
        lay->addWidget(engRow);

        lay->addStretch();
    }

    void updateStats(const FarmStats& s) {
        m_kpiWorkers->setValue(QString("%1/%2").arg(s.busyWorkers + s.idleWorkers).arg(s.totalWorkers));
        m_kpiWorkers->setSub(QString("%1 idle").arg(s.idleWorkers));
        m_kpiBusy->setValue(QString::number(s.busyWorkers));
        m_kpiBusy->setSub(QString("%1 tasks running").arg(s.runningTasks));
        m_kpiJobs->setValue(QString::number(s.activeJobs));
        m_kpiJobs->setSub(QString("%1 queued").arg(s.pendingJobs));
        m_kpiFrames->setValue(QString::number(s.framesRenderedToday));
        m_kpiEfficiency->setValue(QString("%1%").arg((int)(s.farmEfficiency * 100)));
        m_kpiCompleted->setValue(QString::number(s.completedToday));
        m_kpiCompleted->setSub(QString("%1 failed").arg(s.failedToday));

        m_spkCpu->addValue(s.avgCpuPercent);
        m_spkRam->addValue(s.avgRamPercent);
        m_spkGpu->addValue(s.avgGpuPercent);
    }

private:
    KpiTile*        m_kpiWorkers    = nullptr;
    KpiTile*        m_kpiBusy       = nullptr;
    KpiTile*        m_kpiJobs       = nullptr;
    KpiTile*        m_kpiFrames     = nullptr;
    KpiTile*        m_kpiEfficiency = nullptr;
    KpiTile*        m_kpiCompleted  = nullptr;
    SparklineWidget* m_spkCpu = nullptr;
    SparklineWidget* m_spkRam = nullptr;
    SparklineWidget* m_spkGpu = nullptr;
    QLabel* m_engNuke = nullptr, *m_engSil = nullptr,
           *m_engBlender = nullptr, *m_engHoudini = nullptr, *m_engMaya = nullptr;
};

} // namespace BBRender
