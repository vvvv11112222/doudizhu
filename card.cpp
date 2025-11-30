#include "card.h"

// 构造函数实现
Card::Card(Rank r, Suit s) noexcept : rank(r), suit(s) {}

std::string Card::getSuitString() const {
    switch (suit) {
    case Suit::Spades:   return "♠";
    case Suit::Clubs:    return "♣";
    case Suit::Diamonds: return "♦";
    case Suit::Hearts:   return "♥";
    default:             return "";
    }
}

std::string Card::getRankString() const {
    switch (rank) {
    case Rank::Three: return "3";
    case Rank::Four:  return "4";
    case Rank::Five:  return "5";
    case Rank::Six:   return "6";
    case Rank::Seven: return "7";
    case Rank::Eight: return "8";
    case Rank::Nine:  return "9";
    case Rank::Ten:   return "10";
    case Rank::Jack:  return "J";
    case Rank::Queen: return "Q";
    case Rank::King:  return "K";
    case Rank::Ace:   return "A";
    case Rank::Two:   return "2";
    case Rank::S: return "jokerSmall";
    case Rank::B: return "jokerBig";
    default:          return "";
    }
}

int Card::getRankInt() const noexcept {
    switch (rank) {
    case Rank::Three: return 3;
    case Rank::Four:  return 4;
    case Rank::Five:  return 5;
    case Rank::Six:   return 6;
    case Rank::Seven: return 7;
    case Rank::Eight: return 8;
    case Rank::Nine:  return 9;
    case Rank::Ten:   return 10;
    case Rank::Jack:  return 11;
    case Rank::Queen: return 12;
    case Rank::King:  return 13;
    case Rank::Ace:   return 14;
    case Rank::Two:   return 15;
    case Rank::S: return 16;
    case Rank::B: return 17;
    default:          return 0;
    }
}

std::string Card::toString() const {
    return getSuitString() + getRankString();
}

bool Card::operator<(const Card& other) const noexcept {
    // 掼蛋/斗地主排序：先比点数，点数相同比花色
    if (getRankInt() != other.getRankInt())
        return getRankInt() < other.getRankInt();
    return static_cast<int>(suit) < static_cast<int>(other.suit);
}

bool Card::operator==(const Card& other) const noexcept {
    return rank == other.rank && suit == other.suit;
}
