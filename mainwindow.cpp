#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QFrame>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <algorithm>
#include <QTimer>
#include <QLineEdit>
#include <QInputDialog>
#include <QGraphicsDropShadowEffect>
// 放在 mainwindow.cpp 顶部

// mainwindow.cpp 顶部的 CardDelegate 类

class CardDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    // 1. 【修改】调整牌的尺寸，让它更大、比例更协调
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        return QSize(80, 110); // 宽80，高110
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        if (!index.isValid()) return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        QString text = index.data(Qt::DisplayRole).toString();

        // 处理特殊状态文字
        if (text == "不要" || text == "PASS") {
            painter->setPen(QColor(100, 100, 100));
            QFont font = painter->font();
            font.setBold(true);
            font.setPixelSize(22); // 字体加大
            painter->setFont(font);
            painter->drawText(option.rect, Qt::AlignCenter, text);
            painter->restore();
            return;
        }
        if (text.isEmpty()) { painter->restore(); return; }

        // 绘制卡牌背景
        bool isSelected = index.data(Qt::UserRole).toBool();
        QRect rect = option.rect;

        // 【修改】调整边距，防止文字被切掉
        rect.adjust(4, 20, -4, -4);
        if (isSelected) rect.translate(0, -20); // 选中上浮

        // 阴影
        painter->setBrush(QColor(0, 0, 0, 50));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(rect.translated(2, 2), 8, 8);

        // 牌面
        painter->setBrush(Qt::white);
        painter->setPen(QPen(Qt::gray, 1));
        painter->drawRoundedRect(rect, 8, 8);

        // 2. 【修复】解析花色和点数的核心逻辑
        QColor textColor = Qt::black;
        if (text.contains("♥") || text.contains("♦")) textColor = QColor(200, 0, 0); // 红色

        QString rankStr = text;
        QString suitStr = "";

        if (text.contains("jokerSmall")) {
            rankStr = "JOKER"; suitStr="BLACK"; textColor=Qt::black;
        }
        else if (text.contains("jokerBig")) {
            rankStr = "JOKER"; suitStr="RED"; textColor=QColor(200, 0, 0);
        }
        else {
            // 提取花色（Qt中花色符号长度通常为1）
            if (text.startsWith("♠")) suitStr="♠";
            else if (text.startsWith("♣")) suitStr="♣";
            else if (text.startsWith("♥")) suitStr="♥";
            else if (text.startsWith("♦")) suitStr="♦";

            // 【重要修复】使用 suitStr.length() 而不是固定值 3
            rankStr = text.mid(suitStr.length());
        }

        painter->setPen(textColor);

        // 绘制左上角点数
        QFont font = painter->font();
        font.setBold(true);
        font.setPixelSize(20); // 点数大小
        painter->setFont(font);

        // 【修改】调整文字坐标
        QRect topRect = rect.adjusted(5, 5, 0, 0);

        if (text.contains("joker")) {
            // 大小王竖排显示
            font.setPixelSize(12);
            painter->setFont(font);
            painter->drawText(topRect, Qt::AlignLeft | Qt::AlignTop, text.contains("Big") ? "大\n王" : "小\n王");
        } else {
            painter->drawText(topRect, Qt::AlignLeft | Qt::AlignTop, rankStr);

            // 点数下面画个小花色
            font.setPixelSize(14);
            painter->setFont(font);
            painter->drawText(topRect.adjusted(0, 22, 0, 0), Qt::AlignLeft | Qt::AlignTop, suitStr);
        }

        // 绘制中央大花色（水印效果）
        if (!suitStr.isEmpty() && suitStr.length() < 5) {
            font.setPixelSize(45);
            painter->setFont(font);
            painter->setOpacity(0.2); // 半透明
            painter->drawText(rect, Qt::AlignCenter, suitStr);
        }

        painter->restore();
    }
};
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    gameManager(new GameManager(this))
{
    gameManager->setPlayers();
    humanPlayer = gameManager->getHumanPlayer();
    judge = gameManager->getJudge();
    aiPlayer1 = gameManager->getAIPlayer(1);
    setupUI();
    setupConnections();

    updateUI();
}
MainWindow::~MainWindow()
{
}
void MainWindow::setupUI()
{
    setWindowTitle("掼蛋 - 示例版");
    resize(1280, 800);

    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QFrame *infoBar = new QFrame;
    infoBar->setObjectName("infoBar");
    QHBoxLayout *infoLayout = new QHBoxLayout(infoBar);
    infoLayout->setContentsMargins(16, 10, 16, 10);
    infoLayout->setSpacing(18);

    QLabel *titleLabel = new QLabel("掼蛋欢乐桌");
    titleLabel->setObjectName("titleLabel");

    QLabel *coinBadge = new QLabel("🪙 金币 x 9999");
    coinBadge->setObjectName("coinBadge");
    coinBadge->setAlignment(Qt::AlignCenter);

    listAI1 = new QListWidget;
    listAI2 = new QListWidget;
    listAI3 = new QListWidget;
    listHuman = new QListWidget;
    listHuman->setSizeAdjustPolicy(QAbstractItemView::AdjustToContents);
    listHuman->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    listHuman->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listHuman->setUniformItemSizes(true);
    lblLastPlay = new QLabel("上家出牌：无");
    lblStatus = new QLabel("游戏状态：等待开始");
    lblLevels = new QLabel("队伍等级：-- / --");
    lblLevelCard = new QLabel("本局级牌：待定");

    infoLayout->addWidget(titleLabel, 0, Qt::AlignVCenter);
    infoLayout->addWidget(lblStatus, 0, Qt::AlignVCenter);
    infoLayout->addWidget(lblLastPlay, 0, Qt::AlignVCenter);
    infoLayout->addWidget(lblLevels, 0, Qt::AlignVCenter);
    infoLayout->addWidget(lblLevelCard, 0, Qt::AlignVCenter);
    infoLayout->addStretch();
    infoLayout->addWidget(coinBadge, 0, Qt::AlignVCenter);
    lblSelection = new QLabel("已选中：0 张 (无)");
    btnNewGame = new QPushButton("新游戏");
    btnPlay = new QPushButton("出牌");
    btnPass = new QPushButton("不要(过)");
    btnCheatWin = new QPushButton("测试：一键获胜");
    btnCheatWin->setStyleSheet(
        "QPushButton { background-color: #8E44AD; color: white; border: 2px solid #5B2C6F; border-radius: 15px; font-weight: bold; padding: 5px; }"
        "QPushButton:hover { background-color: #9B59B6; }"
        "QPushButton:pressed { background-color: #71368A; }"
        );
    btnDebugOrder = new QPushButton("测试：指定排名结算");
    btnDebugOrder->setStyleSheet(
        "QPushButton { background-color: #2E86C1; color: white; border-radius: 15px; font-weight: bold; padding: 5px; }"
        "QPushButton:hover { background-color: #3498DB; }"
        );
    connect(btnDebugOrder, &QPushButton::clicked, this, [this]() {
        if (!judge || !gameManager) return;

        // 1. 弹出输入框
        bool ok;
        QString text = QInputDialog::getText(this, "指定完赛顺序",
                                             "请输入4个玩家ID (0-3)，用空格隔开\n"
                                             "例如: 0 2 1 3 (表示你自己第一，队友第二)",
                                             QLineEdit::Normal,
                                             "0 2 1 3", &ok);
        if (!ok || text.isEmpty()) return;

        // 2. 解析输入的字符串
        QStringList parts = text.split(" ", Qt::SkipEmptyParts);
        std::vector<int> order;
        std::set<int> checkDup; // 用于检查重复

        bool parseError = false;
        if (parts.size() != 4) parseError = true;

        for (const QString& s : parts) {
            bool isInt;
            int id = s.toInt(&isInt);
            if (!isInt || id < 0 || id > 3 || checkDup.count(id)) {
                parseError = true;
                break;
            }
            order.push_back(id);
            checkDup.insert(id);
        }

        if (parseError) {
            QMessageBox::warning(this, "输入错误", "请输入 0, 1, 2, 3 四个不重复的数字，用空格隔开！");
            return;
        }

        // 3. 调用 Judge 接口强制结算
        judge->debugSimulateGameEnd(order);

        // 注意：
        // 调用 debugSimulateGameEnd 后，Judge 会发出 gameFinished 信号。
        // 你在 setupConnections 里已经连接了 gameFinished 信号到弹窗逻辑。
        // 所以此时会自动弹出 "本局战报...是否继续下一局" 的对话框。
        // 点击 "Yes" 即可开启下一轮。

        // 如果你想跳过那个对话框直接开始下一局，可以将下面的代码解开注释：
        /*
    QTimer::singleShot(500, gameManager, &GameManager::startNextRound);
    */
    });
    btnPlay->setEnabled(false);
    btnPass->setEnabled(false);

    listAI1->setSelectionMode(QAbstractItemView::NoSelection);
    listAI2->setSelectionMode(QAbstractItemView::NoSelection);
    listAI3->setSelectionMode(QAbstractItemView::NoSelection);
    listHuman->setSelectionMode(QAbstractItemView::NoSelection);

    // --- 中央出牌区（四个面板） ---
    playTop = new QListWidget;         // 对应玩家 2（队友）
    playLeft = new QListWidget;        // 对应玩家 3（左手边）
    playRight = new QListWidget;       // 对应玩家 1（右手边）
    playCenterBottom = new QListWidget;// 对应 Human 的桌面出牌（位于 human 手牌之上）

    auto makePlayWidgetDefault = [](QListWidget* w){
        w->setViewMode(QListView::IconMode);
        w->setFlow(QListView::LeftToRight);
        w->setWrapping(false);
        w->setResizeMode(QListView::Adjust);
        w->setSpacing(4);
        w->setMovement(QListView::Static);
        w->setIconSize(QSize(70, 105));
        w->setFixedHeight(140);
    };

    makePlayWidgetDefault(playTop);
    makePlayWidgetDefault(playLeft);
    makePlayWidgetDefault(playRight);
    makePlayWidgetDefault(playCenterBottom);
    // 辅助函数：创建一个带头像和文字的垂直布局
    auto createPlayerInfoWidget = [](QString name, QListWidget* remainList = nullptr) -> QWidget* {
        QWidget* box = new QWidget;
        QVBoxLayout* layout = new QVBoxLayout(box);
        layout->setSpacing(6);
        layout->setContentsMargins(6, 6, 6, 6);

        QLabel* avatar = new QLabel;
        avatar->setPixmap(QPixmap(60, 60));
        avatar->setStyleSheet("QLabel { background-color: #F1C40F; border-radius: 30px; color: #4A235A; font-size: 24px; border: 2px solid white; }");
        avatar->setFixedSize(60, 60);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setText(name.left(1));

        QLabel* nameLbl = new QLabel(name);
        nameLbl->setAlignment(Qt::AlignCenter);

        layout->addWidget(avatar, 0, Qt::AlignCenter);
        layout->addWidget(nameLbl, 0, Qt::AlignCenter);
        if (remainList) {
            remainList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            remainList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            remainList->setFrameShape(QFrame::NoFrame);
            remainList->setStyleSheet("QListWidget { color: #F8E71C; font-weight: bold; background: transparent; }");
            remainList->setFixedHeight(28);
            layout->addWidget(remainList, 0, Qt::AlignCenter);
        }

        return box;
    };
    // --- 布局：带牌桌的中心区域，外加独立的手牌区域 ---
    QVBoxLayout *mainLay = new QVBoxLayout;

    auto applyShadow = [](QWidget *w) {
        QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(w);
        shadow->setBlurRadius(20);
        shadow->setColor(QColor(0, 0, 0, 120));
        shadow->setOffset(0, 6);
        w->setGraphicsEffect(shadow);
    };

    QFrame *boardFrame = new QFrame;
    boardFrame->setObjectName("boardFrame");
    applyShadow(boardFrame);

    QWidget *topInfo = createPlayerInfoWidget("AI 电脑 2", listAI2);
    QWidget *leftInfo = createPlayerInfoWidget("AI 电脑 3", listAI3);
    QWidget *rightInfo = createPlayerInfoWidget("AI 电脑 1", listAI1);
    QWidget *humanInfo = createPlayerInfoWidget("玩家本人", nullptr);

    QVBoxLayout *boardLayout = new QVBoxLayout(boardFrame);
    boardLayout->setContentsMargins(16, 12, 16, 12);
    boardLayout->setSpacing(10);

    boardLayout->addWidget(topInfo, 0, Qt::AlignHCenter);

    QFrame *tableSurface = new QFrame;
    tableSurface->setObjectName("tableSurface");
    QGridLayout *tableGrid = new QGridLayout(tableSurface);
    tableGrid->setContentsMargins(24, 24, 24, 24);
    tableGrid->setHorizontalSpacing(6);
    tableGrid->setVerticalSpacing(6);
    tableGrid->setColumnStretch(0, 1);
    tableGrid->setColumnStretch(1, 2);
    tableGrid->setColumnStretch(2, 1);
    tableGrid->setRowStretch(0, 1);
    tableGrid->setRowStretch(1, 2);
    tableGrid->setRowStretch(2, 1);

    QWidget *centerInfo = new QWidget;
    centerInfo->setAttribute(Qt::WA_StyledBackground, false);
    centerInfo->setStyleSheet("background: transparent;");
    QVBoxLayout *centerBox = new QVBoxLayout(centerInfo);
    centerBox->setAlignment(Qt::AlignCenter);
    centerBox->setSpacing(6);
    centerBox->addWidget(lblLastPlay, 0, Qt::AlignCenter);
    centerBox->addWidget(lblStatus, 0, Qt::AlignCenter);
    centerBox->addWidget(lblLevels, 0, Qt::AlignCenter);
    centerBox->addWidget(lblLevelCard, 0, Qt::AlignCenter);

    tableGrid->addWidget(playTop, 0, 1, Qt::AlignHCenter | Qt::AlignTop);
    tableGrid->addWidget(playLeft, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);
    tableGrid->addWidget(centerInfo, 1, 1, Qt::AlignCenter);
    tableGrid->addWidget(playRight, 1, 2, Qt::AlignRight | Qt::AlignVCenter);
    tableGrid->addWidget(playCenterBottom, 2, 1, Qt::AlignHCenter | Qt::AlignBottom);

    QLabel *deskTitle = new QLabel("桌面 - 你的出牌");
    deskTitle->setAlignment(Qt::AlignCenter);

    QHBoxLayout *middleRow = new QHBoxLayout;
    middleRow->setSpacing(12);
    middleRow->addWidget(leftInfo, 0, Qt::AlignTop);
    middleRow->addWidget(tableSurface, 1);
    middleRow->addWidget(rightInfo, 0, Qt::AlignTop);

    boardLayout->addLayout(middleRow);
    boardLayout->addWidget(deskTitle, 0, Qt::AlignCenter);
    boardLayout->addWidget(humanInfo, 0, Qt::AlignHCenter);

    QFrame *handFrame = new QFrame;
    handFrame->setObjectName("handFrame");
    applyShadow(handFrame);
    QVBoxLayout *handLayout = new QVBoxLayout(handFrame);
    handLayout->setContentsMargins(16, 12, 16, 12);
    handLayout->setSpacing(8);
    QLabel *handTitle = new QLabel("你的手牌");
    handTitle->setAlignment(Qt::AlignCenter);
    handLayout->addWidget(handTitle);
    handLayout->addWidget(lblSelection);

    QScrollArea *handScroll = new QScrollArea;
    handScroll->setFrameShape(QFrame::NoFrame);
    handScroll->setWidgetResizable(true);
    handScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    handScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *handContainer = new QWidget;
    QHBoxLayout *handContainerLayout = new QHBoxLayout(handContainer);
    handContainerLayout->setContentsMargins(0, 0, 0, 0);
    handContainerLayout->addWidget(listHuman);
    handScroll->setWidget(handContainer);

    handLayout->addWidget(handScroll);

    // Buttons row
    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(btnNewGame);
    buttons->addWidget(btnPlay);
    buttons->addWidget(btnPass);
    buttons->addWidget(btnCheatWin);
    buttons->addWidget(btnDebugOrder);

    applyShadow(infoBar);

    mainLay->addWidget(infoBar);
    mainLay->addWidget(boardFrame);
    mainLay->addWidget(handFrame);
    mainLay->addLayout(buttons);

    central->setLayout(mainLay);
    // 设置委托
    CardDelegate* delegate = new CardDelegate(this);
    listHuman->setItemDelegate(delegate);
    playTop->setItemDelegate(delegate);
    playLeft->setItemDelegate(delegate);
    playRight->setItemDelegate(delegate);
    playCenterBottom->setItemDelegate(delegate);

    auto polishList = [](QListWidget* list, int baseIconW = 90, int baseIconH = 120, int overlap = -40, bool allowScroll = false) {
        list->setViewMode(QListView::IconMode);
        list->setFlow(QListView::LeftToRight);
        list->setWrapping(false);
        list->setResizeMode(QListView::Adjust);

        // 图标尺寸（牌实际绘制尺寸）
        list->setIconSize(QSize(baseIconW, baseIconH));

        // 每个格子的大小（一定要 >= icon size + 上浮空间 + 内边距）
        // 上浮空间: 假设选中会上浮 20 px；内边距/边框约 8 px
        int gridW = baseIconW;
        int gridH = baseIconH + 28; // 保证垂直方向有多余空间

        list->setGridSize(QSize(gridW, gridH));

        // 重叠/露出效果：spacing 可以为负数，使卡片部分重叠
        list->setSpacing(overlap);

        list->setMovement(QListView::Static);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setHorizontalScrollBarPolicy(allowScroll ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
        list->setStyleSheet("background: transparent; border: none;");
        list->setFocusPolicy(Qt::NoFocus);

        // 设置固定高度：至少为格子高度 + 顶部/底部外边距
        int extraVerticalMargin = 10; // 额外空白
        list->setFixedHeight(gridH + extraVerticalMargin);
    };

    polishList(listHuman, 90, 120, -28, true);      // 底部手牌，图标 90x120，重叠 -28，支持横向滚动
    polishList(playTop, 70, 90, -46);         // 顶部AI出牌区，稍小但保持堆叠
    polishList(playLeft, 70, 90, -46);
    polishList(playRight, 70, 90, -46);
    polishList(playCenterBottom, 80, 100, -42); // 中央桌面显示区，保持明显重叠

    // 2. 全局 QSS 样式表
    QString qss = R"(
        /* 全局背景：明亮绿色牌桌 + 律动渐变光 */
        QMainWindow {
            background: qradialgradient(spread:pad, cx:0.5, cy:0.35, radius:0.7, fx:0.5, fy:0.35,
                stop:0 rgba(90, 200, 140, 0.32), stop:1 rgba(20, 70, 40, 0.95));
        }

        /* 顶部信息条：半透明磨砂 + 光带描边 */
        QFrame#infoBar {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 rgba(255,255,255,0.16), stop:1 rgba(255,255,255,0.08));
            border: 1px solid rgba(255,255,255,0.18);
            border-radius: 16px;
        }

        QLabel {
            color: #FDFDFD;
            font-family: "Microsoft YaHei";
            font-size: 14px;
            font-weight: 600;
        }

        QLabel#titleLabel {
            font-size: 18px;
            letter-spacing: 1px;
            color: #F9E79F;
        }

        QLabel#coinBadge {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFD26F, stop:1 #FF9A3C);
            color: #5A2E00;
            padding: 8px 14px;
            border-radius: 14px;
            border: 2px solid rgba(255, 255, 255, 0.55);
            font-weight: 800;
        }

        QListWidget {
            background-color: transparent;
            border: none;
            outline: none;
        }

        QFrame#boardFrame, QFrame#handFrame {
            background: rgba(10, 40, 24, 0.45);
            border: 1px solid rgba(255, 255, 255, 0.14);
            border-radius: 16px;
            padding: 10px;
        }

        QFrame#tableSurface {
            background: qradialgradient(cx:0.5, cy:0.5, fx:0.5, fy:0.45, radius:1,
                stop:0 #126f3a, stop:1 #0a3f24);
            border: 2px solid rgba(255, 255, 255, 0.16);
            border-radius: 18px;
            min-height: 260px;
        }

        QScrollArea {
            background: transparent;
            border: none;
        }

        /* 按钮：高光胶囊风格 */
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFD05A, stop:1 #FF9C2D);
            border: 2px solid #C9780F;
            border-radius: 18px;
            color: #4A2500;
            font-family: "SimHei";
            font-size: 16px;
            font-weight: 700;
            padding: 8px 18px;
            min-width: 90px;
            min-height: 34px;
        }
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFE07A, stop:1 #FFB347);
            margin-top: -1px;
        }
        QPushButton:pressed {
            background-color: #E98C24;
            border-style: inset;
            margin-top: 1px;
        }
        QPushButton:disabled {
            background-color: #6C7A7D;
            border-color: #4C5658;
            color: #DCDCDC;
        }

        QPushButton#btnPass {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #38D5F4, stop:1 #0FB6C6);
            border-color: #0F92A8;
            color: #083A42;
        }
        QPushButton#btnPass:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #66E4FF, stop:1 #1FC9D9);
        }
    )";

    this->setStyleSheet(qss);

    // 给按钮设置 ObjectName 以便 QSS 识别
    btnPass->setObjectName("btnPass");
    btnPlay->setObjectName("btnPlay");
    btnNewGame->setObjectName("btnNewGame");

    // ================= UI 美化结束 =================
}

