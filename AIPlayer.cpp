// AIPlayer.cpp
#include "aiplayer.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <QTimer>
#include <QDebug>

AIPlayer::AIPlayer(int id, const std::string& name, QObject* parent)
    : QObject(parent), Player(id, name),
    rng_(static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count()))
{
}

HandType AIPlayer::evaluateHandType(const std::vector<Card>& cards, int levelRank) const {
    HandMatcher matcher(cards, levelRank);
    return matcher.analyze().type;
}

// 主值：返回用于比较同类牌大小的关键点（例如对子/单张等取 rank int）
int AIPlayer::primaryRank(const std::vector<Card>& cards, int levelRank) const {
    HandMatcher matcher(cards, levelRank);
    return matcher.analyze().primaryRank;
}

// 优化 canBeat 逻辑，防止 AI 试图用 3 张牌管 1 张牌等低级错误
bool AIPlayer::canBeat(const std::vector<Card>& candidate, const std::vector<Card>& base, int levelRank) const {
    if (candidate.empty() || base.empty()) return false;

    HandMatcher candMatcher(candidate, levelRank);
    HandMatcher baseMatcher(base, levelRank);

    PlayInfo candInfo = candMatcher.analyze();
    PlayInfo baseInfo = baseMatcher.analyze();

    if (candInfo.type == HandType::Invalid || baseInfo.type == HandType::Invalid) return false;

    // 1. 炸弹及天王炸逻辑
    bool candBomb = candInfo.type == HandType::Bomb || candInfo.type == HandType::StraightFlush || candInfo.type == HandType::TianWang;
    bool baseBomb = baseInfo.type == HandType::Bomb || baseInfo.type == HandType::StraightFlush || baseInfo.type == HandType::TianWang;

    if (candBomb && !baseBomb) return true;
    if (!candBomb && baseBomb) return false;
    if (candBomb && baseBomb) {
        if (candInfo.type != baseInfo.type) return static_cast<int>(candInfo.type) > static_cast<int>(baseInfo.type);
        if (candInfo.size != baseInfo.size) return candInfo.size > baseInfo.size;
        return candInfo.primaryRank > baseInfo.primaryRank;
    }

    // 2. 普通牌逻辑：必须【类型相同】且【张数相同】
    if (candInfo.type != baseInfo.type) return false;
    if (candidate.size() != base.size()) return false;

    return candInfo.primaryRank > baseInfo.primaryRank;
}

// 生成可能出牌（单张 / 对子 / 三张 / 三带一 / 炸弹(4+)）
// 返回 vector of candidate plays (each is vector<Card>)
std::vector<std::vector<Card>> AIPlayer::generatePossiblePlays(int levelRank) const {
    std::vector<std::vector<Card>> out;
    std::vector<Card> hand = getHandCopy();

    if (hand.empty()) return out;

    // sort by rank then suit for deterministic grouping
    std::sort(hand.begin(), hand.end(), [](const Card& a, const Card& b){
        if (a.getRankInt() != b.getRankInt()) return a.getRankInt() < b.getRankInt();
        return static_cast<int>(a.getSuit()) < static_cast<int>(b.getSuit());
    });

    // singles
    for (const auto& c : hand) out.push_back(std::vector<Card>{c});

    // group by rank
    std::map<int, std::vector<int>> idx; // rankInt -> indices in hand
    for (int i = 0; i < (int)hand.size(); ++i) {
        idx[hand[i].getRankInt()].push_back(i);
    }

    // pairs/triples/bombs and triple+single (3+1)
    for (auto &kv : idx) {
        const std::vector<int>& ids = kv.second;
        if (ids.size() >= 2) {
            // pair (take first two)
            out.push_back(std::vector<Card>{ hand[ids[0]], hand[ids[1]] });
        }
        if (ids.size() >= 3) {
            out.push_back(std::vector<Card>{ hand[ids[0]], hand[ids[1]], hand[ids[2]] });
            // three+single: combine triple with any other single card
            for (int j = 0; j < (int)hand.size(); ++j) {
                if (std::find(ids.begin(), ids.end(), j) == ids.end()) {
                    std::vector<Card> t = { hand[ids[0]], hand[ids[1]], hand[ids[2]], hand[j] };
                    out.push_back(t);
                }
            }
        }
        if (ids.size() >= 4) {
            // bomb: all same-rank cards
            std::vector<Card> bomb;
            for (int id : ids) bomb.push_back(hand[id]);
            out.push_back(bomb);
        }
    }

    // Note: may contain duplicates (e.g., different triples from different ranks).
    // We can optionally deduplicate by string representation to keep candidates smaller.
    std::vector<std::vector<Card>> dedup;
    std::set<std::string> seen;
    for (auto &cand : out) {
        std::string key;
        for (auto &c : cand) key += c.toString() + "|";
        if (seen.insert(key).second) dedup.push_back(cand);
    }

    // 过滤：仅保留 HandMatcher 认定的合法牌型
    std::vector<std::vector<Card>> valid;
    for (auto &cand : dedup) {
        if (evaluateHandType(cand, levelRank) != HandType::Invalid) {
            valid.push_back(cand);
        }
    }

    return valid;
}

