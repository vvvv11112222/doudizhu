#include "humanplayer.h"
#include <algorithm>
#include <QDebug>

HumanPlayer::HumanPlayer(int id, const std::string& name, QObject* parent)
    : QObject(parent), Player(id, name)
{
    selectedCards_.clear();
}

// 上层请求玩家开始出牌（异步）
// lastPlay 可为空（传空 vector 表示无上家出牌）
void HumanPlayer::requestMove(const std::vector<Card>& lastPlay)
{
    // 发出信号通知 UI 展示该玩家手牌和上家出的牌（UI 负责呈现，HumanPlayer 不阻塞）
    emit showHandRequested(ID, handCards, lastPlay);
    // 清除上一次的选择，等待用户再次选择
    selectedCards_.clear();
}

void HumanPlayer::toggleSelectCard(const Card& card) {
    // 检查传入的 card 是否有效
    if (std::find(handCards.begin(), handCards.end(), card) == handCards.end()) {
        qDebug() << "toggleSelectCard: card not found in hand!";
        return;
    }

    auto it = std::find(selectedCards_.begin(), selectedCards_.end(), card);
    if (it != selectedCards_.end()) {
        // 已选中 -> 取消选择
        selectedCards_.erase(it);
    } else {
        // 新选中 -> 添加
        selectedCards_.push_back(card);
    }

    // 保持 cards 有序（可以根据牌的 rank 或其他标准排序）
    std::sort(selectedCards_.begin(), selectedCards_.end(), [](const Card& a, const Card& b) {
        return a.getRankInt() < b.getRankInt(); // 根据 rank 排序
    });

    // 可选：通知 UI 更新（如果需要 UI 监听 HumanPlayer 信号，可新增 signal）
    // 这里我们只输出调试信息
    qDebug() << "已选中牌：" << selectedCards_.size();
}

// 用户确认出牌（UI 点击“出牌”）
void HumanPlayer::onUserConfirmPlay()
{
    // 把当前选中的卡牌转为实际的 Card 向量
    std::vector<Card> toPlay = selectedCards_;
    if (toPlay.empty()) {
        // 没选牌就确认：认为是非法（可以让 UI 先提示），这里发 invalidMove
        emit invalidMove(QStringLiteral("未选中任何牌"));
        return;
    }

    // 发信号把候选出牌交给上层（GameManager / Judge）做合法性判断与后续处理
    emit moveReady(toPlay);

    // 通常上层在收到 moveReady 后会调用 Judge::isValidPlay 并在合法时调用 Player::playCards
    // 我们这里将本地选择清空，避免重复提交
    selectedCards_.clear();
}

// 用户点击“过”
void HumanPlayer::onUserPass()
{
    selectedCards_.clear();
    emit passed();
}

// 用户请求提示（UI 点击“提示”）
void HumanPlayer::onUserRequestHint()
{
    emit requestHintFromJudge(ID);
}

std::vector<Card> HumanPlayer::getHandCopy() const
{
    return handCards;
}

// 返回当前已选中的牌（拷贝）
std::vector<Card> HumanPlayer::getSelectedCards() const
{
    return selectedCards_;
}

bool HumanPlayer::isCardSelected(const Card& card) const
{
    return std::find(selectedCards_.begin(), selectedCards_.end(), card) != selectedCards_.end();
}

// 内部：把 selectedCards_ 转换为 Card 向量（按选中顺序）
std::vector<Card> HumanPlayer::selectedCardsFromIndices() const
{
    std::vector<Card> res;
    if (selectedCards_.empty()) return res;

    // 去重并确保索引合法（selectedCards_ 应该已排序）
    std::vector<Card> selected = selectedCards_;
    // 去重
    selected.erase(std::unique(selected.begin(), selected.end()), selected.end());

    // 不需要再做索引查找了，因为选中的已经是 Card 对象
    res = selected;

    return res;
}
