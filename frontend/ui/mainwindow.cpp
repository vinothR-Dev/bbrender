#include "mainwindow.h"
#include "../widgets/job_queue_widget.h"
#include "../widgets/worker_monitor_widget.h"
#include "../widgets/farm_stats_widget.h"
#include "../widgets/log_widget.h"
#include "../widgets/job_detail_widget.h"
#include "../dialogs/job_submit_dialog.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QAction>
#include <QPushButton>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>
#include <QFont>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QPixmap>

namespace BBRender {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("BB Render Farm v1.0");
    setMinimumSize(1280, 800);
    resize(1600, 960);

    // Init backend
    m_scheduler = std::make_unique<Scheduler>(4);
    m_server    = std::make_unique<NetworkServer>(*m_scheduler, 9876);

    // Scheduler event wiring (thread-safe via queued connection)
    m_scheduler->addEventCallback([this](const SchedulerEvent& ev){
        emit schedulerEventReceived(ev);
    });

    applyDarkTheme();
    setupUI();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupTray();
    setupConnections();
    loadSettings();

    m_scheduler->start();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(1000);
    m_refreshTimer->start();

    // Add demo workers for UI preview
    {
        WorkerInfo w1;
        w1.hostname = "render-node-01"; w1.cpuCores = 32; w1.ramTotalMB = 65536;
        w1.gpuCount = 2; w1.gpuModel = "NVIDIA RTX 4090"; w1.vramMB = 24576;
        w1.platform = "linux"; w1.cpuModel = "AMD Threadripper 3970X";
        m_scheduler->registerWorker(w1);

        WorkerInfo w2;
        w2.hostname = "render-node-02"; w2.cpuCores = 64; w2.ramTotalMB = 131072;
        w2.gpuCount = 4; w2.gpuModel = "NVIDIA A100 80GB"; w2.vramMB = 81920;
        w2.platform = "linux"; w2.cpuModel = "Intel Xeon W9-3595X";
        m_scheduler->registerWorker(w2);

        WorkerInfo w3;
        w3.hostname = "win-render-01"; w3.cpuCores = 16; w3.ramTotalMB = 32768;
        w3.gpuCount = 1; w3.gpuModel = "NVIDIA RTX 3080"; w3.vramMB = 10240;
        w3.platform = "windows"; w3.cpuModel = "Intel Core i9-13900K";
        m_scheduler->registerWorker(w3);
    }
}

MainWindow::~MainWindow() {
    saveSettings();
    if (m_serverRunning) m_server->stop();
    m_scheduler->stop();
}

