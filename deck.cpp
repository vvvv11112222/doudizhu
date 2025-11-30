#include "deck.h"
#include <algorithm>
#include <qdebug.h>
// 需要显式列出 Rank / Suit 的所有值，不能直接 ++ 枚举 class
static const std::vector<Suit> ALL_SUITS = {
    Suit::Spades, Suit::Clubs, Suit::Diamonds, Suit::Hearts, Suit::None
};

// 注意：你的 Rank enum 从 Three=3 开始到 Two
// 如果你在 Rank 中添加了 JokerSmall/JokerBig，
// 可在下面加入它们来支持 includeJokers=true。
static const std::vector<Rank> ALL_RANKS = {
    Rank::Three, Rank::Four, Rank::Five, Rank::Six, Rank::Seven,
    Rank::Eight, Rank::Nine, Rank::Ten, Rank::Jack, Rank::Queen,
    Rank::King, Rank::Ace, Rank::Two,
    Rank::S, Rank::B
};
Deck::Deck() : rng_(std::random_device{}()) {}

void Deck::buildDeck() {
    cards_.clear();
    const int numDecks = 2;
    for (int d = 0; d < numDecks; ++d) {
        for (int rv = static_cast<int>(Rank::Three); rv <= static_cast<int>(Rank::Two); ++rv) {
            Rank r = static_cast<Rank>(rv);
            cards_.emplace_back(r, Suit::Spades);
            cards_.emplace_back(r, Suit::Clubs);
            cards_.emplace_back(r, Suit::Diamonds);
            cards_.emplace_back(r, Suit::Hearts);
        }
        cards_.emplace_back(Rank::S, Suit::None);
        cards_.emplace_back(Rank::B,   Suit::None);
    }

    qDebug() << "Deck::buildDeck() 完成, 总牌数 = " << static_cast<int>(cards_.size()); // 期望 108
}
void Deck::shuffleDeck() {
    std::shuffle(cards_.begin(), cards_.end(), rng_);
}

std::vector<std::vector<Card>> Deck::dealRoundRobin(int numPlayers) {
    std::vector<std::vector<Card>> hands;
    hands.assign(numPlayers, std::vector<Card>());
    if (numPlayers <= 0) return hands;

    int p = 0;
    for (size_t i = 0; i < cards_.size(); ++i) {
        hands[p].push_back(cards_[i]);
        p = (p + 1) % numPlayers;
    }
    return hands;
}
