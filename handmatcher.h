#ifndef HANDMATCHER_H
#define HANDMATCHER_H

#include <vector>
#include <map>
#include <algorithm>
#include "card.h" // 确保你的项目中包含了 card.h

// 牌型定义
enum class HandType {
    Invalid = 0,
    Single,         // 单张
    Pair,           // 对子
    Trips,          // 三张
    TripsWithPair,  // 三带二 (Full House)
    TriplePairs,    // 三连对 (334455)
    SteelPlate,     // 钢板 (333444)
    Bomb,           // 炸弹 (4张及以上)
    StraightFlush,  // 同花顺 (5张同花且连续)
    TianWang        // 天王炸 (4张王)
};

// 分析结果结构体
struct PlayInfo {
    HandType type = HandType::Invalid;
    int primaryRank = 0;      // 牌型主值（用于比大小）
    int size = 0;             // 总张数
    bool isStraightFlush = false; // 是否同花顺标记
};

class HandMatcher {
public:
    // 构造函数：传入待分析的牌和当前级牌点数
    HandMatcher(const std::vector<Card>& cards, int levelRank);

    // 主分析函数：返回识别出的最佳牌型
    PlayInfo analyze();

private:
    std::vector<Card> allCards; // 保存所有牌，便于做花色等整体校验
    std::vector<Card> solids;   // 固定牌（除去万能牌后的牌）
    int wildCount;            // 万能牌（红桃级牌）数量
    int levelRank;            // 当前级牌点数（如打2，则levelRank=15）
    int totalCount;           // 牌总数

    // --- 内部匹配函数 ---
    PlayInfo matchTianWang();       // 天王炸
    PlayInfo matchBomb();           // 炸弹
    PlayInfo matchStraightFlush();  // 同花顺
    PlayInfo matchSteelPlate();     // 钢板 (333444)
    PlayInfo matchTriplePairs();    // 三连对 (334455)
    PlayInfo matchTripsWithPair();  // 三带二
    PlayInfo matchBasics();         // 单、对、三

    // --- 辅助函数 ---
    // 获取逻辑值用于比大小 (2 < 3 ... < A < Level < S < B)
    int getLogValue(const Card& c) const;
    // 获取序列值用于顺子 (2,3,4...14)
    int getSeqValue(const Card& c) const;
    // 判断是否是万能牌
    bool isWild(const Card& c) const;
    // 获取固定牌的逻辑值计数
    std::map<int, int> getLogCounts() const;
    // 检查顺子逻辑（返回顺子顶张，否则返回0）
    int checkStraightRank() const;
    // 通用连续元组匹配 (用于钢板、三连对)
    PlayInfo matchConsecutiveTuples(int tupleCount, int tupleSize, HandType type);
};

#endif // HANDMATCHER_H
