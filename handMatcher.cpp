#include "handmatcher.h"
#include <set>
#include <cmath>
#include <algorithm>

// ==========================================
// 构造与初始化
// ==========================================
HandMatcher::HandMatcher(const std::vector<Card>& cards, int lvlRank)
    : allCards(cards)
    , wildCount(0)
    , levelRank(lvlRank)
    , totalCount(static_cast<int>(cards.size()))
{
    solids.reserve(cards.size());

    for (const auto& c : cards) {
        if (isWild(c)) {
            wildCount++;
        } else {
            solids.push_back(c);
        }
    }

    // 预排序固定牌（按逻辑大小），方便后续处理
    std::sort(solids.begin(), solids.end(), [this](const Card& a, const Card& b) {
        return getLogValue(a) < getLogValue(b);
    });
}

// ==========================================
// 公共接口：全部分析
// ==========================================
PlayInfo HandMatcher::analyze() {
    if (totalCount == 0) return {};

    PlayInfo info;

    // 1. 优先级最高：天王炸
    info = matchTianWang();
    if (info.type != HandType::Invalid) return info;

    // 2. 6张及以上炸弹 (通常大于同花顺)
    if (totalCount >= 6) {
        info = matchBomb();
        if (info.type != HandType::Invalid) return info;
    }

    // 3. 同花顺
    info = matchStraightFlush();
    if (info.type != HandType::Invalid) return info;

    // 4. 普通炸弹 (4-5张)
    info = matchBomb();
    if (info.type != HandType::Invalid) return info;

    // 5. 按张数区分普通牌型
    switch (totalCount) {
    case 6:
        info = matchSteelPlate(); // 钢板
        if (info.type != HandType::Invalid) return info;

        info = matchTriplePairs(); // 三连对
        if (info.type != HandType::Invalid) return info;
        break;
    case 5:
        info = matchTripsWithPair(); // 三带二
        if (info.type != HandType::Invalid) return info;
        break;
    case 3:
    case 2:
    case 1:
        info = matchBasics(); // 单/对/三
        if (info.type != HandType::Invalid) return info;
        break;
    }

    return {}; // 无效牌型
}

// ==========================================
// 具体的匹配逻辑
// ==========================================

// 1. 天王炸 (4张王)
PlayInfo HandMatcher::matchTianWang() {
    if (totalCount != 4) return {};
    if (wildCount > 0) return {}; // 必须是真王

    int small = 0, big = 0;
    for (const auto& c : solids) {
        if (c.getRank() == Rank::S) small++;
        else if (c.getRank() == Rank::B) big++;
        else return {}; // 含杂牌
    }

    if (small == 2 && big == 2) {
        return {HandType::TianWang, 20, 4};
    }
    return {};
}

// 2. 炸弹 (4+张)
PlayInfo HandMatcher::matchBomb() {
    // 基础条件：炸弹至少4张
    if (totalCount < 4) return {};

    // 炸弹必须全部同点数，不允许使用红桃级牌来“补牌”
    if (wildCount > 0) return {};

    for (const auto& c : solids) {
        if (c.getRank() == Rank::S || c.getRank() == Rank::B) return {};
    }

    int firstVal = getLogValue(solids[0]);
    for (size_t i = 1; i < solids.size(); ++i) {
        if (getLogValue(solids[i]) != firstVal) {
            return {};
        }
    }
    return {HandType::Bomb, firstVal, totalCount};
}

// 3. 同花顺
PlayInfo HandMatcher::matchStraightFlush() {
    if (totalCount != 5) return {};

    // 1) 花色必须统一且必须能确定花色
    if (allCards.empty()) return {};

    Suit suitRef = Suit::None;
    for (const auto& c : allCards) {
        if (c.getRank() == Rank::S || c.getRank() == Rank::B) return {}; // 大小王不能参与同花顺
        if (!isWild(c)) {
            if (suitRef == Suit::None) suitRef = c.getSuit();
            else if (c.getSuit() != suitRef) return {};
        }
    }

    // 如果全是万能牌，则无法确定花色，视为无效
    if (suitRef == Suit::None) return {};

    // 万能牌视作指定花色，检查其他牌是否冲突
    for (const auto& c : allCards) {
        if (isWild(c)) continue;
        if (c.getSuit() != suitRef) return {};
    }

    int top = checkStraightRank();
    if (top > 0) {
        return {HandType::StraightFlush, top, 5, true};
    }

    return {};
}
PlayInfo HandMatcher::matchSteelPlate() {
    if (totalCount != 6) return {};
    if (wildCount >= 2) return {};
    std::map<int, int> counts = getLogCounts();

    if (counts.size() != 2) return {};
    auto it = counts.begin();
    int lowRank = it->first;
    int highRank = (++it)->first;
    if (highRank != lowRank + 1) return {};

    if (counts[lowRank] > 3 || counts[highRank] > 3) return {};
    return {HandType::SteelPlate, highRank, 6};
}

