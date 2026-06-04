#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QFormLayout>
#include <QSpinBox>
#include <QGroupBox>
#include <QTimer>
#include <QMetaObject>

#include "logger.h"
#include "config_manager.h"
#include "virtual_display.h"
#include "frame_grabber.h"
#include "video_encoder.h"
#include "usb_transport.h"

using namespace PlugScreen;

class PlugScreenMainWindow : public QMainWindow {
    Q_OBJECT

public:
    PlugScreenMainWindow() : QMainWindow() {
        setWindowTitle("PlugScreen // Windows Host Display Controller");
        setMinimumSize(800, 500);

        // Load config
        ConfigManager::Instance().LoadConfig("plugscreen_host.ini");

        // UI Building
        SetupUI();

        // Bind callbacks and slots
        connect(m_pStartBtn, &QPushButton::clicked, this, &PlugScreenMainWindow::OnStartClicked);
        connect(m_pStopBtn, &QPushButton::clicked, this, &PlugScreenMainWindow::OnStopClicked);

        // Quick log timer to pull logs and show on console UI
        QTimer* pLogTimer = new QTimer(this);
        connect(pLogTimer, &QTimer::timeout, this, &PlugScreenMainWindow::OnTimerUpdate);
        pLogTimer->start(200);

        LOG_INFO("UI_MAIN", "PlugScreen Qt Host application loaded.");
        UpdateSystemState("OFFLINE_IDLE");
    }

    ~PlugScreenMainWindow() {
        StopPipeline();
    }

private slots:
    void OnStartClicked() {
        LOG_INFO("UI_MAIN", "Initiating low-overhead screen mirror pipeline...");
        
        // Load configurations from input widgets
        ApplicationConfig currentConf = ConfigManager::Instance().GetConfig();
        currentConf.screenWidth = m_pWidthSpin->value();
        currentConf.screenHeight = m_pHeightSpin->value();
        currentConf.frameRate = m_pRateSpin->value();
        currentConf.targetBitrate = m_pBitrateSpin->value() * 1000 * 1000;
        ConfigManager::Instance().UpdateConfig(currentConf);
        ConfigManager::Instance().SaveConfig("plugscreen_host.ini");

        // 1. Load Virtual Desktop layers
        if (!m_display.Initialize(currentConf.screenWidth, currentConf.screenHeight, currentConf.frameRate)) {
            UpdateLogConsole("[!] Could not construct local virtual monitor layers.");
            return;
        }
        m_display.EnableMonitor();

        // 2. Setup low-latency USB transport pipelines
        m_transport.Initialize(
            [this](const HandshakePayload& handshake) {
                // Handle handshake
                QMetaObject::invokeMethod(this, [this, handshake]() {
                    UpdateLogConsole(QString("Handshake confirmed with: %1 (%2x%3 @ %4 FPS)")
                        .arg(handshake.deviceName)
                        .arg(handshake.requestWidth)
                        .arg(handshake.requestHeight)
                        .arg(handshake.preferredFps));
                    UpdateSystemState("HANDSHAKE_ACTIVE");
                });
            },
            [this](const InputEventPayload& touchEvt) {
                // Input backflows from tablet actions
                QMetaObject::invokeMethod(this, [this, touchEvt]() {
                    LOG_DEBUG("INPUT_BACKFLOW", QString("Received tablet action: %1, coords: (%2, %3)")
                        .arg(static_cast<int>(touchEvt.action))
                        .arg(touchEvt.normalizedX)
                        .arg(touchEvt.normalizedY).toStdString());
                });
            }
        );
        m_transport.AutoDetectDevice(currentConf.usbVendorId, currentConf.usbProductId);

        // 3. Configure Hardware Encoder structures
        m_encoder.Initialize(
            currentConf.screenWidth, 
            currentConf.screenHeight, 
            currentConf.frameRate, 
            currentConf.targetBitrate,
            [this](const uint8_t* pData, size_t size, uint64_t timestampUs, bool isKeyframe) {
                // USB transmitter callback inside the encoder
                uint8_t flags = 0;
                if (isKeyframe) flags |= FrameFlag_KeyFrame;
                m_sequenceCounter++;
                m_transport.StreamDataPacket(PacketType::VideoFrame, pData, static_cast<uint32_t>(size), m_sequenceCounter, flags);
            }
        );

        // 4. Trigger desktop grabber loops
        m_grabber.Initialize(0, [this](ID3D11Texture2D* pTex, uint64_t timestamp) {
            // Raw GPU texture acquired callback
            // Feed the DXGI frame directly to the compiler
            ID3D11Device* pDev = nullptr;
            pTex->GetDevice(&pDev);
            ID3D11DeviceContext* pCtx = nullptr;
            pDev->GetImmediateContext(&pCtx);
            
            m_encoder.EncodeFrame(pTex, pCtx, timestamp);
            
            pCtx->Release();
            pDev->Release();
        });
        m_grabber.Start();

        m_pStartBtn->setEnabled(false);
        m_pStopBtn->setEnabled(true);
        UpdateSystemState("STREAMING_ACTIVE");
    }

    void OnStopClicked() {
        StopPipeline();
        m_pStartBtn->setEnabled(true);
        m_pStopBtn->setEnabled(false);
        UpdateSystemState("OFFLINE_IDLE");
    }

