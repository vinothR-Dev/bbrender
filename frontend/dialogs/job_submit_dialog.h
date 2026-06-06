#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include "../../shared/types/bb_types.h"

namespace BBRender {

class JobSubmitDialog : public QDialog {
    Q_OBJECT
public:
    explicit JobSubmitDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Submit Render Job — BB Render Farm");
        setMinimumSize(700, 600);
        setModal(true);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // Header bar
        auto* hdr = new QWidget();
        hdr->setFixedHeight(56);
        hdr->setStyleSheet("background:#1a1d24; border-bottom:1px solid #2a2d36;");
        auto* hdrLay = new QHBoxLayout(hdr);
        hdrLay->setContentsMargins(20, 0, 20, 0);
        auto* hdrTitle = new QLabel("⬛  NEW RENDER JOB");
        hdrTitle->setStyleSheet("color:#4a9eff; font-size:15px; font-weight:bold; letter-spacing:2px;");
        hdrLay->addWidget(hdrTitle);
        hdrLay->addStretch();
        root->addWidget(hdr);

        // Tabs
        auto* tabs = new QTabWidget();
        tabs->setDocumentMode(true);
        root->addWidget(tabs, 1);

        // ── Tab 1: Job Settings ────────────────────────────────────────────
        auto* jobTab = new QWidget();
        auto* jobLay = new QFormLayout(jobTab);
        jobLay->setContentsMargins(24, 20, 24, 20);
        jobLay->setSpacing(12);
        jobLay->setLabelAlignment(Qt::AlignRight);

        m_nameEdit = new QLineEdit("Untitled Render");
        m_nameEdit->setPlaceholderText("e.g. Shot_020_beauty_v003");
        jobLay->addRow("Job Name *", m_nameEdit);

        m_deptEdit = new QLineEdit();
        m_deptEdit->setPlaceholderText("e.g. Compositing, FX, Lighting");
        jobLay->addRow("Department", m_deptEdit);

        m_userEdit = new QLineEdit(qgetenv("USER"));
        jobLay->addRow("Submitted By", m_userEdit);

        m_priorityCombo = new QComboBox();
        m_priorityCombo->addItem("🔴 Critical",   (int)Priority::Critical);
        m_priorityCombo->addItem("🟠 High",        (int)Priority::High);
        m_priorityCombo->addItem("🟡 Normal",      (int)Priority::Normal);
        m_priorityCombo->addItem("🟢 Low",         (int)Priority::Low);
        m_priorityCombo->addItem("⚫ Background",  (int)Priority::Background);
        m_priorityCombo->setCurrentIndex(2);
        jobLay->addRow("Priority", m_priorityCombo);

        m_notesEdit = new QTextEdit();
        m_notesEdit->setPlaceholderText("Optional notes / description...");
        m_notesEdit->setFixedHeight(60);
        jobLay->addRow("Notes", m_notesEdit);

        tabs->addTab(jobTab, "  Job  ");

        // ── Tab 2: Render Settings ────────────────────────────────────────
        auto* renderTab = new QWidget();
        auto* renderLay = new QFormLayout(renderTab);
        renderLay->setContentsMargins(24, 20, 24, 20);
        renderLay->setSpacing(12);
        renderLay->setLabelAlignment(Qt::AlignRight);

        // Engine
        m_engineCombo = new QComboBox();
        m_engineCombo->addItem("Nuke",         (int)RenderEngine::Nuke);
        m_engineCombo->addItem("Silhouette",   (int)RenderEngine::Silhouette);
        m_engineCombo->addItem("Blender",      (int)RenderEngine::Blender);
        m_engineCombo->addItem("Houdini",      (int)RenderEngine::Houdini);
        m_engineCombo->addItem("Maya",         (int)RenderEngine::Maya);
        m_engineCombo->addItem("Cinema4D",     (int)RenderEngine::Cinema4D);
        m_engineCombo->addItem("After Effects",(int)RenderEngine::AfterEffects);
        m_engineCombo->addItem("Custom",       (int)RenderEngine::Custom);
        renderLay->addRow("Render Engine *", m_engineCombo);

        // Scene file
        auto* sceneRow = new QHBoxLayout();
        m_sceneEdit = new QLineEdit();
        m_sceneEdit->setPlaceholderText("/path/to/comp.nk  or  /path/to/scene.blend");
        auto* sceneBtn = new QPushButton("Browse...");
        sceneBtn->setFixedWidth(80);
        sceneRow->addWidget(m_sceneEdit);
        sceneRow->addWidget(sceneBtn);
        renderLay->addRow("Scene File *", sceneRow);