void MainWindow::setupConnections() {
    connect(listHuman, &QListWidget::itemSelectionChanged, this, &MainWindow::onSelectionChanged);
    connect(listHuman, &QListWidget::itemClicked, this, &MainWindow::onCardClicked);
    connect(btnPlay, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(btnPass, &QPushButton::clicked, this, &MainWindow::onPassClicked);
    // connect(btnCheatWin, &QPushButton::clicked, this, [this]() {
    //     if (judge && gameManager) {
    //         // 假设人类玩家 ID 为 0
    //         judge->debugDirectWin(0);

    //         // 禁用按钮防止重复点击
    //         btnCheatWin->setEnabled(false);
    //         btnPlay->setEnabled(false);
    //         btnPass->setEnabled(false);

    //         lblStatus->setText("调试模式：你已强制获胜，等待 AI 结束...");
    //     }
    // });
    connect(btnCheatWin, &QPushButton::clicked, this, [this]() {
        if (judge && gameManager) {
            // === 新增测试逻辑开始 ===
            // 强制将 队伍0 (你和P2) 的等级设为 13 (即打K)
            // 这样只要赢了这一局，等级就会 >= 14 (A)，触发 matchFinished
            judge->debugSetLevel(0, 13);
            // === 新增测试逻辑结束 ===

            // 假设人类玩家 ID 为 0
            judge->debugDirectWin(0);

            // 禁用按钮防止重复点击
            btnCheatWin->setEnabled(false);
            btnPlay->setEnabled(false);
            btnPass->setEnabled(false);

            lblStatus->setText("调试模式：已强制设为Lv.13并获胜，等待结算...");
        }
    });
    connect(btnNewGame, &QPushButton::clicked, this, [this]() {
        lblStatus->setText("正在初始化新游戏（级牌为2）...");
        if (humanPlayer) {
            humanPlayer->resetSelection();
        }
        // 清空 UI 中可能残留的上浮标记
        if (listHuman) {
            for (int i = 0; i < listHuman->count(); ++i) {
                if (auto *item = listHuman->item(i)) {
                    item->setData(Qt::UserRole, false);
                }
            }
            listHuman->viewport()->update();
        }
        btnCheatWin->setEnabled(true);
        QMetaObject::invokeMethod(gameManager, "startNewGame", Qt::QueuedConnection);
    });
    connect(judge, &Judge::lastPlayUpdated, this, [this](int playerId){
        Q_UNUSED(playerId);
        this->updateUI(); // 也可以实现一个更细粒度的 updatePlayerArea(playerId)
    });
    connect(gameManager, &GameManager::playerDealt, this, &MainWindow::updateUI);
    connect(gameManager, &GameManager::gameStarted, this, [this]() {
        lblStatus->setText("游戏开始！");
        updateUI();
    });


    // Judge 通知 UI 更新
    connect(judge, &Judge::playerHandChanged, this, &MainWindow::updateUI);
    //connect(judge, &Judge::lastPlayUpdated, this, &MainWindow::updateUI);
    connect(btnNewGame, &QPushButton::clicked, this, [this]() {
        lblStatus->setText("正在初始化新比赛(打2)...");
        // ... (原有的清理UI逻辑) ...
        QMetaObject::invokeMethod(gameManager, "startNewGame", Qt::QueuedConnection);
    });

    connect(judge, &Judge::gameFinished, this, [this]() {
        QString msg = "=== 本局结束 ===\n\n";

        // --- 新增代码开始：显示完赛顺序 ---
        auto placements = judge->getPreviousPlacements();
        if (!placements.empty()) {
            msg += "【出牌顺序】\n";
            for (size_t i = 0; i < placements.size(); ++i) {
                int pid = placements[i];
                QString name;

                // 为了让显示更直观，区分人类和电脑
                if (pid == 0) {
                    name = "玩家0 (你)";
                } else {
                    name = QString("玩家%1 (电脑)").arg(pid);
                }

                msg += QString("第%1名: %2\n").arg(i + 1).arg(name);
            }
            msg += "\n";
        }
        // --- 新增代码结束 ---

        // 显示等级信息
        msg += QString("【当前战况】\n队伍0 (你 & P2)：Lv.%1\n队伍1 (P1 & P3)：Lv.%2")
                   .arg(judge->getTeamLevel(0))
                   .arg(judge->getTeamLevel(1));

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "本局战报", msg + "\n\n是否继续下一局？",
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            lblStatus->setText("正在开始下一局...");
            if (humanPlayer) humanPlayer->resetSelection();

            // 调用下一局逻辑（保留等级）
            QTimer::singleShot(500, gameManager, &GameManager::startNextRound);
        } else {
            lblStatus->setText("游戏暂停，请点击“新游戏”重新开始");
        }
    });
    connect(judge, &Judge::matchFinished, this, [this](int winningTeam) {
        QString levelStr = (winningTeam == 0) ? "队伍0 (你)" : "队伍1 (AI)";
        QString msg = QString("恭喜！%1 成功打过 A (14级)！\n获得最终胜利！").arg(levelStr);

        QMessageBox::information(this, "最终冠军", msg);

        lblStatus->setText("比赛结束，冠军：" + levelStr);
        btnPlay->setEnabled(false);
        btnPass->setEnabled(false);
    });
    connect(judge, &Judge::turnChanged, this, &MainWindow::updateUI);
    // 连接清台信号
    connect(judge, &Judge::tableCleared, this, &MainWindow::updateUI);
    connect(judge, &Judge::playerReported, this, [this](int playerId, int remain){
        lblStatus->setText(QString("玩家 %1 报牌：剩余 %2 张").arg(playerId).arg(remain));
    });
    // 确保其他信号也连接了
    connect(judge, &Judge::lastPlayUpdated, this, &MainWindow::updateUI); // 这里不再需要 lambda 参数，全量刷新虽然浪费一点但逻辑最稳
    connect(judge, &Judge::turnChanged, this, &MainWindow::updateUI);
    connect(judge, &Judge::askForTribute, this, [this](int playerId, bool isReturn) {
        if (playerId != 0) return; // 只处理人类

        QString title = isReturn ? "请还贡" : "请进贡";
        QString msg = isReturn ? "请选择一张牌还给进贡者（任意牌）"
                               : "请选择你手中最大的牌进贡（红桃级牌除外）";

        lblStatus->setText(title + "：" + msg);

        // 启用交互，但修改按钮文字
        btnPlay->setText("确认选择");
        btnPlay->setEnabled(true);
        btnPass->setEnabled(false); // 进贡/还贡不能跳过

        // 清空上次选择
        humanPlayer->resetSelection();
        refreshSelectionSummary();
    });

    connect(judge, &Judge::tributeResult, this, [this](int payer, int receiver, const Card& card, bool isReturn) {
        QString action = isReturn ? "还贡" : "进贡";
        QString msg = QString("玩家 %1 向 玩家 %2 %3了一张： %4")
                          .arg(payer).arg(receiver).arg(action)
                          .arg(QString::fromStdString(card.toString()));

        QMessageBox::information(this, "进贡通知", msg);
        updateUI(); // 刷新手牌显示
    });

    connect(judge, &Judge::tributeResisted, this, [this](int playerId) {
        QMessageBox::information(this, "抗贡", QString("玩家 %1 拥有双大王，触发抗贡！").arg(playerId));
    });
}
void MainWindow::onCardClicked(QListWidgetItem *item) {
    if (!humanPlayer) return;
    int index = listHuman->row(item);
    humanPlayer->toggleSelectCard(index);
    bool nowSelected = humanPlayer->isIndexSelected(index);
    item->setData(Qt::UserRole, nowSelected); // 切换
    listHuman->viewport()->update(); // 强制重绘，触发 Delegate 的上浮动画
    refreshSelectionSummary();
}

