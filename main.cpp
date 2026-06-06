#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QPainter>
#include <QTimer>
#include <QFontDatabase>
#include <QDir>
#include "frontend/ui/mainwindow.h"

// ─── Splash screen ────────────────────────────────────────────────────────────
class BBSplash : public QSplashScreen {
public:
    BBSplash() : QSplashScreen() {
        QPixmap px(600, 340);
        px.fill(QColor("#0d0f14"));

        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);

        // Background grid
        p.setPen(QPen(QColor("#1a1d24"), 1));
        for (int x = 0; x < 600; x += 30)
            p.drawLine(x, 0, x, 340);
        for (int y = 0; y < 340; y += 30)
            p.drawLine(0, y, 600, y);

        // Gradient overlay
        QLinearGradient grad(0, 0, 0, 340);
        grad.setColorAt(0.0, QColor(0,0,0,180));
        grad.setColorAt(1.0, QColor(0,0,0,220));
        p.fillRect(0, 0, 600, 340, grad);

        // Accent bar
        QLinearGradient accentGrad(0, 0, 600, 0);
        accentGrad.setColorAt(0.0, QColor("#4a9eff"));
        accentGrad.setColorAt(0.5, QColor("#9060ff"));
        accentGrad.setColorAt(1.0, QColor("#4a9eff"));
        p.fillRect(0, 0, 600, 3, accentGrad);

        // Logo block
        p.setBrush(QColor("#4a9eff"));
        p.setPen(Qt::NoPen);
        p.drawRect(60, 100, 14, 14);
        p.drawRect(78, 100, 14, 14);
        p.drawRect(60, 118, 14, 14);
        p.setBrush(QColor("#9060ff"));
        p.drawRect(78, 118, 14, 14);

        // Title
        p.setPen(QColor("#ffffff"));
        p.setFont(QFont("Segoe UI", 36, QFont::Bold));
        p.drawText(104, 127, "BB RENDER");

        // Subtitle
        p.setPen(QColor("#4a9eff"));
        p.setFont(QFont("Segoe UI", 12));
        p.drawText(106, 150, "Professional Distributed Render Farm");

        // Divider
        p.setPen(QPen(QColor("#2a2d36"), 1));
        p.drawLine(60, 175, 540, 175);

        // Engine tags
        QStringList engines = {"NUKE", "SILHOUETTE", "BLENDER", "HOUDINI", "MAYA", "CINEMA4D"};
        QList<QColor> colors = {
            QColor("#ff6040"), QColor("#40a0ff"), QColor("#f06030"),
            QColor("#ffa040"), QColor("#40c090"), QColor("#c080ff")
        };
        int ex = 60;
        for (int i = 0; i < engines.size(); ++i) {
            QRect tag(ex, 190, 72, 22);
            p.setBrush(colors[i].darker(180));
            p.setPen(colors[i]);
            p.drawRoundedRect(tag, 3, 3);
            p.setPen(colors[i]);
            p.setFont(QFont("Segoe UI", 8, QFont::Bold));
            p.drawText(tag, Qt::AlignCenter, engines[i]);
            ex += 82;
        }

        // Bottom info
        p.setPen(QColor("#4a4d56"));
        p.setFont(QFont("Segoe UI", 9));
        p.drawText(60, 300, "v1.0.0  |  Qt6 + C++20  |  Windows / Linux / macOS");

        // Loading bar bg
        p.setBrush(QColor("#1e2128"));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(60, 315, 480, 6, 3, 3);

        p.end();
        setPixmap(px);
    }

    void setProgress(int pct, const QString& msg) {
        QPixmap copy = pixmap();
        QPainter p(&copy);
        // Clear loading area
        p.fillRect(60, 305, 480, 30, QColor("#0d0f14"));
        // Message
        p.setPen(QColor("#6e7280"));
        p.setFont(QFont("Segoe UI", 9));
        p.drawText(60, 315, msg);
        // Bar track
        p.setBrush(QColor("#1e2128")); p.setPen(Qt::NoPen);
        p.drawRoundedRect(60, 320, 480, 6, 3, 3);
        // Bar fill
        QLinearGradient bg(0, 0, 480, 0);
        bg.setColorAt(0.0, QColor("#4a9eff"));
        bg.setColorAt(1.0, QColor("#9060ff"));
        p.setBrush(bg);
        p.drawRoundedRect(60, 320, (int)(480 * pct / 100.0), 6, 3, 3);
        p.end();
        setPixmap(copy);
        repaint();
        QApplication::processEvents();
    }
};

// ─── Main ─────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // High-DPI support
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName("BBRender");
    app.setApplicationDisplayName("BB Render Farm");
    app.setOrganizationName("BBRender");
    app.setApplicationVersion("1.0.0");

    // Splash
    BBSplash splash;
    splash.show();

    splash.setProgress(10, "Initializing application...");
    QThread::msleep(120);

    splash.setProgress(25, "Loading render engine plugins...");
    QThread::msleep(120);

    splash.setProgress(40, "Starting scheduler engine...");
    QThread::msleep(120);

    splash.setProgress(55, "Loading farm configuration...");
    QThread::msleep(120);

    splash.setProgress(70, "Connecting to database...");
    QThread::msleep(120);

    splash.setProgress(85, "Setting up network layer...");
    QThread::msleep(120);

    splash.setProgress(95, "Building UI...");
    QThread::msleep(80);

    // Create main window
    BBRender::MainWindow win;

    splash.setProgress(100, "Ready.");
    QThread::msleep(300);

    win.show();
    splash.finish(&win);

    return app.exec();
}
