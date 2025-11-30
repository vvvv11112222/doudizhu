#ifndef PLAYER_H
#define PLAYER_H

#include "card.h"
#include <array>
#include <vector>

class Judge; // 前向声明（如需要）

class Player{
public:
    Player(int id, const std::string& name = "");
    virtual ~Player();
    // 只读访问
    int getID() const noexcept;
    const std::string& getName() const noexcept;
    size_t getCardCount() const noexcept;
    std::array<int,15> getRankCounts() const;
    std::vector<Card> getHandCopy() const;
    void clearHand();
    void setHand(const std::vector<Card>& cards);   // 替换整个手牌
    void addCards(const std::vector<Card>& cards);   // 保留原追加语义（可选）
    // 手牌操作（主 API）
    // receiveCards: 发牌时调用（将 cards 添加到玩家手牌）
    virtual void receiveCards(const std::vector<Card>& cards);

    // playCards: 尝试从手牌移除这些牌并更新辅助索引（不负责规则合法性校验）
    // 返回 true 表示成功移除了这些牌，false 表示手牌中不包含所请求的组合
    virtual bool playCards(const std::vector<Card>& cards);

    // pass: 玩家选择过（不出牌）
    virtual void pass(); // 如果需要可改为返回状态 enum

    virtual void updateRankCounts();

protected:
    int ID;
    std::string name;
    std::vector<Card> handCards;          // 主存储（按需排序）

    // 3 3
    // J 11
    // Q 12
    // k 13
    // A 14
    // 2 15
    std::array<int, 18> rankCounts_{};     // 按 rank 的计数（A..K）
};

#endif // PLAYER_H