void MainWindow::onSelectionChanged() {
    QList<QListWidgetItem*> selectedItems = listHuman->selectedItems();

    // 恢复所有牌的默认外观
    // for (int i = 0; i < listHuman->count(); ++i) {
    //     QListWidgetItem* item = listHuman->item(i);
    //     item->setBackground(Qt::white);
    //     item->setSizeHint(QSize(40, 100));  // 默认卡牌大小
    // }

    // // 高亮选中的牌并获取选中的 Card
    // for (QListWidgetItem* item : selectedItems) {
    //     item->setBackground(Qt::yellow);
    //     item->setSizeHint(QSize(40, 120)); // 稍微变高一点，模拟“上移”的感觉

    //     // 找到该 item 的索引
    //     int index = listHuman->row(item);
    //     if (index >= 0 && humanPlayer) {
    //         // 获取该索引对应的 Card 对象
    //         Card selectedCard = humanPlayer->getHandCopy()[index];

    //         // 通知 humanPlayer 切换选中状态（传入 Card）
    //         humanPlayer->toggleSelectCard(selectedCard);
    //     }
    // }

    // qDebug() << "当前选中牌数：" << selectedItems.size();
}

void MainWindow::onPlayClicked() {
    if (!humanPlayer || !judge) return;

    // --- 新增：进贡阶段处理 ---
    if (judge->getGamePhase() != GamePhase::Playing) {
        auto cards = humanPlayer->getSelectedCards();
        if (cards.size() != 1) {
            QMessageBox::warning(this, "提示", "进贡/还贡只能选择一张牌！");
            return;
        }

        // 提交给 Judge
        bool ok = judge->submitTribute(0, cards[0]);
        if (ok) {
            humanPlayer->resetSelection();
            btnPlay->setText("出牌"); // 恢复按钮文字（虽然马上会被禁用）
            btnPlay->setEnabled(false);
            lblStatus->setText("等待其他玩家...");
        } else {
            QMessageBox::warning(this, "错误", "选择的牌不符合进贡规则（必须是最大的牌）");
        }
        return;
    }
    if (!humanPlayer || !judge) return;
    if (judge->getCurrentTurn() != 0) {
        QMessageBox::information(this, "提示", "现在还没轮到你出牌");
        return;
    }
    auto cards = humanPlayer->getSelectedCards();
    if (cards.empty()) {
        std::vector<Card> fallbackSelection;
        std::vector<Card> humanHand = humanPlayer->getHandCopy();
        for (int i = 0; i < listHuman->count() && i < static_cast<int>(humanHand.size()); ++i) {
            QListWidgetItem* item = listHuman->item(i);
            if (item && item->data(Qt::UserRole).toBool()) {
                fallbackSelection.push_back(humanHand[i]);
            }
        }
        cards = fallbackSelection;
    }

    if (cards.empty()) {
        QMessageBox::information(this, "提示", "请先选择要出的牌");
        return;
    }
    // 尝试出牌
    bool success = judge->playHumanCard(cards);

    if (success) {
        // 【修复核心】：出牌成功后，必须清空 HumanPlayer 内部的选中状态
        humanPlayer->resetSelection();

        // 重新刷新界面状态
        updateUI();
    } else {
        QMessageBox::warning(this, "出牌失败", "出牌不符合规则或管不上上家");
    }
}
void MainWindow::onPassClicked() {
    if (judge) {
        lblStatus->setText("你选择了过");
        judge->humanPass();
        updateUI();
    }
}