PlayInfo HandMatcher::matchTriplePairs() {
    if (totalCount != 6) return {};
    if (wildCount >= 2) return {};
    std::map<int, int> counts = getLogCounts();
    if (counts.size() != 3) return {};
    auto it = counts.begin();
    int r1 = it->first;
    int r2 = (++it)->first;
    int r3 = (++it)->first;

    if (r2 != r1 + 1 || r3 != r2 + 1) return {};
    if (counts[r1] > 2 || counts[r2] > 2 || counts[r3] > 2) return {};
    return {HandType::TriplePairs, r3, 6};
}


// 7. 三带二 (Full House)
PlayInfo HandMatcher::matchTripsWithPair() {
    // 基础检查：必须是5张牌
    if (totalCount != 5) return {};

    std::map<int, int> counts = getLogCounts();

    // --- 情况 1: 红心级牌数量为 0 ---
    if (wildCount == 0) {
        // solid 种类数必须为 2 (例如 33344)
        if (counts.size() != 2) return {};

        // 检查分布是否为 3:2，并找出 3张 的那个点数
        int tripVal = -1;
        for (auto kv : counts) {
            if (kv.second == 3) {
                tripVal = kv.first;
                break;
            }
        }

        // 找到了张数为3的主值，则匹配成功
        if (tripVal != -1) {
            return {HandType::TripsWithPair, tripVal, 5};
        }
        return {};
    }

    // --- 情况 2: 红心级牌数量为 1 ---
    if (wildCount == 1) {
        // solid (4张) 种类数必须为 2
        // (例如 3334 + Wild 或 3344 + Wild)
        if (counts.size() != 2) return {};

        auto it = counts.begin();
        int r1 = it->first;
        int r2 = (++it)->first;

        // 核心条件：必须连续
        // 注意：getLogValue 中 A=14, 2=2。这里 14 和 2 不算连续。
        if (std::abs(r1 - r2) != 1) return {};

        // 返回较大牌的主值
        // (根据您的指示：3334+Wild -> 返回 4，即视为 44433)
        return {HandType::TripsWithPair, std::max(r1, r2), 5};
    }

    // --- 情况 3: 红心级牌数量为 2 ---
    if (wildCount == 2) {
        // solid (3张) 种类数必须为 2
        // (例如 344 + 2Wild 或 334 + 2Wild)
        if (counts.size() != 2) return {};

        auto it = counts.begin();
        int r1 = it->first;
        int r2 = (++it)->first;

        // 此时不需要连续，直接返回较大的牌的主值
        return {HandType::TripsWithPair, std::max(r1, r2), 5};
    }

    return {};
}

// 8. 基础牌型
PlayInfo HandMatcher::matchBasics() {
    std::map<int, int> counts = getLogCounts();
    if (counts.size() > 1) return {}; // 只能有一种点数

    int val = counts.empty() ? 14 : counts.begin()->first;

    if (totalCount == 1) return {HandType::Single, val, 1};
    if (totalCount == 2) return {HandType::Pair, val, 2};
    if (totalCount == 3) return {HandType::Trips, val, 3};
    return {};
}

// ==========================================
// 辅助与通用逻辑
// ==========================================

int HandMatcher::getLogValue(const Card& c) const {
    if (c.getRank() == Rank::B) return 20;
    if (c.getRank() == Rank::S) return 19;

    int r = c.getRankInt(); // 假设: 3=3 ... A=14, 2=15
    if (r == levelRank) return 18; // 级牌
    if (r == 15) return 2;         // 2 (若非级牌)
    return r;
}

int HandMatcher::getSeqValue(const Card& c) const {
    int r = c.getRankInt();
    if (r == 15) return 2; // 还原2
    return r;
}

