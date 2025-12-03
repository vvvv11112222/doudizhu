#include "humanplayer.h"
#include <algorithm>
#include <QDebug>

HumanPlayer::HumanPlayer(int id, const std::string& name, QObject* parent)
    : QObject(parent), Player(id, name)
{
    selectedIndices_.clear();
}

void HumanPlayer::requestMove(const std::vector<Card>& lastPlay)
{
    emit showHandRequested(ID, handCards, lastPlay);
    selectedIndices_.clear();
}

void HumanPlayer::toggleSelectCard(int index) {
    if (index < 0 || index >= static_cast<int>(handCards.size())) {
        qDebug() << "toggleSelectCard: invalid index!";
        return;
    }

    auto it = std::find(selectedIndices_.begin(), selectedIndices_.end(), index);
    if (it != selectedIndices_.end()) {
        // 已选中 -> 取消选择
        selectedIndices_.erase(it);
    } else {
        // 新选中 -> 添加
        selectedIndices_.push_back(index);
    }

    // 保持索引有序，确保出牌顺序稳定且允许重复牌面共存
    std::sort(selectedIndices_.begin(), selectedIndices_.end());

    qDebug() << "已选中牌：" << selectedIndices_.size();
}

// 用户确认出牌（UI 点击“出牌”）
void HumanPlayer::onUserConfirmPlay()
{
    std::vector<Card> toPlay = selectedCardsFromIndices();
    if (toPlay.empty()) {
        emit invalidMove(QStringLiteral("未选中任何牌"));
        return;
    }
    emit moveReady(toPlay);
    selectedIndices_.clear();
}

// 用户点击“过”
void HumanPlayer::onUserPass()
{
    selectedIndices_.clear();
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
    return selectedCardsFromIndices();
}

bool HumanPlayer::isIndexSelected(int index) const
{
 return std::find(selectedIndices_.begin(), selectedIndices_.end(), index) != selectedIndices_.end();
}

std::vector<Card> HumanPlayer::selectedCardsFromIndices() const
{
    std::vector<Card> res;
    if (selectedIndices_.empty()) return res;

    for (int idx : selectedIndices_) {
        if (idx >= 0 && idx < static_cast<int>(handCards.size())) {
            res.push_back(handCards[idx]);
        }
    }
    return res;
}