void MainWindow::updateUI() {
    if (!humanPlayer || !judge) return;
    bool isHumanTurn = (judge->getCurrentTurn() == 0);
    btnPlay->setEnabled(isHumanTurn);
    btnPass->setEnabled(isHumanTurn);
    auto rankToString = [](int rank) -> QString {
        switch (rank) {
        case 11: return "J";
        case 12: return "Q";
        case 13: return "K";
        case 14: return "A";
        case 15: return "2";
        default: return QString::number(rank);
        }
    };
    // 1. 刷新人类手牌（保持不变，但确保用新的 Delegate）
    listHuman->clear();
    std::vector<Card> humanHand = humanPlayer->getHandCopy();
    for (size_t i = 0; i < humanHand.size(); ++i) {
        const auto& card = humanHand[i];
        QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(card.toString()));
        bool selected = humanPlayer->isIndexSelected(static_cast<int>(i));
        item->setData(Qt::UserRole, selected); // 初始化选中状态
        listHuman->addItem(item);
    }

    // 2. 刷新 AI 剩余张数
    // 更新侧边栏文字
    listAI1->clear(); listAI1->addItem(QString("剩 %1 张").arg(judge->getPlayerHandCount(1)));
    listAI2->clear(); listAI2->addItem(QString("剩 %1 张").arg(judge->getPlayerHandCount(2)));
    listAI3->clear(); listAI3->addItem(QString("剩 %1 张").arg(judge->getPlayerHandCount(3)));

    // 3. 刷新桌面出牌区域（核心修改）
    auto updateTableArea = [&](int playerId, QListWidget* area) {
        area->clear();

        // 情况A：玩家 Pass 了
        if (judge->hasPlayerPassed(playerId)) {
            QListWidgetItem* item = new QListWidgetItem("不要");
            item->setFlags(Qt::NoItemFlags); // 不可选中
            area->addItem(item);
            return;
        }

        // 情况B：玩家出牌了
        std::vector<Card> played = judge->getPlayerLastPlay(playerId);
        if (!played.empty()) {
            for (const auto& c : played) {
                QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(c.toString()));
                item->setFlags(Qt::NoItemFlags);
                area->addItem(item);
            }
        }
        // 情况C：新的一轮还没轮到他，或者他是庄家刚开始 -> 显示空白（什么都不做）
    };

    updateTableArea(0, playCenterBottom);
    updateTableArea(1, playRight);
    updateTableArea(2, playTop);
    updateTableArea(3, playLeft);

    // 4. 更新状态文字
    if (judge->getCurrentTurn() == 0) {
        lblStatus->setText("轮到你了：请出牌");
    } else {
        lblStatus->setText(QString("等待玩家 %1 出牌...").arg(judge->getCurrentTurn()));
    }

    // 显示双方等级与本局级牌
    if (lblLevels) {
        lblLevels->setText(QString("队伍0 等级：%1 | 队伍1 等级：%2")
                               .arg(judge->getTeamLevel(0))
                               .arg(judge->getTeamLevel(1)));
    }
    if (lblLevelCard) {
        int levelRank = judge->getCurrentLevelRank();
        QString rankText = levelRank > 0 ? rankToString(levelRank) : QStringLiteral("--");
        lblLevelCard->setText(QString("本局级牌：♥%1（队伍%2）")
                                  .arg(rankText)
                                  .arg(judge->getCurrentLevelTeam()));
    }

    // 5. 按钮控制
    if (judge->getCurrentTurn() == 0) {
        btnPlay->setEnabled(true);
        listHuman->setEnabled(true);

        // 如果桌面为空（你是庄家），不能过
        if (judge->getLastCards().empty()) {
            btnPass->setEnabled(false);
            btnPass->setText("必须出牌");
        } else {
            btnPass->setEnabled(true);
            btnPass->setText("不要");
        }
    } else {
        btnPlay->setEnabled(false);
        btnPass->setEnabled(false);
        listHuman->setEnabled(false);
        btnPass->setText("不要");
    }

    refreshSelectionSummary();
}

void MainWindow::refreshSelectionSummary() {
    if (!humanPlayer || !lblSelection) return;

    std::vector<Card> selected = humanPlayer->getSelectedCards();
    QStringList parts;
    for (const auto &card : selected) {
        parts << QString::fromStdString(card.toString());
    }

    QString detail = parts.isEmpty() ? QStringLiteral("无") : parts.join(QStringLiteral(", "));
    lblSelection->setText(QStringLiteral("已选中：%1 张 (%2)")
                              .arg(selected.size())
                              .arg(detail));
}
