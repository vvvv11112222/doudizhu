#include "Judge.h"
#include <QTime>
#include <QTimer>
#include <algorithm>
#include <iostream>
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
static bool isBomb(const std::vector<Card>& cards) {
    int cardCount = cards.size();
    if (cardCount < 4) return false;

    // 四王炸弹（2张小王+2张大王？根据你的牌组定义调整，这里假设是4张王）
    int jokerCount = 0;
    for (const auto& c : cards) {
        if (c.getRankString() == "jokerSmall" || c.getRankString() == "jokerBig") jokerCount++;
    }
    if (jokerCount == 4) return true;

    // 同点数炸弹（4张及以上同rank）
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
static bool canBeat(const std::vector<Card>& current, const std::vector<Card>& last) {
    // 1. 第一次出牌（last 为空）：current 有效则可出
    if (last.empty()) return true;

    // 2. 炸弹优先级最高（除了更大的炸弹）
    bool currentIsBomb = isBomb(current);
    bool lastIsBomb = isBomb(last);

    if (currentIsBomb && !lastIsBomb) return true; // 炸弹压非炸弹
    if (!currentIsBomb && lastIsBomb) return false; // 非炸弹压不住炸弹

    // 3. 都是炸弹：比大小（张数多的大；张数相同比点数；同花顺 < 同点数炸弹）
    if (currentIsBomb && lastIsBomb) {
        int currentSize = current.size();
        int lastSize = last.size();
        if (currentSize != lastSize) return currentSize > lastSize;

        // 张数相同：同点数炸弹 > 同花顺
        bool currentSameRank = (currentSize >=4 && std::all_of(current.begin(), current.end(),
                                                                [&](const Card& c){ return c.getRank() == current[0].getRank(); }));
        bool lastSameRank = (lastSize >=4 && std::all_of(last.begin(), last.end(),
                                                          [&](const Card& c){ return c.getRank() == last[0].getRank(); }));

        if (currentSameRank && !lastSameRank) return true;
        if (!currentSameRank && lastSameRank) return false;

        // 同类型炸弹：比点数（取第一张牌的rank值）
        return current[0].getRankInt() > last[0].getRankInt();
    }

    // 4. 非炸弹：必须牌型相同、张数相同，且点数更大
    if (current.size() != last.size()) return false;

    // 单张/对子/三张：比点数（取第一张牌的rank值，默认同牌型）
    if (current.size() == 1 || current.size() == 2 || current.size() == 3) {
        return current[0].getRankInt() > last[0].getRankInt();
    }

    // 5. 复杂牌型（三带一、三带二、顺子等）：简化处理（可根据需求扩展）
    // 这里仅实现基础逻辑，实际需补充完整牌型匹配
    return false;
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
    // 1. 空牌无效
    if (playCards.empty()) return false;

    int cardCount = playCards.size();

    // 2. 单张（1张）：有效
    if (cardCount == 1) return true;

    // 3. 对子（2张同点数）
    if (cardCount == 2) {
        return playCards[0].getRank() == playCards[1].getRank();
    }

    // 4. 三张（3张同点数）
    if (cardCount == 3) {
        return playCards[0].getRank() == playCards[1].getRank()
        && playCards[1].getRank() == playCards[2].getRank();
    }

    // 5. 三带一（3张同点数 + 1张任意）
    if (cardCount == 4) {
        std::map<std::string, int> rankCount;
        for (const auto& c : playCards) {
            rankCount[c.getRankString()]++;
        }
        // 必须有一个rank出现3次，另一个出现1次
        return (rankCount.size() == 2) &&
               (rankCount.begin()->second == 3 || rankCount.begin()->second == 1);
    }

    // 6. 三带二（3张同点数 + 2张同点数）
    if (cardCount == 5) {
        std::map<std::string, int> rankCount;
        for (const auto& c : playCards) {
            rankCount[c.getRankString()]++;
        }
        // 必须有一个rank出现3次，另一个出现2次
        if (rankCount.size() != 2) return false;
        auto it = rankCount.begin();
        int cnt1 = it->second;
        int cnt2 = (++it)->second;
        return (cnt1 == 3 && cnt2 == 2) || (cnt1 == 2 && cnt2 == 3);
    }

    // 7. 顺子（5张连续点数，不包含2和王）
    if (cardCount == 5) {
        std::vector<Rank> ranks;
        for (const auto& c : playCards) {
            Rank val = c.getRank();
            // 顺子不能包含2、小王、大王
            if (val == Rank::Two || val >= Rank::S) {
                return false;
            }
            ranks.push_back(val);
        }
        std::sort(ranks.begin(), ranks.end());
        // 检查连续
        for (int i = 1; i < 5; i++) {
            if (static_cast<int>(ranks[i]) != static_cast<int>(ranks[i-1]) + 1) {
                return false;
            }
        }
        return true;
    }

    // 8. 炸弹（4张及以上同点数、同花顺、四王）
    if (isBomb(playCards)) return true;

    // 其他牌型（如连对、钢板等）可根据需求扩展
    return false;
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