        // Output path
        auto* outRow = new QHBoxLayout();
        m_outputEdit = new QLineEdit();
        m_outputEdit->setPlaceholderText("/render/output/###.exr");
        auto* outBtn = new QPushButton("Browse...");
        outBtn->setFixedWidth(80);
        outRow->addWidget(m_outputEdit);
        outRow->addWidget(outBtn);
        renderLay->addRow("Output Path *", outRow);

        // Engine path
        m_enginePathEdit = new QLineEdit();
        m_enginePathEdit->setPlaceholderText("Leave empty to use system default");
        renderLay->addRow("Engine Executable", m_enginePathEdit);

        // Camera
        m_cameraEdit = new QLineEdit();
        m_cameraEdit->setPlaceholderText("camera1 (optional)");
        renderLay->addRow("Camera", m_cameraEdit);

        // Extra args
        m_argsEdit = new QLineEdit();
        m_argsEdit->setPlaceholderText("-x -t 4 (optional extra args)");
        renderLay->addRow("Extra Args", m_argsEdit);

        tabs->addTab(renderTab, "  Render  ");

        // ── Tab 3: Frame Range ────────────────────────────────────────────
        auto* frameTab = new QWidget();
        auto* frameLay = new QFormLayout(frameTab);
        frameLay->setContentsMargins(24, 20, 24, 20);
        frameLay->setSpacing(12);
        frameLay->setLabelAlignment(Qt::AlignRight);

        m_frameStart = new QSpinBox(); m_frameStart->setRange(1, 999999); m_frameStart->setValue(1);
        m_frameEnd   = new QSpinBox(); m_frameEnd->setRange(1, 999999);   m_frameEnd->setValue(100);
        m_frameStep  = new QSpinBox(); m_frameStep->setRange(1, 100);     m_frameStep->setValue(1);
        m_chunkSize  = new QSpinBox(); m_chunkSize->setRange(1, 1000);    m_chunkSize->setValue(5);
        m_chunkSize->setToolTip("Number of frames per task (smaller = finer distribution)");

        auto* rangeRow = new QHBoxLayout();
        rangeRow->addWidget(new QLabel("Start:"));   rangeRow->addWidget(m_frameStart);
        rangeRow->addWidget(new QLabel("End:"));     rangeRow->addWidget(m_frameEnd);
        rangeRow->addWidget(new QLabel("Step:"));    rangeRow->addWidget(m_frameStep);
        rangeRow->addStretch();
        frameLay->addRow("Frame Range *", rangeRow);
        frameLay->addRow("Chunk Size", m_chunkSize);

        // Frame count preview
        m_frameCountLabel = new QLabel("→ 100 frames / 20 tasks");
        m_frameCountLabel->setStyleSheet("color:#4a9eff; font-size:11px;");
        frameLay->addRow("", m_frameCountLabel);

        tabs->addTab(frameTab, "  Frames  ");

        // ── Tab 4: Resources ─────────────────────────────────────────────
        auto* resTab = new QWidget();
        auto* resLay = new QFormLayout(resTab);
        resLay->setContentsMargins(24, 20, 24, 20);
        resLay->setSpacing(12);
        resLay->setLabelAlignment(Qt::AlignRight);

        m_cpuCoresReq = new QSpinBox(); m_cpuCoresReq->setRange(0, 256); m_cpuCoresReq->setValue(0);
        m_cpuCoresReq->setSpecialValueText("Any");
        m_cpuCoresReq->setToolTip("Minimum CPU cores required (0 = any)");
        resLay->addRow("Min CPU Cores", m_cpuCoresReq);

        m_ramReq = new QSpinBox(); m_ramReq->setRange(0, 1024*1024); m_ramReq->setSuffix(" MB");
        m_ramReq->setValue(0); m_ramReq->setSpecialValueText("Any");
        resLay->addRow("Min RAM", m_ramReq);

        m_gpuReq = new QSpinBox(); m_gpuReq->setRange(0, 8); m_gpuReq->setValue(0);
        m_gpuReq->setSpecialValueText("Not required");
        resLay->addRow("GPUs Required", m_gpuReq);

        m_vramReq = new QSpinBox(); m_vramReq->setRange(0, 1024*1024); m_vramReq->setSuffix(" MB");
        m_vramReq->setValue(0); m_vramReq->setSpecialValueText("Any");
        resLay->addRow("Min VRAM", m_vramReq);

        m_platformCombo = new QComboBox();
        m_platformCombo->addItems({"Any Platform", "Linux", "Windows", "macOS"});
        resLay->addRow("Platform", m_platformCombo);

        tabs->addTab(resTab, "  Resources  ");

        // ── Buttons ────────────────────────────────────────────────────────
        auto* btnBar = new QWidget();
        btnBar->setFixedHeight(60);
        btnBar->setStyleSheet("background:#1a1d24; border-top:1px solid #2a2d36;");
        auto* btnLay = new QHBoxLayout(btnBar);
        btnLay->setContentsMargins(20, 10, 20, 10);