// 根据上家牌选择出牌：空 lastCards -> 随机选一个 possible play
// 非空 -> 找出能压制上家的所有 candidate，返回最小能压制牌（保守策略）
std::vector<Card> AIPlayer::decideToMove(const std::vector<Card>& lastCards, int levelRank) {
    auto possible = generatePossiblePlays(levelRank);
    if (possible.empty()) return {};

    // shuffle possible a bit to avoid deterministic behaviour when choosing random
    std::shuffle(possible.begin(), possible.end(), rng_);

    if (lastCards.empty()) {
        // choose random candidate (but prefer non-bombs if possible)
        // try to select a non-bomb first
        std::vector<int> nonbomb_idxs;
        for (int i = 0; i < (int)possible.size(); ++i) {
            if (evaluateHandType(possible[i], levelRank) != HandType::Bomb) nonbomb_idxs.push_back(i);
        }
        if (!nonbomb_idxs.empty()) {
            std::uniform_int_distribution<int> dist(0, (int)nonbomb_idxs.size()-1);
            return possible[nonbomb_idxs[dist(rng_)]];
        }
        std::uniform_int_distribution<int> dist(0, (int)possible.size()-1);
        return possible[dist(rng_)];
    }

    // find beating candidates
    std::vector<std::vector<Card>> beating;
    for (auto &cand : possible) {
        if (canBeat(cand, lastCards, levelRank)) beating.push_back(cand);
    }
    if (beating.empty()) return {}; // pass

    // choose the "smallest" beating play:
    auto rankForType = [](HandType t)->int {
        switch (t) {
        case HandType::Single: return 1;
        case HandType::Pair:   return 2;
        case HandType::Trips: return 3;
        case HandType::Bomb:   return 4;
        case HandType::StraightFlush: return 5;
        case HandType::TianWang: return 6;
        default: return 0;
        }
    };

    std::sort(beating.begin(), beating.end(), [&](const std::vector<Card>& a, const std::vector<Card>& b){
        HandType ta = evaluateHandType(a, levelRank);
        HandType tb = evaluateHandType(b, levelRank);
        int rta = rankForType(ta);
        int rtb = rankForType(tb);
        if (rta != rtb) return rta < rtb; // prefer weaker hand type
        int ra = primaryRank(a, levelRank);
        int rb = primaryRank(b, levelRank);
        if (ra != rb) return ra < rb;     // prefer smaller primary rank
        if (a.size() != b.size()) return a.size() < b.size();
        // fallback lexicographic by card string
        std::string sa, sb;
        for (auto &c : a) sa += c.toString() + "|";
        for (auto &c : b) sb += c.toString() + "|";
        return sa < sb;
    });

    return beating.front();
}

// aiPlay slot: simulate think delay and then emit moveReady(selected) or passed()
void AIPlayer::aiPlay(const std::vector<Card>& lastPlay, int levelRank) {
    // schedule the decision after thinkDelayMs_ to simulate thinking
    QTimer::singleShot(thinkDelayMs_, this, [this, lastPlay, levelRank]() {
        try {
            std::vector<Card> chosen = decideToMove(lastPlay, levelRank);
            if (chosen.empty()) {
                emit passed();
            } else {
                emit moveReady(chosen);
            }
        } catch (const std::exception &e) {
            qWarning() << "AI decide exception:" << e.what();
            emit passed();
        }
    });
}