void MainWindow::setupUI() {
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Main horizontal splitter: left panel + right detail
    m_mainSplitter = new QSplitter(Qt::Horizontal, central);
    m_mainSplitter->setHandleWidth(2);

    // ── Left: tabs ───────────────────────────────────────────────────────────
    m_tabs = new QTabWidget(m_mainSplitter);
    m_tabs->setTabPosition(QTabWidget::South);
    m_tabs->setDocumentMode(true);

    m_jobQueue      = new JobQueueWidget(m_scheduler.get(), m_tabs);
    m_workerMonitor = new WorkerMonitorWidget(m_scheduler.get(), m_tabs);
    m_statsWidget   = new FarmStatsWidget(m_tabs);
    m_logWidget     = new LogWidget(m_tabs);

    m_tabs->addTab(m_jobQueue,      QIcon(), "  Job Queue  ");
    m_tabs->addTab(m_workerMonitor, QIcon(), "  Workers  ");
    m_tabs->addTab(m_statsWidget,   QIcon(), "  Dashboard  ");
    m_tabs->addTab(m_logWidget,     QIcon(), "  Log  ");

    // Style tabs
    m_tabs->setStyleSheet(R"(
        QTabBar::tab {
            background: #1e2128;
            color: #8a8f9e;
            padding: 8px 20px;
            font-size: 12px;
            border: none;
            border-top: 2px solid transparent;
        }
        QTabBar::tab:selected {
            background: #252830;
            color: #e8eaf0;
            border-top: 2px solid #4a9eff;
        }
        QTabBar::tab:hover:!selected { background: #22252d; color: #c0c4d0; }
        QTabWidget::pane { border: none; }
    )");

    // ── Right: job detail panel ────────────────────────────────────────────
    m_jobDetail = new JobDetailWidget(m_scheduler.get(), m_mainSplitter);

    m_mainSplitter->addWidget(m_tabs);
    m_mainSplitter->addWidget(m_jobDetail);
    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setSizes({1100, 400});

    root->addWidget(m_mainSplitter);
}

void MainWindow::setupMenuBar() {
    auto* mb = menuBar();
    mb->setStyleSheet("QMenuBar { background:#1a1d24; color:#c0c4d0; padding: 2px; }"
                      "QMenuBar::item:selected { background:#2a2d36; }"
                      "QMenu { background:#1e2128; color:#c0c4d0; border:1px solid #3a3d46; }"
                      "QMenu::item:selected { background:#2a6496; }");

    // Farm menu
    auto* farmMenu = mb->addMenu("&Farm");
    farmMenu->addAction(QIcon(), "&Submit Job...",    this, &MainWindow::onSubmitJob,    QKeySequence("Ctrl+N"));
    farmMenu->addSeparator();
    farmMenu->addAction(QIcon(), "Start &Server",     this, &MainWindow::onStartServer,  QKeySequence("Ctrl+Shift+S"));
    farmMenu->addAction(QIcon(), "Sto&p Server",      this, &MainWindow::onStopServer,   QKeySequence("Ctrl+Shift+P"));
    farmMenu->addAction(QIcon(), "&Connect to Farm",  this, &MainWindow::onConnectToServer);
    farmMenu->addSeparator();
    farmMenu->addAction(QIcon(), "&Preferences...",   this, &MainWindow::onPreferences,  QKeySequence("Ctrl+,"));
    farmMenu->addSeparator();
    farmMenu->addAction("E&xit", qApp, &QApplication::quit, QKeySequence("Ctrl+Q"));

    // View menu
    auto* viewMenu = mb->addMenu("&View");
    viewMenu->addAction("Job Queue",    [this]{ m_tabs->setCurrentIndex(0); });
    viewMenu->addAction("Workers",      [this]{ m_tabs->setCurrentIndex(1); });
    viewMenu->addAction("Dashboard",    [this]{ m_tabs->setCurrentIndex(2); });
    viewMenu->addAction("Log",          [this]{ m_tabs->setCurrentIndex(3); });

    // Help menu
    auto* helpMenu = mb->addMenu("&Help");
    helpMenu->addAction("&About BB Render", this, &MainWindow::onAbout);
}

void MainWindow::setupToolBar() {
    m_toolbar = addToolBar("Main");
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(20, 20));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toolbar->setStyleSheet(R"(
        QToolBar {
            background: #1a1d24;
            border-bottom: 1px solid #2a2d36;
            padding: 4px 8px;
            spacing: 4px;
        }
        QToolButton {
            background: #252830;
            color: #c0c4d0;
            border: 1px solid #3a3d46;
            border-radius: 4px;
            padding: 5px 12px;
            font-size: 12px;
        }
        QToolButton:hover  { background: #2e3240; border-color: #4a9eff; color: #e8eaf0; }
        QToolButton:pressed { background: #1e2440; }
    )");

    // Brand label
    auto* brand = new QLabel("  ⬛ BB RENDER  ");
    brand->setStyleSheet("color:#4a9eff; font-size:16px; font-weight:bold; "
                         "letter-spacing:2px; padding-right:16px;");
    m_toolbar->addWidget(brand);
    m_toolbar->addSeparator();

    // Submit button - prominent green
    m_actSubmit = m_toolbar->addAction("➕  Submit Job");
    m_actSubmit->setShortcut(QKeySequence("Ctrl+N"));
    m_actSubmit->setToolTip("Submit a new render job (Ctrl+N)");

    m_toolbar->addSeparator();

    m_actStart = m_toolbar->addAction("▶  Start Server");
    m_actStop  = m_toolbar->addAction("⏹  Stop Server");
    m_actStop->setEnabled(false);

    m_toolbar->addSeparator();
    m_actRefresh = m_toolbar->addAction("↻  Refresh");
    m_actRefresh->setShortcut(QKeySequence("F5"));

    // Spacer
    auto* spacer = new QWidget(); spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    // Server status indicator
    auto* serverLabel = new QLabel("  ● SERVER OFFLINE  ");
    serverLabel->setObjectName("serverLabel");
    serverLabel->setStyleSheet("color:#e05555; font-size:11px; font-weight:bold;");
    m_toolbar->addWidget(serverLabel);
    m_statusServer = serverLabel;
}

void MainWindow::setupStatusBar() {
    auto* sb = statusBar();
    sb->setStyleSheet("QStatusBar { background:#13151a; color:#6e7280; font-size:11px; border-top:1px solid #2a2d36; }"
                      "QStatusBar::item { border: none; }");

    m_statusWorkers    = new QLabel("Workers: 0/0");
    m_statusJobs       = new QLabel("Jobs: 0 active");
    m_statusFPS        = new QLabel("Frames: 0/day");
    m_statusEfficiency = new QLabel("Efficiency: 0%");

    auto style = "padding: 0 12px; color: #8a8f9e;";
    m_statusWorkers->setStyleSheet(style);
    m_statusJobs->setStyleSheet(style);
    m_statusFPS->setStyleSheet(style);
    m_statusEfficiency->setStyleSheet(style);

    sb->addWidget(m_statusWorkers);
    sb->addWidget(new QLabel("|")); // separator
    sb->addWidget(m_statusJobs);
    sb->addWidget(new QLabel("|"));
    sb->addWidget(m_statusFPS);
    sb->addWidget(new QLabel("|"));
    sb->addWidget(m_statusEfficiency);
    sb->addPermanentWidget(new QLabel("BB Render Farm v1.0.0  "));
}

void MainWindow::setupTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    m_trayIcon = new QSystemTrayIcon(this);

    QPixmap px(32, 32); px.fill(QColor("#4a9eff"));
    m_trayIcon->setIcon(QIcon(px));
    m_trayIcon->setToolTip("BB Render Farm");

    auto* trayMenu = new QMenu(this);
    trayMenu->addAction("Show",   this, &MainWindow::show);
    trayMenu->addAction("Submit", this, &MainWindow::onSubmitJob);
    trayMenu->addSeparator();
    trayMenu->addAction("Quit",   qApp, &QApplication::quit);
    m_trayIcon->setContextMenu(trayMenu);
    m_trayIcon->show();
}

void MainWindow::setupConnections() {
    connect(m_actSubmit,  &QAction::triggered, this, &MainWindow::onSubmitJob);
    connect(m_actStart,   &QAction::triggered, this, &MainWindow::onStartServer);
    connect(m_actStop,    &QAction::triggered, this, &MainWindow::onStopServer);
    connect(m_actRefresh, &QAction::triggered, this, &MainWindow::onRefreshFarm);
    connect(m_refreshTimer, &QTimer::timeout,  this, &MainWindow::onTimerTick);

    if (m_trayIcon)
        connect(m_trayIcon, &QSystemTrayIcon::activated,
                this, &MainWindow::onTrayActivated);

    // Cross-thread event from scheduler
    connect(this, &MainWindow::schedulerEventReceived,
            this, &MainWindow::onSchedulerEvent, Qt::QueuedConnection);

    // Job selection from queue
    connect(m_jobQueue, &JobQueueWidget::jobSelected,
            this, &MainWindow::onJobSelected);
    connect(m_workerMonitor, &WorkerMonitorWidget::workerSelected,
            this, &MainWindow::onWorkerSelected);
}

void MainWindow::onTimerTick() {
    auto stats = m_scheduler->getFarmStats();
    updateStatusBar(stats);
    m_statsWidget->updateStats(stats);
    m_jobQueue->refresh();
    m_workerMonitor->refresh();
    emit farmStatsUpdated(stats);
}

void MainWindow::updateStatusBar(const FarmStats& s) {
    m_statusWorkers->setText(QString("Workers: %1/%2 busy")
        .arg(s.busyWorkers).arg(s.onlineWorkers));
    m_statusJobs->setText(QString("Jobs: %1 active, %2 pending")
        .arg(s.activeJobs).arg(s.pendingJobs));
    m_statusFPS->setText(QString("Frames: %1/day")
        .arg(s.framesRenderedToday));
    m_statusEfficiency->setText(QString("Efficiency: %1%")
        .arg(static_cast<int>(s.farmEfficiency * 100)));
}

void MainWindow::onSubmitJob() {
    JobSubmitDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        auto job = dlg.getJobInfo();
        JobID id = m_scheduler->submitJob(job);
        m_logWidget->appendLog(QString("[INFO] Job submitted: %1 (id=%2)")
            .arg(QString::fromStdString(job.name)).arg(id));
        m_tabs->setCurrentIndex(0); // Switch to job queue
    }
}

