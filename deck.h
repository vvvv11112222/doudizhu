#ifndef DECK_H
#define DECK_H

#include "card.h"
#include <vector>
#include <random>

class Deck {
public:
    Deck();

    // 构件牌堆
    void buildDeck();

    // 洗牌
    void shuffleDeck();

    // 直接按轮流方式发完（直到牌堆为空）并返回 numPlayers 个手牌
    std::vector<std::vector<Card>> dealRoundRobin(int numPlayers);

    // 取得当前底牌（调试/显示用）
    const std::vector<Card>& cards() const noexcept { return cards_; }

    // 清空牌堆
    void clear() noexcept { cards_.clear(); }

private:
    std::vector<Card> cards_;
    std::mt19937 rng_;
};

#endif // DECK_H
