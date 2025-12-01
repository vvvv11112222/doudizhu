#include "Judge.h"
#include <QTime>
#include <QTimer>
#include <algorithm>
#include <iostream>
#include <map>
#include "card.h"
#include "player.h"
#include "AIPlayer.h"
// 辅助：定义牌面值映射（方便比较大小，掼蛋规则：2 < 3 < ... < 10 < J < Q < K < A < 小王 < 大王）
enum class CardRankValue {
    TWO = 2, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN,
    J, Q, K, A, JOKER_SMALL, JOKER_BIG
};

int Judge::getPlayerHandCount(int playerId) const {
    if (playerId < 0 || playerId >= static_cast<int>(players.size())) return 0;
    return static_cast<int>(players[playerId]->getCardCount());
}
int Judge::getLastPlayer() const { return lastPlayer; }
std::vector<Card> Judge::getLastCards() const {
    return lastCards; // 返回副本，调用者不可修改内部状态
}
int Judge::getCurrentTurn() const {
    return currentTurn;
}
void Judge::setPlayers(const std::vector<Player*>& newPlayers) {
    players.clear();
    players.assign(newPlayers.begin(), newPlayers.end());
    playerLastPlays.assign(players.size(), {});
    playerPassedRound.assign(players.size(), false);
}
void Judge::setCurrentTurn(int turn) {
    if (turn < 0 || turn >= players.size()) {
        qWarning() << "无效的轮次：" << turn;
        return;
    }

    currentTurn = turn;
    qDebug() << "Judge::setCurrentTurn -> 当前轮到玩家:" << currentTurn;

    // 通知外部更新UI
    emit turnChanged();
}
void Judge::beginFirstTurn() {
    if (currentTurn < 0) {
        qWarning() << "Judge::beginFirstTurn -> 当前轮次无效";
        return;
    }

    qDebug() << "Judge::beginFirstTurn -> 游戏开始，轮到玩家" << currentTurn;

    // 通知GameManager或UI更新轮次显示
    emit turnChanged();

    // 如果是AI玩家（假设0是人类，1/2/3是AI）
    if (currentTurn != 0) {
        aiPlay();
    } else {
        // 通知UI：人类玩家可以出牌
        emit playerTurnStart(currentTurn);
    }
}
static bool isRocket(const std::vector<Card>& cards) {
    if (cards.size() != 2) return false;

    bool hasSmall = false;
    bool hasBig = false;
    for (const auto& c : cards) {
        if (c.getRank() == Rank::S) hasSmall = true;
        else if (c.getRank() == Rank::B) hasBig = true;
    }
    return hasSmall && hasBig;
}

static bool isBomb(const std::vector<Card>& cards) {
    if (isRocket(cards)) return true;
    int cardCount = cards.size();
    if (cardCount < 4) return false;

    // 四王炸弹（2张小王+2张大王？根据你的牌组定义调整，这里假设是4张王）
    int jokerCount = 0;
    for (const auto& c : cards) {
        if (c.getRankString() == "jokerSmall" || c.getRankString() == "jokerBig") jokerCount++;
    }
    if (jokerCount == 4) return true;

    std::string firstRank = cards[0].getRankString();
    bool sameRank = true;
    for (const auto& c : cards) {
        if (c.getRankString() != firstRank) {
            sameRank = false;
            break;
        }
    }
    if (sameRank) return true;

    // 同花顺（5张同花色+连续rank）
    if (cardCount == 5) {
        // 检查同花色
        std::string suit = cards[0].getSuitString();
        bool sameSuit = true;
        for (const auto& c : cards) {
            if (c.getSuitString() != suit) {
                sameSuit = false;
                break;
            }
        }
        if (!sameSuit) return false;

        // 检查连续rank
        std::vector<int> ranks;
        for (const auto& c : cards) {
            ranks.push_back(c.getRankInt());
        }
        std::sort(ranks.begin(), ranks.end());
        bool consecutive = true;
        for (int i = 1; i < 5; i++) {
            if (ranks[i] != (ranks[i-1] + 1)) {
                consecutive = false;
                break;
            }
        }
        return consecutive;
    }

    return false;
}
enum class PlayType {
    Invalid,
    Single,
    Pair,
    Trips,
    TripsWithSingle,
    TripsWithPair,
    StraightFive,
    Bomb,
    Rocket
};

struct PlayInfo {
    PlayType type{PlayType::Invalid};
    int primaryRank{0};
    int size{0};
};