void MainWindow::onRefreshFarm() {
    m_jobQueue->refresh();
    m_workerMonitor->refresh();
}

void MainWindow::onJobSelected(JobID id) {
    m_selectedJob = id;
    auto opt = m_scheduler->getJob(id);
    if (opt) m_jobDetail->showJob(*opt);
}

void MainWindow::onWorkerSelected(WorkerID id) {
    // Could show worker detail panel
    Q_UNUSED(id);
}

void MainWindow::onStartServer() {
    if (m_server->start()) {
        m_serverRunning = true;
        m_actStart->setEnabled(false);
        m_actStop->setEnabled(true);
        m_statusServer->setText("  ● SERVER ONLINE :9876  ");
        m_statusServer->setStyleSheet("color:#4ec94e; font-size:11px; font-weight:bold;");
        m_logWidget->appendLog("[INFO] BB Render Server started on port 9876");
        if (m_trayIcon)
            m_trayIcon->showMessage("BB Render", "Farm server started on port 9876",
                                    QSystemTrayIcon::Information, 3000);
    } else {
        QMessageBox::critical(this, "Server Error", "Failed to start the render server.\nCheck port 9876 availability.");
    }
}

void MainWindow::onStopServer() {
    m_server->stop();
    m_serverRunning = false;
    m_actStart->setEnabled(true);
    m_actStop->setEnabled(false);
    m_statusServer->setText("  ● SERVER OFFLINE  ");
    m_statusServer->setStyleSheet("color:#e05555; font-size:11px; font-weight:bold;");
    m_logWidget->appendLog("[INFO] Server stopped");
}

