#ifndef CARD_H
#define CARD_H

#include <string>

enum class Suit {
    Spades,    // 黑桃
    Clubs,     // 梅花
    Diamonds,  // 方块
    Hearts,    // 红心
    None       // 用于大小王
};

// 统一枚举名称，避免混淆
enum class Rank {
    Three = 3, Four, Five, Six, Seven, Eight, Nine, Ten,
    Jack, Queen, King, Ace, Two,
    S, // 小王
    B    // 大王
};

class Card {
public:
    Card(Rank r = Rank::Three, Suit s = Suit::Clubs) noexcept; // 声明构造函数

    Rank getRank() const noexcept { return rank; }
    Suit getSuit() const noexcept { return suit; }

    std::string getSuitString() const;
    std::string getRankString() const;
    int getRankInt() const noexcept;
    std::string toString() const;

    bool operator<(const Card& other) const noexcept;
    bool operator==(const Card& other) const noexcept;

private:
    Rank rank;
    Suit suit;
};

#endif // CARD_H
