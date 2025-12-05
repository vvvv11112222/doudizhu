// AIPlayer.cpp
#include "aiplayer.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <functional>
#include <numeric> // std::iota 需要这个
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

// 主值：返回用于比较同类牌大小的关键点
int AIPlayer::primaryRank(const std::vector<Card>& cards, int levelRank) const {
    HandMatcher matcher(cards, levelRank);
    return matcher.analyze().primaryRank;
}

// 优化 canBeat 逻辑
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

// 生成可能出牌 - 最终修复版 (支持所有掼蛋牌型)
std::vector<std::vector<Card>> AIPlayer::generatePossiblePlays(int levelRank) const {
    std::vector<std::vector<Card>> valid;
    std::vector<Card> hand = getHandCopy();
    if (hand.empty()) return valid;

    // 判定红桃级牌（万能牌）
    auto isWild = [levelRank](const Card& c) {
        return c.getSuit() == Suit::Hearts && c.getRankInt() == levelRank;
    };

    // 预处理
    std::map<int, std::vector<Card>> rankGroups;
    std::map<int, std::vector<Card>> logGroups;
    std::map<Suit, std::map<int, std::vector<Card>>> suitGroups;
    std::vector<Card> wildCards;

    auto logValue = [levelRank](const Card& c) {
        int r = c.getRankInt();
        if (c.getRank() == Rank::B) return 20;
        if (c.getRank() == Rank::S) return 19;
        if (r == levelRank) return 18;
        if (r == 15) return 2; // 还原 2
        return r;
    };

    // 排序
    std::sort(hand.begin(), hand.end(), [](const Card& a, const Card& b){
        if (a.getRankInt() != b.getRankInt()) return a.getRankInt() < b.getRankInt();
        return static_cast<int>(a.getSuit()) < static_cast<int>(b.getSuit());
    });

    for (const auto& c : hand) {
        if (isWild(c)) {
            wildCards.push_back(c);
            continue;
        }
        int r = c.getRankInt();
        rankGroups[r].push_back(c);
        logGroups[logValue(c)].push_back(c);
        if (c.getSuit() != Suit::None) {
            suitGroups[c.getSuit()][c.getRankInt()].push_back(c);
        }
    }

    std::set<std::string> seen;
    auto keyOf = [](const std::vector<Card>& cards) {
        std::vector<Card> sorted = cards;
        std::sort(sorted.begin(), sorted.end(), [](const Card& a, const Card& b){
            if (a.getRankInt() != b.getRankInt()) return a.getRankInt() < b.getRankInt();
            return static_cast<int>(a.getSuit()) < static_cast<int>(b.getSuit());
        });
        std::string key;
        for (const auto& c : sorted) key += c.toString() + "|";
        return key;
    };

    auto addIfValid = [&](const std::vector<Card>& cards) {
        if (cards.empty()) return;
        if (evaluateHandType(cards, levelRank) == HandType::Invalid) return;
        std::string key = keyOf(cards);
        if (seen.insert(key).second) valid.push_back(cards);
    };

    auto choose = [](const std::vector<Card>& src, int k) {
        std::vector<std::vector<Card>> res;
        if (k == 0) { res.push_back({}); return res; }
        if (k > static_cast<int>(src.size())) return res;
        std::vector<int> idx(k);
        std::iota(idx.begin(), idx.end(), 0);
        while (true) {
            std::vector<Card> pick;
            for (int i : idx) pick.push_back(src[i]);
            res.push_back(pick);
            int pos = k - 1;
            while (pos >= 0 && idx[pos] == static_cast<int>(src.size()) - k + pos) pos--;
            if (pos < 0) break;
            idx[pos]++;
            for (int j = pos + 1; j < k; ++j) idx[j] = idx[j - 1] + 1;
        }
        return res;
    };

    // 1) 基础牌型
    for (const auto& [rank, cards] : rankGroups) {
        int sz = static_cast<int>(cards.size());
        for (int take = 1; take <= std::min(3, sz); ++take) {
            for (const auto& subset : choose(cards, take)) addIfValid(subset);
        }
        for (int need = 1; need <= 3; ++need) {
            int maxWild = std::min(need, static_cast<int>(wildCards.size()));
            for (int wildUse = 1; wildUse <= maxWild; ++wildUse) {
                int solidNeed = need - wildUse;
                if (solidNeed < 0 || solidNeed > sz) continue;
                for (const auto& subset : choose(cards, solidNeed)) {
                    std::vector<Card> combo = subset;
                    combo.insert(combo.end(), wildCards.begin(), wildCards.begin() + wildUse);
                    addIfValid(combo);
                }
            }
        }
    }
    for (int need = 1; need <= std::min(3, static_cast<int>(wildCards.size())); ++need) {
        addIfValid(std::vector<Card>(wildCards.begin(), wildCards.begin() + need));
    }

    // 2) 炸弹
    int wildTotal = static_cast<int>(wildCards.size());
    for (const auto& [rank, cards] : rankGroups) {
        int sz = static_cast<int>(cards.size());
        for (int total = 4; total <= sz + wildTotal; ++total) {
            int minSolids = std::max(1, total - wildTotal);
            int maxSolids = std::min(total, sz);
            for (int solidsTake = minSolids; solidsTake <= maxSolids; ++solidsTake) {
                int wildNeed = total - solidsTake;
                for (const auto& subset : choose(cards, solidsTake)) {
                    std::vector<Card> combo = subset;
                    if (wildNeed > 0) combo.insert(combo.end(), wildCards.begin(), wildCards.begin() + wildNeed);
                    addIfValid(combo);
                }
            }
        }
    }

    // 3) 天王炸
    auto jokerCount = [&](Rank r) {
        auto it = rankGroups.find(static_cast<int>(r));
        return (it == rankGroups.end()) ? 0 : static_cast<int>(it->second.size());
    };
    if (jokerCount(Rank::S) >= 2 && jokerCount(Rank::B) >= 2) {
        std::vector<Card> combo;
        combo.insert(combo.end(), rankGroups.at(static_cast<int>(Rank::S)).begin(), rankGroups.at(static_cast<int>(Rank::S)).begin() + 2);
        combo.insert(combo.end(), rankGroups.at(static_cast<int>(Rank::B)).begin(), rankGroups.at(static_cast<int>(Rank::B)).begin() + 2);
        addIfValid(combo);
    }

    // 4) 三带二
    for (const auto& [tripRank, tripCards] : rankGroups) {
        for (const auto& [pairRank, pairCards] : rankGroups) {
            for (int wildTrip = 0; wildTrip <= wildTotal; ++wildTrip) {
                for (int wildPair = 0; wildPair + wildTrip <= wildTotal; ++wildPair) {
                    int needTrip = 3 - wildTrip, needPair = 2 - wildPair;
                    if (needTrip < 0 || needPair < 0) continue;
                    if (needTrip > (int)tripCards.size() || needPair > (int)pairCards.size()) continue;
                    if (needTrip + needPair + wildTrip + wildPair != 5) continue;
                    for (const auto& tSubset : choose(tripCards, needTrip)) {
                        for (const auto& pSubset : choose(pairCards, needPair)) {
                            std::vector<Card> combo = tSubset;
                            combo.insert(combo.end(), pSubset.begin(), pSubset.end());
                            combo.insert(combo.end(), wildCards.begin(), wildCards.begin() + wildTrip + wildPair);
                            addIfValid(combo);
                        }
                    }
                }
            }
        }
    }

    // 5) 三连对
    std::vector<int> logKeys;
    for (const auto& kv : logGroups) logKeys.push_back(kv.first);
    std::sort(logKeys.begin(), logKeys.end());
    for (size_t i = 0; i + 2 < logKeys.size(); ++i) {
        int a = logKeys[i], b = logKeys[i + 1], c = logKeys[i + 2];
        if (b != a + 1 || c != b + 1) continue;
        const auto& g1 = logGroups[a];
        const auto& g2 = logGroups[b];
        const auto& g3 = logGroups[c];

        auto tryCombo = [&](int w1, int w2, int w3, int usedWild) {
            if (usedWild > wildTotal) return;
            if (g1.size() < (2-w1) || g2.size() < (2-w2) || g3.size() < (2-w3)) return;
            for (const auto& p1 : choose(g1, 2-w1))
                for (const auto& p2 : choose(g2, 2-w2))
                    for (const auto& p3 : choose(g3, 2-w3)) {
                        std::vector<Card> c = p1;
                        c.insert(c.end(), p2.begin(), p2.end());
                        c.insert(c.end(), p3.begin(), p3.end());
                        if (usedWild > 0) c.insert(c.end(), wildCards.begin(), wildCards.begin() + usedWild);
                        addIfValid(c);
                    }
        };
        tryCombo(0,0,0,0); // 0 wild
        tryCombo(1,0,0,1); tryCombo(0,1,0,1); tryCombo(0,0,1,1); // 1 wild
    }

    // 6) 钢板
    for (size_t i = 0; i + 1 < logKeys.size(); ++i) {
        int a = logKeys[i], b = logKeys[i + 1];
        if (b != a + 1) continue;
        const auto& g1 = logGroups[a];
        const auto& g2 = logGroups[b];

        auto tryCombo = [&](int w1, int w2, int usedWild) {
            if (usedWild > wildTotal) return;
            if (g1.size() < (3-w1) || g2.size() < (3-w2)) return;
            for (const auto& p1 : choose(g1, 3-w1))
                for (const auto& p2 : choose(g2, 3-w2)) {
                    std::vector<Card> c = p1;
                    c.insert(c.end(), p2.begin(), p2.end());
                    if (usedWild > 0) c.insert(c.end(), wildCards.begin(), wildCards.begin() + usedWild);
                    addIfValid(c);
                }
        };
        tryCombo(0,0,0);
        tryCombo(1,0,1); tryCombo(0,1,1);
    }

    // 7) 同花顺
    for (const auto& [suit, rankMap] : suitGroups) {
        auto findRankCard = [&](int r) -> const Card* {
            auto it = rankMap.find(r);
            if (it != rankMap.end() && !it->second.empty()) return &it->second.front();
            if (r == 2) { // Fix Rank 2 issue
                auto it15 = rankMap.find(15);
                if (it15 != rankMap.end() && !it15->second.empty()) return &it15->second.front();
            }
            return nullptr;
        };
        auto checkSeq = [&](const std::vector<int>& targetRanks) {
            std::vector<Card> seq;
            int missing = 0;
            for (int r : targetRanks) {
                if (const Card* card = findRankCard(r)) seq.push_back(*card);
                else missing++;
            }
            if (missing <= wildTotal) {
                seq.insert(seq.end(), wildCards.begin(), wildCards.begin() + missing);
                addIfValid(seq);
            }
        };

        for (int start = 2; start <= 10; ++start) {
            std::vector<int> tr; for(int i=0; i<5; ++i) tr.push_back(start+i);
            checkSeq(tr);
        }
        checkSeq({14, 2, 3, 4, 5}); // A2345
    }

    return valid;
}

std::vector<Card> AIPlayer::decideToMove(const std::vector<Card>& lastCards, int levelRank) {
    auto possible = generatePossiblePlays(levelRank);
    if (possible.empty()) return {};

    std::shuffle(possible.begin(), possible.end(), rng_);

    if (lastCards.empty()) {
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

    std::vector<std::vector<Card>> beating;
    for (auto &cand : possible) {
        if (canBeat(cand, lastCards, levelRank)) beating.push_back(cand);
    }
    if (beating.empty()) return {};

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
        if (rta != rtb) return rta < rtb;
        int ra = primaryRank(a, levelRank);
        int rb = primaryRank(b, levelRank);
        if (ra != rb) return ra < rb;
        if (a.size() != b.size()) return a.size() < b.size();
        std::string sa, sb;
        for (auto &c : a) sa += c.toString() + "|";
        for (auto &c : b) sb += c.toString() + "|";
        return sa < sb;
    });

    return beating.front();
}

void AIPlayer::aiPlay(const std::vector<Card>& lastPlay, int levelRank) {
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