void MainWindow::onConnectToServer() {
    // Opens a connect-to-farm dialog (remote mode)
    QMessageBox::information(this, "Connect to Farm",
        "Enter remote server address in Preferences > Farm Server.");
}

void MainWindow::onSchedulerEvent(const SchedulerEvent& ev) {
    QString msg;
    switch (ev.type) {
        case SchedulerEvent::Type::JobAdded:
            msg = QString("[FARM] Job #%1 added to queue").arg(ev.jobId); break;
        case SchedulerEvent::Type::JobStarted:
            msg = QString("[FARM] Job #%1 started rendering").arg(ev.jobId); break;
        case SchedulerEvent::Type::JobCompleted:
            msg = QString("[FARM] ✓ Job #%1 completed").arg(ev.jobId); break;
        case SchedulerEvent::Type::JobFailed:
            msg = QString("[FARM] ✗ Job #%1 failed").arg(ev.jobId); break;
        case SchedulerEvent::Type::WorkerJoined:
            msg = QString("[FARM] Worker #%1 connected").arg(ev.workerId); break;
        case SchedulerEvent::Type::WorkerLeft:
            msg = QString("[FARM] Worker #%1 disconnected").arg(ev.workerId); break;
        case SchedulerEvent::Type::TaskAssigned:
            msg = QString("[FARM] Task #%1 assigned to worker #%2").arg(ev.taskId).arg(ev.workerId); break;
        case SchedulerEvent::Type::TaskCompleted:
            msg = QString("[FARM] Task #%1 completed").arg(ev.taskId); break;
        case SchedulerEvent::Type::TaskFailed:
            msg = QString("[FARM] Task #%1 failed").arg(ev.taskId); break;
        default: return;
    }
    m_logWidget->appendLog(msg);
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) {
        isVisible() ? hide() : show();
    }
}

void MainWindow::onPreferences() {
    QMessageBox::information(this, "Preferences", "Preferences dialog coming soon.");
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "About BB Render Farm",
        "<h2>BB Render Farm v1.0.0</h2>"
        "<p>High-performance distributed render farm manager.</p>"
        "<p>Supports: <b>Nuke, Silhouette, Blender, Houdini, Maya, Cinema4D</b></p>"
        "<p>Platform: Windows / Linux / macOS</p>"
        "<p>Built with Qt6 + C++20</p>"
        "<hr><p style='color:gray'>© 2025 BB Render. All rights reserved.</p>");
}

