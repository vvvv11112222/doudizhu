#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <algorithm>
#include <QTimer>
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
    resize(1000, 700);

    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    listAI1 = new QListWidget;
    listAI2 = new QListWidget;
    listAI3 = new QListWidget;
    listHuman = new QListWidget;
    lblLastPlay = new QLabel("上家出牌：无");
    lblStatus = new QLabel("游戏状态：等待开始");
    lblLevels = new QLabel("队伍等级：-- / --");
    lblLevelCard = new QLabel("本局级牌：待定");
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
    auto createPlayerLayout = [](QString name, QWidget* playArea) -> QVBoxLayout* {
        QVBoxLayout* box = new QVBoxLayout;

        // 模拟头像
        QLabel* avatar = new QLabel;
        avatar->setPixmap(QPixmap(60, 60)); // 实际应用请用 setPixmap(QPixmap(":/img/avatar.png"));
        avatar->setStyleSheet("background-color: #DDD; border-radius: 30px; border: 2px solid white;");
        avatar->setFixedSize(60, 60);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setText(name.left(1)); // 显示名字首字母当头像
        avatar->setStyleSheet("QLabel { background-color: #F1C40F; border-radius: 30px; color: #4A235A; font-size: 24px; border: 2px solid white; }");

        QLabel* nameLbl = new QLabel(name);

        box->addWidget(avatar, 0, Qt::AlignCenter);
        box->addWidget(nameLbl, 0, Qt::AlignCenter);
        box->addWidget(playArea, 0, Qt::AlignCenter);
        return box;
    };
    // --- 布局：我们做一个 3x3 风格的中间区域（Top / Left / Center / Right / Bottom） ---
    QVBoxLayout *mainLay = new QVBoxLayout;
    // Top row: player2 label + playTop (centered)
    QHBoxLayout *rowTop = new QHBoxLayout;
    rowTop->addStretch();
    QVBoxLayout *topBox = createPlayerLayout("AI 电脑 2", playTop);
    rowTop->addLayout(topBox);
    rowTop->addStretch(); // 保持居中
    // Middle row: left play, center info, right play
    QHBoxLayout *rowMiddle = new QHBoxLayout;
    // left column (player3)
    QVBoxLayout *leftBox = createPlayerLayout("AI 电脑 3", playLeft);
    rowMiddle->addLayout(leftBox);

    // center column: can show game status / last plays overall
    QVBoxLayout *centerBox = new QVBoxLayout;
    centerBox->addWidget(lblLastPlay, 0, Qt::AlignCenter);
    centerBox->addWidget(lblStatus, 0, Qt::AlignCenter);
    centerBox->addWidget(lblLevels, 0, Qt::AlignCenter);
    centerBox->addWidget(lblLevelCard, 0, Qt::AlignCenter);
    rowMiddle->addLayout(centerBox, 1); // give center more stretch

    // right column (player1)
    QVBoxLayout *rightBox = createPlayerLayout("AI 电脑 1", playRight);
    rowMiddle->addLayout(rightBox);
    // Bottom row: human play area above human hand
    QHBoxLayout *rowBottom = new QHBoxLayout;
    rowBottom->addStretch();
    QVBoxLayout *humanPlayBox = new QVBoxLayout;
    humanPlayBox->addWidget(new QLabel("桌面 - 你的出牌"), 0, Qt::AlignCenter);
    humanPlayBox->addWidget(playCenterBottom);
    humanPlayBox->addWidget(new QLabel("你的手牌"));
     humanPlayBox->addWidget(lblSelection);
    humanPlayBox->addWidget(listHuman);
    rowBottom->addLayout(humanPlayBox);
    rowBottom->addStretch();

    // Buttons row
    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(btnNewGame);
    buttons->addWidget(btnPlay);
    buttons->addWidget(btnPass);
    buttons->addWidget(btnCheatWin);

    mainLay->addLayout(rowTop);
    mainLay->addLayout(rowMiddle);
    mainLay->addLayout(rowBottom);
    mainLay->addLayout(buttons);

    central->setLayout(mainLay);
    // 设置委托
    CardDelegate* delegate = new CardDelegate(this);
    listHuman->setItemDelegate(delegate);
    playTop->setItemDelegate(delegate);
    playLeft->setItemDelegate(delegate);
    playRight->setItemDelegate(delegate);
    playCenterBottom->setItemDelegate(delegate);

    auto polishList = [](QListWidget* list, int baseIconW = 90, int baseIconH = 120, int overlap = -40) {
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
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setStyleSheet("background: transparent; border: none;");
        list->setFocusPolicy(Qt::NoFocus);

        // 设置固定高度：至少为格子高度 + 顶部/底部外边距
        int extraVerticalMargin = 10; // 额外空白
        list->setFixedHeight(gridH + extraVerticalMargin);
    };

    polishList(listHuman, 90, 120, -35);      // 底部手牌，图标 90x120，重叠 -35
    polishList(playTop, 70, 90, -40);         // 顶部AI出牌区，稍小
    polishList(playLeft, 70, 90, -40);
    polishList(playRight, 70, 90, -40);
    polishList(playCenterBottom, 80, 100, -38); // 中央桌面显示区

    // 2. 全局 QSS 样式表
    QString qss = R"(
        /* 全局背景：深绿色牌桌风格 */
        QMainWindow {
            background-color: #2E5C38; /* 经典牌桌绿 */
            background-image: url(:/images/table_bg.png); /* 如果你有纹理图最好 */
        }

        /* 标签文字：白色、加粗、投影 */
        QLabel {
            color: #FFFFFF;
            font-family: "Microsoft YaHei";
            font-size: 14px;
            font-weight: bold;
        }

        /* 列表控件：透明背景，无边框 */
        QListWidget {
            background-color: transparent;
            border: none;
            outline: none; /* 去掉选中时的虚线框 */
        }

        /* 按钮通用风格：橙色渐变，圆角 */
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFD700, stop:1 #FF8C00);
            border: 2px solid #B8860B;
            border-radius: 15px;
            color: #552200;
            font-family: "SimHei";
            font-size: 16px;
            font-weight: bold;
            padding: 5px 15px;
            min-width: 80px;
            min-height: 30px;
        }
        /* 按钮悬停 */
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFE44D, stop:1 #FF9F33);
            margin-top: 2px; /* 模拟按压前的浮动感 */
        }
        /* 按钮按下 */
        QPushButton:pressed {
            background-color: #E67E22;
            border-style: inset;
            margin-top: 4px;
        }
        /* 禁用状态：灰色 */
        QPushButton:disabled {
            background-color: #7F8C8D;
            border-color: #555;
            color: #DDD;
        }

        /* 特殊按钮颜色：不要/过 (蓝色系) */
        QPushButton#btnPass {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3498DB, stop:1 #2980B9);
            border-color: #1F618D;
            color: white;
        }
        QPushButton#btnPass:hover {
             background-color: #5DADE2;
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
    connect(btnCheatWin, &QPushButton::clicked, this, [this]() {
        if (judge && gameManager) {
            // 假设人类玩家 ID 为 0
            judge->debugDirectWin(0);

            // 禁用按钮防止重复点击
            btnCheatWin->setEnabled(false);
            btnPlay->setEnabled(false);
            btnPass->setEnabled(false);

            lblStatus->setText("调试模式：你已强制获胜，等待 AI 结束...");
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