static PlayInfo analyzePlay(const std::vector<Card>& cards) {
    PlayInfo info;
    info.size = static_cast<int>(cards.size());
    if (cards.empty()) return info;

    // 事先对 rank 做一次统计，便于后续复用
    std::map<int, int> rankCount;
    for (const auto& c : cards) {
        rankCount[c.getRankInt()]++;
    }

    if (isRocket(cards)) {
        info.type = PlayType::Rocket;
        return info;
    }
    if (isBomb(cards)) {
        info.type = PlayType::Bomb;
        info.primaryRank = cards[0].getRankInt();
        return info;
    }

    if (cards.size() == 1) {
        info.type = PlayType::Single;
        info.primaryRank = cards[0].getRankInt();
        return info;
    }

    if (cards.size() == 2 && rankCount.size() == 1) {
        info.type = PlayType::Pair;
        info.primaryRank = rankCount.begin()->first;
        return info;
    }

    if (cards.size() == 3 && rankCount.size() == 1) {
        info.type = PlayType::Trips;
        info.primaryRank = rankCount.begin()->first;
        return info;
    }

    if (cards.size() == 4) {
        for (const auto& kv : rankCount) {
            if (kv.second == 3) {
                info.type = PlayType::TripsWithSingle;
                info.primaryRank = kv.first;
                return info;
            }
        }
    }

    if (cards.size() == 5) {
        for (const auto& kv : rankCount) {
            if (kv.second == 3) {
                // 三带二
                info.type = PlayType::TripsWithPair;
                info.primaryRank = kv.first;
                return info;
            }
        }

        // 顺子（5 张，且不含 2 和大小王）
        bool invalidRank = std::any_of(cards.begin(), cards.end(), [](const Card& c) {
            return c.getRank() == Rank::Two || c.getRank() == Rank::S || c.getRank() == Rank::B;
        });
        if (!invalidRank) {
            std::vector<int> ranks;
            for (const auto& c : cards) ranks.push_back(c.getRankInt());
            std::sort(ranks.begin(), ranks.end());
            bool consecutive = true;
            for (int i = 1; i < 5; ++i) {
                if (ranks[i] != ranks[i - 1] + 1) {
                    consecutive = false;
                    break;
                }
            }
            if (consecutive) {
                info.type = PlayType::StraightFive;
                info.primaryRank = ranks.back();
                return info;
            }
        }
    }

    return info;
}

static bool canBeat(const std::vector<Card>& current, const std::vector<Card>& last) {
    if (last.empty()) return true;

    PlayInfo currentInfo = analyzePlay(current);
    PlayInfo lastInfo = analyzePlay(last);

    if (currentInfo.type == PlayType::Invalid || lastInfo.type == PlayType::Invalid) {
        return false;
    }

    // 王炸优先级最高
    if (lastInfo.type == PlayType::Rocket) return false;
    if (currentInfo.type == PlayType::Rocket) return true;

    // 炸弹处理（非王炸）
    if (lastInfo.type == PlayType::Bomb && currentInfo.type != PlayType::Bomb) return false;
    if (currentInfo.type == PlayType::Bomb && lastInfo.type != PlayType::Bomb) return true;

    if (currentInfo.type != lastInfo.type) return false;

    // 同牌型比较大小
    switch (currentInfo.type) {
    case PlayType::Single:
    case PlayType::Pair:
    case PlayType::Trips:
    case PlayType::TripsWithSingle:
    case PlayType::TripsWithPair:
    case PlayType::StraightFive:
        return currentInfo.size == lastInfo.size && currentInfo.primaryRank > lastInfo.primaryRank;
    case PlayType::Bomb:
        if (currentInfo.size != lastInfo.size) return currentInfo.size > lastInfo.size;
        return currentInfo.primaryRank > lastInfo.primaryRank;
    case PlayType::Rocket:
    case PlayType::Invalid:
    default:
        return false;
    }
}


Judge::Judge(QObject *parent)
    : QObject(parent)
    , currentTurn(0)
    , lastPlayer(-1)
    , lastWasPass(false)
{   playerLastPlays.assign(4, std::vector<Card>());
     playerPassedRound.assign(4, false);
    // 初始化随机数生成器（用于 AI 出牌）
    std::random_device rd;
    if (rd.entropy() > 0) {
        rng.seed(rd());
    } else {
        // 若 random_device 不可用，用时间种子
        rng.seed(QTime::currentTime().msec());
    }

}


