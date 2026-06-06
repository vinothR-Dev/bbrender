#pragma once
#include <QMainWindow>
#include <QTabWidget>
#include <QSplitter>
#include <QToolBar>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QLabel>
#include <memory>
#include "../../backend/scheduler/scheduler.h"
#include "../../backend/network/network_server.h"

// Forward declarations
class JobQueueWidget;
class WorkerMonitorWidget;
class JobSubmitDialog;
class FarmStatsWidget;
class LogWidget;
class JobDetailWidget;

namespace BBRender {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onSubmitJob();
    void onRefreshFarm();
    void onJobSelected(JobID id);
    void onWorkerSelected(WorkerID id);
    void onStartServer();
    void onStopServer();
    void onConnectToServer();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onTimerTick();
    void onSchedulerEvent(const SchedulerEvent& ev);
    void onPreferences();
    void onAbout();

signals:
    void schedulerEventReceived(const SchedulerEvent& ev);
    void farmStatsUpdated(const FarmStats& stats);

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupTray();
    void setupConnections();
    void applyDarkTheme();
    void updateStatusBar(const FarmStats& stats);
    void saveSettings();
    void loadSettings();

    // Core backend
    std::unique_ptr<Scheduler>     m_scheduler;
    std::unique_ptr<NetworkServer> m_server;

    // Central UI
    QTabWidget*         m_tabs          = nullptr;
    QSplitter*          m_mainSplitter  = nullptr;

    // Tab widgets
    JobQueueWidget*     m_jobQueue      = nullptr;
    WorkerMonitorWidget* m_workerMonitor = nullptr;
    FarmStatsWidget*    m_statsWidget   = nullptr;
    LogWidget*          m_logWidget     = nullptr;
    JobDetailWidget*    m_jobDetail     = nullptr;

    // Toolbar
    QToolBar*  m_toolbar   = nullptr;
    QAction*   m_actSubmit = nullptr;
    QAction*   m_actStart  = nullptr;
    QAction*   m_actStop   = nullptr;
    QAction*   m_actRefresh = nullptr;

    // Status bar widgets
    QLabel* m_statusWorkers  = nullptr;
    QLabel* m_statusJobs     = nullptr;
    QLabel* m_statusFPS      = nullptr;
    QLabel* m_statusEfficiency = nullptr;
    QLabel* m_statusServer   = nullptr;

    // System tray
    QSystemTrayIcon* m_trayIcon = nullptr;

    // Update timer
    QTimer* m_refreshTimer = nullptr;

    // State
    bool m_serverRunning = false;
    JobID m_selectedJob  = 0;
};

} // namespace BBRender