void MainWindow::applyDarkTheme() {
    qApp->setStyle(QStyleFactory::create("Fusion"));
    QPalette p;
    p.setColor(QPalette::Window,          QColor("#1a1d24"));
    p.setColor(QPalette::WindowText,      QColor("#c0c4d0"));
    p.setColor(QPalette::Base,            QColor("#13151a"));
    p.setColor(QPalette::AlternateBase,   QColor("#1e2128"));
    p.setColor(QPalette::Text,            QColor("#c0c4d0"));
    p.setColor(QPalette::Button,          QColor("#252830"));
    p.setColor(QPalette::ButtonText,      QColor("#c0c4d0"));
    p.setColor(QPalette::Highlight,       QColor("#2a5fa5"));
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::Link,            QColor("#4a9eff"));
    p.setColor(QPalette::ToolTipBase,     QColor("#252830"));
    p.setColor(QPalette::ToolTipText,     QColor("#e0e4f0"));
    qApp->setPalette(p);

    qApp->setStyleSheet(R"(
        QWidget { font-family: 'Segoe UI', 'SF Pro Display', 'Noto Sans', sans-serif; font-size: 12px; }
        QScrollBar:vertical { background:#1a1d24; width:10px; border-radius:5px; }
        QScrollBar::handle:vertical { background:#3a3d46; border-radius:5px; min-height:20px; }
        QScrollBar::handle:vertical:hover { background:#4a9eff; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QScrollBar:horizontal { background:#1a1d24; height:10px; border-radius:5px; }
        QScrollBar::handle:horizontal { background:#3a3d46; border-radius:5px; min-width:20px; }
        QScrollBar::handle:horizontal:hover { background:#4a9eff; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }
        QSplitter::handle { background:#2a2d36; }
        QToolTip { background:#252830; color:#e0e4f0; border:1px solid #3a3d46; padding:4px; border-radius:4px; }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            background:#13151a; color:#c0c4d0;
            border:1px solid #3a3d46; border-radius:4px;
            padding:4px 8px; selection-background-color:#2a5fa5;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus {
            border-color:#4a9eff;
        }
        QComboBox::drop-down { border: none; }
        QComboBox QAbstractItemView { background:#1e2128; color:#c0c4d0; selection-background-color:#2a5fa5; }
        QPushButton {
            background:#252830; color:#c0c4d0;
            border:1px solid #3a3d46; border-radius:4px;
            padding:6px 16px; min-width:80px;
        }
        QPushButton:hover  { background:#2e3240; border-color:#4a9eff; color:#e8eaf0; }
        QPushButton:pressed { background:#1e2440; }
        QPushButton:default {
            background:#1a4a8a; border-color:#4a9eff; color:#ffffff;
        }
        QPushButton:default:hover { background:#2060b0; }
        QHeaderView::section {
            background:#1e2128; color:#8a8f9e; font-size:11px; font-weight:bold;
            border: none; border-right:1px solid #2a2d36;
            padding:6px 8px; text-transform:uppercase;
        }
        QTableView, QTreeView, QListView {
            background:#13151a; color:#c0c4d0;
            gridline-color:#1e2128; border:none;
            selection-background-color:#1e3460;
        }
        QTableView::item, QTreeView::item {
            padding: 4px; border-bottom:1px solid #1e2128;
        }
        QTableView::item:selected, QTreeView::item:selected {
            background:#1e3460; color:#e8eaf0;
        }
        QTableView::item:alternate { background:#161820; }
        QGroupBox {
            border:1px solid #2a2d36; border-radius:6px;
            margin-top:12px; padding:8px;
            color:#8a8f9e; font-size:11px; font-weight:bold;
        }
        QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 4px; }
        QProgressBar {
            background:#1e2128; border:none; border-radius:3px;
            height:6px; text-align:center; color:transparent;
        }
        QProgressBar::chunk { background:#4a9eff; border-radius:3px; }
        QDialog { background:#1a1d24; }
    )");
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
}

void MainWindow::saveSettings() {
    QSettings s("BBRender", "BBRenderFarm");
    s.setValue("geometry", saveGeometry());
    s.setValue("splitter", m_mainSplitter->saveState());
}

void MainWindow::loadSettings() {
    QSettings s("BBRender", "BBRenderFarm");
    if (s.contains("geometry")) restoreGeometry(s.value("geometry").toByteArray());
    if (s.contains("splitter")) m_mainSplitter->restoreState(s.value("splitter").toByteArray());
}

} // namespace BBRender