bool Judge::isValidPlay(const std::vector<Card>& playCards) const {
    PlayInfo info = analyzePlay(playCards);
    return info.type != PlayType::Invalid;
}
bool Judge::playHumanCard(const std::vector<Card>& playCards) {
    if (finishOrder.size()==4) return false;
    if (currentTurn != 0) {
        emit gameFinished();
        return false;
    }

    std::vector<Card> humanHand = players[0]->getHandCopy();
    for (const auto& card : playCards) {
         auto it = std::find(humanHand.begin(), humanHand.end(), card);
        if (it == humanHand.end()) {
            qWarning() << "出牌错误：玩家不拥有该牌" << QString::fromStdString(card.toString());
            return false;
        }
    }

    bool valid = isValidPlay(playCards);
    if (valid && !lastCards.empty()) {
        valid = canBeat(playCards, lastCards);
    }

    if (!valid) {
        qWarning() << "无效出牌！";
        return false;
    }

    players[0]->playCards(playCards);

    lastPlayer = 0;
    lastCards = playCards;
    lastWasPass = false;
    playerLastPlays[0] = playCards;
     playerPassedRound[0] = false;
    emit playerHandChanged(0);
    emit lastPlayUpdated(0);

    // 5. 检查是否获胜
    checkVictory(0);

    // 6. 切换到下一轮
    nextTurn();
    return true;
}
void Judge::playAICards(int aiId, const std::vector<Card>& aiChosen)
{
    if (finishOrder.size() == 4) return;

    // 1. AI Pass 的处理
    if (aiChosen.empty()) {
        qInfo() << "AI" << aiId << "选择过牌";
        lastWasPass = true;
        playerPassedRound[aiId] = true;
        playerLastPlays[aiId].clear(); // 清空上次出的牌（显示为过）
        emit lastPlayUpdated(aiId);

        nextTurn();
        return;
    }

    // 2. AI 出牌逻辑
    if (!lastCards.empty() && !canBeat(aiChosen, lastCards)) {
        // 容错：如果AI算错了，强制Pass
        playerPassedRound[aiId] = true;
        playerLastPlays[aiId].clear();
        emit lastPlayUpdated(aiId);
        nextTurn();
        return;
    }

    // 3. 执行出牌
    players[aiId]->playCards(aiChosen);

    // 胜利判定

    // 【重要】：有人出牌了，这才是 lastPlayer 易主的时候
    lastPlayer = aiId;
    lastCards = aiChosen;
    lastWasPass = false;
    playerPassedRound[aiId] = false;

    // 更新记录和 UI
    playerLastPlays[aiId] = lastCards;
    emit lastPlayUpdated(aiId);
    emit playerHandChanged(aiId);

    // 4. 检查如果游戏还没结束，继续下一轮
    if (finishOrder.size() < players.size()) {
        checkVictory(aiId);

        nextTurn();
    }
}
void Judge::humanPass() {
    if (finishOrder.size() == 4) return;
    if (currentTurn != 0) return;

    // 如果 lastCards 为空（即你是先手，或者一轮结束刚轮到你），不允许 Pass
    // 规则：拥有出牌权时不能不要。
    if (lastCards.empty()) {
        qWarning() << "当前由你出牌，不能跳过！";
        return; // 这里可以加个信号通知 UI 弹窗提示
    }

    qInfo() << "人类玩家选择过";
    lastWasPass = true;
    playerPassedRound[0] = true;
    playerLastPlays[0].clear();
    emit lastPlayUpdated(0);
    // 【重要修改】：不要更新 lastPlayer！
    // lastPlayer = 0;  <-- 删掉这行

    // 清空人类玩家在桌面上的出牌显示（可选，视UI需求而定）
    // playerLastPlays[0].clear();
    // emit lastPlayUpdated(0);

    // 切换到下一轮
    nextTurn();
}

// 获取最后出牌的字符串描述
QString Judge::lastPlayString() const {
    if (lastCards.empty()) {
        return "暂无出牌";
    }

    QString str;
    for (const auto& card : lastCards) {
        str += QString::fromStdString(card.toString()) + " ";
    }
    return str.trimmed();
}

// judge.cpp

