#include "player.h"
#include "card.h"
#include <algorithm>
#include <iostream>
#include "qdebug.h"
Player::Player(int id, const std::string& name) : ID(id), name(name) {}
Player::~Player() = default;

int Player::getID() const noexcept { return ID; }
const std::string& Player::getName() const noexcept { return name; }
size_t Player::getCardCount() const noexcept { return handCards.size(); }
std::vector<Card> Player::getHandCopy() const {
    return handCards;
}
void Player::clearHand() {
    handCards.clear();
    rankCounts_.fill(0);
}
// 替换手牌（用于每局发牌时使用）
void Player::setHand(const std::vector<Card>& cards) {
    handCards = cards;         // 直接替换，不追加
    qDebug() << "玩家" << ID << "手牌设置完成，数量:" << handCards.size();
    updateRankCounts();        // 更新 rankCounts_、并可排序 handCards
}

// 追加手牌（如果你确实需要追加语义）
void Player::addCards(const std::vector<Card>& cards) {
    handCards.insert(handCards.end(), cards.begin(), cards.end());
    updateRankCounts();
}

// receiveCards: 将 cards 添加到玩家手牌
void Player::receiveCards(const std::vector<Card>& cards) {
    // 直接追加
    for (const auto &c : cards) {
        handCards.push_back(c);
    }
    // 重新计算 rankCounts_
    updateRankCounts();
}
// playCards: 尝试从手牌移除这些牌（不负责规则合法性判断）
// 返回 true 表示成功移除了这些牌，false 表示玩家手中不包含所请求的组合
bool Player::playCards(const std::vector<Card>& cards) {
    // 快速检查：每张请求出的牌都必须在手牌中（考虑重复牌）
    // 采用临时副本判断（避免部分删除后失败造成不一致）
    std::vector<Card> temp = handCards;

    for (const auto &c : cards) {
        auto it = std::find(temp.begin(), temp.end(), c);
        if (it == temp.end()) {
            // 有一张牌在手牌中找不到 -> 失败
            return false;
        }
        temp.erase(it); // 从临时手牌中删掉，继续检查剩余
    }

    // 若检查通过，则从真实 handCards 中移除这些牌
    for (const auto &c : cards) {
        auto it = std::find(handCards.begin(), handCards.end(), c);
        if (it != handCards.end()) {
            handCards.erase(it);
        } else {
            // 理论上不会到这里（因为前面检查已通过），为保险起见直接返回 false
            return false;
        }
    }

    // 更新计数表
    updateRankCounts();
    return true;
}

// pass: 玩家选择不出（默认不做任何状态修改，子类可 override）
void Player::pass() {
    // 默认实现：空操作（可在子类记录日志/时间戳等）
    std::cout << "Player " << ID << " (" << name << ") passes.\n";
}



void Player::updateRankCounts() {
    // 清零
    rankCounts_.fill(0);

    // 统计每张牌对应的 rank 索引
    for (const auto &c : handCards) {
        int idx = c.getRankInt();
        if (idx >= 3 && idx < static_cast<int>(rankCounts_.size())) {
            rankCounts_[idx] += 1;
        } else {
            // 如果出现未知 rank，可在这里打印调试信息或断言
            // qDebug() << "Unknown rank in updateRankCounts";
        }
    }

    // 按 rank + suit 排序（便于显示与做牌型检测前的顺序）
    std::sort(handCards.begin(), handCards.end(), [](const Card &a, const Card &b) {
        int ia = a.getRankInt();
        int ib = b.getRankInt();
        if (ia != ib) return ia < ib;
        // 如果花色也是 enum class，按其 underlying value 比较（或显式 switch）
        return static_cast<int>(a.getSuit()) < static_cast<int>(b.getSuit());
    });
}