    void OnTimerUpdate() {
        // Redraw diagnostic updates or grab files logs
    }

private:
    void SetupUI() {
        QWidget* pMainWidget = new QWidget(this);
        setCentralWidget(pMainWidget);

        QVBoxLayout* pMainLayout = new QVBoxLayout(pMainWidget);
        pMainLayout->setContentsMargins(15, 15, 15, 15);
        pMainLayout->setSpacing(12);

        // App Metadata Display
        QHBoxLayout* pHeaderLayout = new QHBoxLayout();
        QLabel* pLogoLabel = new QLabel("<b>PLUGSCREEN: High-Speed Display Mirror Engine</b>", this);
        pLogoLabel->setStyleSheet("font-size: 14px; color: #06B6D4;");
        m_pStatusLabel = new QLabel("Pipeline Status: OFFLINE", this);
        m_pStatusLabel->setStyleSheet("font-family: monospace; font-weight: bold; font-size: 11px;");
        pHeaderLayout->addWidget(pLogoLabel);
        pHeaderLayout->addStretch();
        pHeaderLayout->addWidget(m_pStatusLabel);
        pMainLayout->addLayout(pHeaderLayout);

        // Workspace Panels (Split layout options)
        QHBoxLayout* pWorkspaceLayout = new QHBoxLayout();
        pWorkspaceLayout->setSpacing(15);

        // Left Panel: Configurations GUI
        QGroupBox* pConfigBox = new QGroupBox("Channel & Encoder Settings", this);
        QFormLayout* pForm = new QFormLayout(pConfigBox);
        pForm->setLabelAlignment(Qt::AlignRight);

        m_pWidthSpin = new QSpinBox(this);
        m_pWidthSpin->setRange(1024, 3840);
        m_pWidthSpin->setValue(ConfigManager::Instance().GetConfig().screenWidth);

        m_pHeightSpin = new QSpinBox(this);
        m_pHeightSpin->setRange(768, 2160);
        m_pHeightSpin->setValue(ConfigManager::Instance().GetConfig().screenHeight);

        m_pRateSpin = new QSpinBox(this);
        m_pRateSpin->setRange(30, 120);
        m_pRateSpin->setValue(ConfigManager::Instance().GetConfig().frameRate);

        m_pBitrateSpin = new QSpinBox(this);
        m_pBitrateSpin->setRange(1, 20);
        m_pBitrateSpin->setValue(ConfigManager::Instance().GetConfig().targetBitrate / (1000 * 1000));
        m_pBitrateSpin->setSuffix(" Mbps");

        pForm->addRow("Extended Width:", m_pWidthSpin);
        pForm->addRow("Extended Height:", m_pHeightSpin);
        pForm->addRow("Target Framerate:", m_pRateSpin);
        pForm->addRow("Bitrate Limit:", m_pBitrateSpin);

        QHBoxLayout* pActionsLayout = new QHBoxLayout();
        m_pStartBtn = new QPushButton("Start Mirroring", this);
        m_pStartBtn->setStyleSheet("background-color: #06B6D4; color: black; font-weight: bold; padding: 6px; border-radius: 4px;");
        m_pStopBtn = new QPushButton("Stop Mirroring", this);
        m_pStopBtn->setEnabled(false);
        pActionsLayout->addWidget(m_pStartBtn);
        pActionsLayout->addWidget(m_pStopBtn);
        pForm->addRow(pActionsLayout);

        pWorkspaceLayout->addWidget(pConfigBox, 2);

        // Right Panel: Host Terminal Console Output Logs
        QGroupBox* pConsoleBox = new QGroupBox("Host System Core Diagnostics Log Terminal", this);
        QVBoxLayout* pConsoleLayout = new QVBoxLayout(pConsoleBox);
        m_pConsoleLogs = new QTextEdit(this);
        m_pConsoleLogs->setReadOnly(true);
        m_pConsoleLogs->setFontFamily("Courier New");
        m_pConsoleLogs->setFontPointSize(9);
        m_pConsoleLogs->setStyleSheet("background-color: #0F172A; color: #38BDF8; border-radius: 4px; border: 1px solid #334155;");
        pConsoleLayout->addWidget(m_pConsoleLogs);

        pWorkspaceLayout->addWidget(pConsoleBox, 3);
        pMainLayout->addLayout(pWorkspaceLayout);
    }

    void StopPipeline() {
        LOG_INFO("UI_MAIN", "Shutting down stream pipelines...");
        m_grabber.Shutdown();
        m_encoder.Shutdown();
        m_display.Shutdown();
        m_transport.Shutdown();
        UpdateLogConsole("[-] Stream pipelines closed.");
    }

    void UpdateLogConsole(const QString& line) {
        m_pConsoleLogs->append(QString("[%1] %2").arg(QTime::currentTime().toString(), line));
    }

    void UpdateSystemState(const QString& state) {
        m_pStatusLabel->setText(QString("STATE: [%1]").arg(state));
    }

    // Windows Subsystems
    VirtualDisplay m_display;
    FrameGrabber   m_grabber;
    VideoEncoder   m_encoder;
    UsbTransport   m_transport;

    uint32_t       m_sequenceCounter = 0;

    // GUI Handlers
    QLabel*      m_pStatusLabel;
    QSpinBox*    m_pWidthSpin;
    QSpinBox*    m_pHeightSpin;
    QSpinBox*    m_pRateSpin;
    QSpinBox*    m_pBitrateSpin;
    QPushButton* m_pStartBtn;
    QPushButton* m_pStopBtn;
    QTextEdit*   m_pConsoleLogs;
};

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    Logger::Instance().Initialize("plugscreen_host.log");
    PlugScreenMainWindow w;
    w.show();
    int res = a.exec();
    Logger::Instance().Shutdown();
    return res;
}

#include "main.moc"