bool HandMatcher::isWild(const Card& c) const {
    return (c.getSuit() == Suit::Hearts && c.getRankInt() == levelRank);
}

std::map<int, int> HandMatcher::getLogCounts() const {
    std::map<int, int> m;
    for (const auto& c : solids) m[getLogValue(c)]++;
    return m;
}

// 通用连续元组逻辑 (用于钢板/连对)
PlayInfo HandMatcher::matchConsecutiveTuples(int tupleCount, int tupleSize, HandType type) {
    std::map<int, int> counts = getLogCounts();

    for (int start = 2; start <= 14 - tupleCount + 1; ++start) {

        bool possible = true;
        int wildUsed = 0;
        int cardsCovered = 0;

        for (int i = 0; i < tupleCount; ++i) {
            int cur = start + i;
            int c = counts[cur];
            if (c > tupleSize) { possible = false; break; } // 单张点数过多

            wildUsed += (tupleSize - c);
            cardsCovered += c;
        }

        if (!possible) continue;
        if (wildCount < wildUsed) continue;
        if (cardsCovered + wildCount != totalCount) continue;
        return {type, start + tupleCount - 1, totalCount};
    }
    return {};
}

// 检查顺子序列 (核心算法修改版)
int HandMatcher::checkStraightRank() const {
    if (solids.empty()) return 0; // 无固定牌无法判定顺子
    std::vector<int> seqs;
    for (const auto& c : solids) {
        int v = getSeqValue(c);
        if (v >= 16) return 0;
        seqs.push_back(v);
    }
    std::sort(seqs.begin(), seqs.end());
    bool hasA = std::find(seqs.begin(), seqs.end(), 14) != seqs.end();

    std::vector<int> uniq = seqs;
    auto last = std::unique(uniq.begin(), uniq.end());
    if (last != uniq.end()) return 0;
    auto calcResult = [&](int maxV, int missing) -> int {
        // 情况 A: 连续 (没有缺牌)
        if (missing == 0) {
            // 返回 solids 最大的主值加上尽可能多的红桃级牌数
            // 但不能超过 A (14)
            int result = maxV + wildCount;
            return (result > 14) ? 14 : result;
        }

        // 情况 B: 中间差了一张牌 (missing == 1)
        if (missing == 1) {
            if (wildCount == 1) return maxV;     // 补齐中间，最大值仍是 maxV
            if (wildCount == 2) {
                // 补齐中间消耗1张，剩1张加在尾部
                int result = maxV + 1;
                return (result > 14) ? 14 : result;
            }
        }

        // 情况 C: 中间差了两张牌 (missing == 2)
        if (missing == 2) {
            if (wildCount == 2) return maxV;     // 补齐中间，最大值仍是 maxV
        }

        return 0; // 其他情况无法构成顺子
    };

    // --- 策略 1: 特殊同花顺 A2345 (A当作1) ---
    if (hasA) {
        std::vector<int> lowSeqs = seqs;
        // 将 A(14) 变为 1，并重新排序
        for(int& x : lowSeqs) if(x == 14) x = 1;
        std::sort(lowSeqs.begin(), lowSeqs.end());

        // 只有当最大值不超过 5 时才可能是 A2345
        if (lowSeqs.back() <= 5) {
            int minV = lowSeqs.front();
            int maxV = lowSeqs.back();
            int count = lowSeqs.size();
            // 计算跨度缺失: (5 - 1 + 1) - 3 = 2
            int missing = (maxV - minV + 1) - count;

            // 调用通用逻辑，如果计算成功，强制返回 5 (A2345的主值)
            if (calcResult(maxV, missing) > 0) return 5;
        }
    }

    // --- 策略 2: 普通同花顺 (A当作14) ---
    int minV = seqs.front();
    int maxV = seqs.back();
    int count = seqs.size();

    // 计算中间缺了几张
    // 例如: 3,4,6. Max=6, Min=3. Count=3. (6-3+1)-3 = 1. 缺1张(5)
    int missing = (maxV - minV + 1) - count;

    int res = calcResult(maxV, missing);

    // A 只能出现在顺子的首尾（A2345 或 10JQKA），否则视为无效
    if (hasA && res != 5 && res != 14) return 0;

    return res;
}