void Judge::nextTurn() {
    if (finishOrder.size() >= players.size() - 1) {
        emit gameFinished();
        return;
    }

    // 1. 寻找下一个还没出完牌的玩家
    int checkCount = 0;
    do {
        currentTurn = (currentTurn + 1) % players.size();
        checkCount++;
        // 如果找了一圈都没找到有牌的人，说明出bug了或者游戏结束了
        if (checkCount > players.size()) {
            emit gameFinished();
            return;
        }
    } while (players[currentTurn]->getCardCount() == 0);
    bool roundOver = (currentTurn == lastPlayer);
    qDebug() << "轮次切换到玩家：" << currentTurn;

    // 2. 关键逻辑：检查是否“一轮游”（即其他人都不要，权回到了 lastPlayer 手里）
    // 特殊情况：如果 lastPlayer 已经出完牌赢了，那么权应该交给他的下家（即当前的 currentTurn）
    if (lastPlayer != -1 && players[lastPlayer]->getCardCount() == 0) {

        qInfo() << "一轮结束，玩家" << currentTurn << "获得新出牌权";
        lastCards.clear(); // 清空上家牌，意味着可以出任意牌
        lastPlayer = -1;   // 重置 lastPlayer（可选，或者设为 currentTurn）
        roundOver = true;
    }
    if (lastPlayer == -1) {
        roundOver = true;
    }
    if (roundOver) {
        startNewRound(currentTurn);
    }
    // 3. 发送轮次改变信号
    emit turnChanged();

    // 4. 如果是 AI，触发思考
    if (currentTurn != 0) { // 假设 0 是人类
        // 延迟一会再出牌，体验更好
        QTimer::singleShot(800, this, &Judge::aiPlay);
    } else {
        // 是人类，通知 UI 启用按钮
        emit playerTurnStart(0);
    }
}

// AI 玩家出牌逻辑（简单实现：优先压制，无牌可出则pass）
void Judge::aiPlay() {
    if (finishOrder.size()==4) return;

    AIPlayer* ai = dynamic_cast<AIPlayer*>(players[currentTurn]);
    if (!ai) { nextTurn(); return; }

    std::vector<Card> chosen = ai->decideToMove(lastCards);
    if (chosen.empty()) {
        // pass
         qInfo() << "AI" << currentTurn << "选择过牌";
        lastWasPass = true;
        playerPassedRound[currentTurn] = true;
         playerLastPlays[currentTurn].clear();
        emit lastPlayUpdated(currentTurn);
    } else {
        // 执行出牌（Judge 负责移除手牌 / 更新状态）
        qInfo() << "AI" << currentTurn << "出牌:" << chosen.size() << "张";
        players[currentTurn]->playCards(chosen);
        checkVictory(currentTurn);
        lastPlayer = currentTurn;
        lastCards = chosen;
        lastWasPass = false;
        playerPassedRound[currentTurn] = false;
        emit playerHandChanged(currentTurn);
        playerLastPlays[currentTurn] = chosen;
        emit lastPlayUpdated(currentTurn);

    }
    nextTurn();
}
bool Judge::hasPlayerPassed(int playerId) const {
    if (playerId < 0 || playerId >= (int)playerPassedRound.size()) return false;
    return playerPassedRound[playerId];
}

void Judge::startNewRound(int leaderId) {
    qInfo() << "=== 新的一轮开始，庄家：" << leaderId << " ===";
    lastCards.clear();
    lastPlayer = -1; // 或者设为 leaderId，视逻辑而定，这里设为 -1 表示桌面无牌

    // 清空所有人的出牌记录和Pass状态
    for (auto& vec : playerLastPlays) vec.clear();
    std::fill(playerPassedRound.begin(), playerPassedRound.end(), false);

    // 通知 UI 清空桌面
    emit tableCleared();
}
std::vector<Card> Judge::getPlayerLastPlay(int playerId) const {
    if (playerId < 0 || playerId >= static_cast<int>(playerLastPlays.size()))
        return {};
    return playerLastPlays[playerId];
}
void Judge::checkVictory(int playerId) {
    if (players[playerId]->getCardCount() == 0) {
        finishOrder.push_back(playerId);
        int place = static_cast<int>(finishOrder.size()); // 1,2,3,4
        qInfo() << "玩家" << playerId << "完成, 排名:" << place;
        emit playerFinished(playerId, place);
    }
    if (!players.empty() && static_cast<int>(finishOrder.size()) >= static_cast<int>(players.size())) {
        qInfo() << "所有玩家已完成，比赛结束";
        emit gameFinished(); // 最终结束信号（不带参数）
    }
}