        auto* cancelBtn = new QPushButton("Cancel");
        cancelBtn->setFixedSize(100, 36);
        auto* submitBtn = new QPushButton("▶  Submit Job");
        submitBtn->setFixedSize(140, 36);
        submitBtn->setDefault(true);
        submitBtn->setStyleSheet("QPushButton { background:#1a4a8a; color:#fff; "
                                 "border:1px solid #4a9eff; border-radius:4px; "
                                 "font-weight:bold; font-size:13px; }"
                                 "QPushButton:hover { background:#2060b0; }");
        btnLay->addStretch();
        btnLay->addWidget(cancelBtn);
        btnLay->addWidget(submitBtn);
        root->addWidget(btnBar);

        // Connections
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        connect(submitBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(sceneBtn, &QPushButton::clicked, this, [this]{
            auto f = QFileDialog::getOpenFileName(this, "Select Scene File");
            if (!f.isEmpty()) m_sceneEdit->setText(f);
        });
        connect(outBtn, &QPushButton::clicked, this, [this]{
            auto f = QFileDialog::getSaveFileName(this, "Select Output Path");
            if (!f.isEmpty()) m_outputEdit->setText(f);
        });

        auto updateFrameCount = [this]{
            int total = (m_frameEnd->value() - m_frameStart->value()) / m_frameStep->value() + 1;
            int tasks = (total + m_chunkSize->value() - 1) / m_chunkSize->value();
            m_frameCountLabel->setText(QString("→ %1 frames / %2 tasks").arg(total).arg(tasks));
        };
        connect(m_frameStart, QOverload<int>::of(&QSpinBox::valueChanged), updateFrameCount);
        connect(m_frameEnd,   QOverload<int>::of(&QSpinBox::valueChanged), updateFrameCount);
        connect(m_frameStep,  QOverload<int>::of(&QSpinBox::valueChanged), updateFrameCount);
        connect(m_chunkSize,  QOverload<int>::of(&QSpinBox::valueChanged), updateFrameCount);
    }

    JobInfo getJobInfo() const {
        JobInfo j;
        j.name         = m_nameEdit->text().toStdString();
        j.department   = m_deptEdit->text().toStdString();
        j.submittedBy  = m_userEdit->text().toStdString();
        j.notes        = m_notesEdit->toPlainText().toStdString();
        j.priority     = static_cast<Priority>(m_priorityCombo->currentData().toInt());
        j.engine       = static_cast<RenderEngine>(m_engineCombo->currentData().toInt());
        j.sceneFile    = m_sceneEdit->text().toStdString();
        j.outputPath   = m_outputEdit->text().toStdString();
        j.enginePath   = m_enginePathEdit->text().toStdString();
        j.camera       = m_cameraEdit->text().toStdString();
        j.engineArgs   = m_argsEdit->text().toStdString();
        j.frames.start     = m_frameStart->value();
        j.frames.end       = m_frameEnd->value();
        j.frames.step      = m_frameStep->value();
        j.frames.chunkSize = m_chunkSize->value();
        j.resources.cpuCores = m_cpuCoresReq->value();
        j.resources.ramMB    = m_ramReq->value();
        j.resources.gpuCount = m_gpuReq->value();
        j.resources.vramMB   = m_vramReq->value();
        auto p = m_platformCombo->currentText().toLower();
        if (p != "any platform") j.resources.platform = p.toStdString();
        return j;
    }

private:
    QLineEdit*  m_nameEdit       = nullptr;
    QLineEdit*  m_deptEdit       = nullptr;
    QLineEdit*  m_userEdit       = nullptr;
    QComboBox*  m_priorityCombo  = nullptr;
    QTextEdit*  m_notesEdit      = nullptr;
    QComboBox*  m_engineCombo    = nullptr;
    QLineEdit*  m_sceneEdit      = nullptr;
    QLineEdit*  m_outputEdit     = nullptr;
    QLineEdit*  m_enginePathEdit = nullptr;
    QLineEdit*  m_cameraEdit     = nullptr;
    QLineEdit*  m_argsEdit       = nullptr;
    QSpinBox*   m_frameStart     = nullptr;
    QSpinBox*   m_frameEnd       = nullptr;
    QSpinBox*   m_frameStep      = nullptr;
    QSpinBox*   m_chunkSize      = nullptr;
    QLabel*     m_frameCountLabel = nullptr;
    QSpinBox*   m_cpuCoresReq    = nullptr;
    QSpinBox*   m_ramReq         = nullptr;
    QSpinBox*   m_gpuReq         = nullptr;
    QSpinBox*   m_vramReq        = nullptr;
    QComboBox*  m_platformCombo  = nullptr;
};

} // namespace BBRender
