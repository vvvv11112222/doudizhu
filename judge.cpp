#include "Judge.h"
#include <QTime>
#include <QTimer>
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include "card.h"
#include "player.h"

// 级牌、红桃级牌相关辅助：掼蛋顺序 2 < 3 < ... < A < 级牌 < 小王 < 大王
static int logicalRank(const Card& c) {
    switch (c.getRank()) {
    case Rank::Two:   return 2;
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
    case Rank::S:     return 15; // 小王
    case Rank::B:     return 16; // 大王
    default: return 0;
    }
}

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
int Judge::getTeamLevel(int teamId) const {
    if (teamId < 0 || teamId >= static_cast<int>(teamLevels.size())) return 0;
    return teamLevels[teamId];
}

int Judge::getCurrentLevelTeam() const {
    if (!previousPlacements.empty()) {
        return previousPlacements.front() % 2;
    }
    // 初局或尚未产生上局排名时，以当前更高等级的一队作为级牌队伍
    return (teamLevels[1] > teamLevels[0]) ? 1 : 0;
}

int Judge::getCurrentLevelRank() const {
    int team = getCurrentLevelTeam();
    if (team < 0 || team >= static_cast<int>(teamLevels.size())) return 0;
    return teamLevels[team];
}
void Judge::setPlayers(const std::vector<Player*>& newPlayers) {
    players.clear();
    players.assign(newPlayers.begin(), newPlayers.end());
    playerLastPlays.assign(players.size(), {});
    playerPassedRound.assign(players.size(), false);
}
std::vector<int> Judge::getPreviousPlacements() const {
    return previousPlacements;
}
void Judge::resetForNewHand() {
    finishOrder.clear();
    lastCards.clear();
    lastPlayer = -1;
    lastWasPass = false;
    if (!players.empty()) {
        playerLastPlays.assign(players.size(), {});
        playerPassedRound.assign(players.size(), false);
    }
    // 默认逆时针，玩家顺序：0 -> 1 -> 2 -> 3
    direction = 1;
    currentTurn = 0;
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

static bool canBeat(const std::vector<Card>& current, const std::vector<Card>& last, int levelRank) {
    if (last.empty()) {
        HandMatcher matcher(current, levelRank);
        return matcher.analyze().type != HandType::Invalid;
    }

    // 2. 调用 HandMatcher 获取 PlayInfo
    HandMatcher curMatcher(current, levelRank);
    PlayInfo currentInfo = curMatcher.analyze();

    HandMatcher lastMatcher(last, levelRank);
    PlayInfo lastInfo = lastMatcher.analyze();

    if (currentInfo.type == HandType::Invalid || lastInfo.type == HandType::Invalid) {
        return false;
    }

    // 天王炸最高
    if (lastInfo.type == HandType::TianWang) return false;
    if (currentInfo.type == HandType::TianWang) return true;

    // 炸弹体系处理
    auto bombPriority = [](const PlayInfo& info) {
        if (info.type == HandType::TianWang) return 1000;
        if (info.type == HandType::Bomb && info.size >= 6) return info.size*10;
        if (info.type == HandType::StraightFlush) return 55;
        if (info.type == HandType::Bomb && info.size == 5) return 50;
        if (info.type == HandType::Bomb) return 40;
        return 0;
    };

    int lastBombScore = bombPriority(lastInfo);
    int curBombScore  = bombPriority(currentInfo);

    if (lastBombScore > 0 || curBombScore > 0) {
        if (curBombScore == 0) return false; // 非炸弹无法压炸弹
        if (curBombScore != lastBombScore) return curBombScore > lastBombScore;
         return currentInfo.primaryRank > lastInfo.primaryRank;
    }

    if (currentInfo.type != lastInfo.type) return false;

    switch (currentInfo.type) {
    case HandType::Single:
    case HandType::Pair:
    case HandType::Trips:
    case HandType::TripsWithPair:
    case HandType::TriplePairs:
    case HandType::SteelPlate:
        return currentInfo.size == lastInfo.size && currentInfo.primaryRank > lastInfo.primaryRank;
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
    HandMatcher matcher(playCards, getCurrentLevelRank());
    PlayInfo info = matcher.analyze();
    return info.type != HandType::Invalid;
}
int Judge::teammateOf(int playerId) const {
    if (players.empty()) return -1;
    return (playerId + 2) % static_cast<int>(players.size());
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
        valid = canBeat(playCards, lastCards, getCurrentLevelRank());
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
    int remain = players[0]->getCardCount();
    if (remain > 0 && remain <= 10) {
        emit playerReported(0, remain);
    }

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
    if (!lastCards.empty() && !canBeat(aiChosen, lastCards, getCurrentLevelRank())) {
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
    int remain = players[aiId]->getCardCount();
    if (remain > 0 && remain <= 10) {
        emit playerReported(aiId, remain);
    }
    // 4. 检查如果游戏还没结束，继续下一轮
    if (finishOrder.size() < players.size()) {
        checkVictory(aiId);

        nextTurn();
    }
}
void Judge::humanPass() {
    if (finishOrder.size() == 4) return;
    if (currentTurn != 0) return;
    // if (lastCards.empty()) {
    //     qWarning() << "当前由你出牌，不能跳过！";
    //     return; // 这里可以加个信号通知 UI 弹窗提示
    // }

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
    int next = advanceTurnIndex(currentTurn);
    if (next < 0) {
        finalizeGame();
        return;
    }
    currentTurn = next;

    if (allOthersPassed()) {
        int leader = lastPlayer;
        if (leader < 0) leader = currentTurn;

        if (leader >= 0 && players[leader]->getCardCount() == 0) {
            if (!finishOrder.empty() && finishOrder.front() == leader) {
                int mate = teammateOf(leader);
                if (mate >= 0 && players[mate]->getCardCount() > 0) {
                    leader = mate;
                    qInfo() << "接风出牌：头游队友" << leader << "承接出牌权";
                }
            }

            // 如果仍然无人可出（或头游队友也没牌），顺时针找到下一个仍有手牌的玩家
            if (leader >= 0 && players[leader]->getCardCount() == 0) {
                int nextWithCards = advanceTurnIndex(leader);
                if (nextWithCards >= 0) {
                    leader = nextWithCards;
                    qInfo() << "当前玩家已出完，顺延到玩家" << leader;
                }
            }
        }
        startNewRound(leader);
        currentTurn = leader;
    }

    emit turnChanged();

    if (currentTurn != 0) {
        QTimer::singleShot(800, this, &Judge::aiPlay);
    } else {
        emit playerTurnStart(0);
    }
}

// AI 玩家出牌逻辑（简单实现：优先压制，无牌可出则pass）
void Judge::aiPlay() {
    if (finishOrder.size()==4) return;

    AIPlayer* ai = dynamic_cast<AIPlayer*>(players[currentTurn]);
    if (!ai) { nextTurn(); return; }

    std::vector<Card> chosen = ai->decideToMove(lastCards, getCurrentLevelRank());
    if (chosen.empty()) {
        // pass
         qInfo() << "AI" << currentTurn << "选择过牌";
        lastWasPass = true;
        playerPassedRound[currentTurn] = true;
         playerLastPlays[currentTurn].clear();
        emit lastPlayUpdated(currentTurn);
    } else {
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
        int remain = players[currentTurn]->getCardCount();
        if (remain > 0 && remain <= 10) {
            emit playerReported(currentTurn, remain);
        }

    }
    nextTurn();
}
bool Judge::hasPlayerPassed(int playerId) const {
    if (playerId < 0 || playerId >= (int)playerPassedRound.size()) return false;
    return playerPassedRound[playerId];
}
bool Judge::allOthersPassed() const {
    if (lastCards.empty() || lastPlayer < 0) return false;
    for (int i = 0; i < static_cast<int>(players.size()); ++i) {
        if (players[i]->getCardCount() == 0) continue;
        if (i == lastPlayer) continue;
        if (!playerPassedRound[i]) return false;
    }
    return true;
}

int Judge::advanceTurnIndex(int startFrom) const {
    if (players.empty()) return -1;
    int idx = startFrom;
    for (size_t step = 0; step < players.size(); ++step) {
        idx = (idx + direction + players.size()) % players.size();
        if (players[idx]->getCardCount() > 0) return idx;
    }
    return -1;
}
void Judge::startNewRound(int leaderId) {
    qInfo() << "=== 新的一轮开始，庄家：" << leaderId << " ===";
    lastCards.clear();
    lastPlayer = -1; // 或者设为 leaderId，视逻辑而定，这里设为 -1 表示桌面无牌
    for (auto& vec : playerLastPlays) vec.clear();
    std::fill(playerPassedRound.begin(), playerPassedRound.end(), false);
    currentTurn = leaderId;
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
        if (std::find(finishOrder.begin(), finishOrder.end(), playerId) == finishOrder.end()) {
            finishOrder.push_back(playerId);
            int place = static_cast<int>(finishOrder.size()); // 1,2,3,4
            qInfo() << "玩家" << playerId << "完成, 排名:" << place;
            emit playerFinished(playerId, place);
        }
    }
    if (!players.empty() && static_cast<int>(finishOrder.size()) == static_cast<int>(players.size()) - 1) {
        finalizeGame();
    }
}

void Judge::finalizeGame() {
    if (players.size() > finishOrder.size()) {
        for (int i = 0; i < static_cast<int>(players.size()); ++i) {
            if (std::find(finishOrder.begin(), finishOrder.end(), i) == finishOrder.end()) {
                finishOrder.push_back(i);
            }
        }
    }
    previousPlacements = finishOrder;
    tributePending = true;

    if (!finishOrder.empty()) {
        int headTeam = finishOrder.front() % 2;
        int delta = 0;
        if (finishOrder.size() >= 2 && finishOrder[1] % 2 == headTeam) {
            delta = 3;
        } else if (finishOrder.size() >= 3 && finishOrder[2] % 2 == headTeam) {
            delta = 2;
        } else if (!finishOrder.empty() && finishOrder.back() % 2 == headTeam) {
            delta = 1;
        }
        teamLevels[headTeam] = teamLevels[headTeam] + delta;
        qInfo() << "队伍" << headTeam << "升级" << delta << "级，当前级别:" << teamLevels[headTeam];
        if (teamLevels[headTeam] >= 14) {
            emit matchFinished(headTeam); // 达到A，整场比赛结束
        } else {
            emit gameFinished(); // 没到A，仅本局结束，准备下一局
        }
    } else {
        emit gameFinished(); // 异常情况保底
    }
}

void Judge::debugDirectWin(int playerId) {
    if (playerId < 0 || playerId >= players.size()) return;

    // 1. 强制清空该玩家手牌
    players[playerId]->clearHand();

    // 2. 通知 UI 更新手牌显示（变为空）
    emit playerHandChanged(playerId);

    // 3. 触发胜利检查（这会将玩家加入 finishOrder 并触发 playerFinished 信号）
    checkVictory(playerId);

    // 4. 如果当前正好轮到该玩家出牌，则强行结束他的回合，转给下家
    // 这样游戏流转才能继续，直到所有 AI 打完，触发进贡逻辑
    if (currentTurn == playerId) {
        // 模拟他打出了“空”，或者直接交出牌权
        lastPlayer = -1; // 既然赢了，牌权重置或交给下家
        lastCards.clear();
        nextTurn();
    }

    qDebug() << "调试：玩家" << playerId << "已强制获胜";
}
void Judge::resetGameLevels() {
    teamLevels = {2, 2}; // 双方重置为打2
}
bool isCardSmaller(const Card& a, const Card& b, int levelRank) {
    auto getVal = [levelRank](const Card& c) {
        if (c.getRank() == Rank::B) return 200; // 大王
        if (c.getRank() == Rank::S) return 190; // 小王
        int r = c.getRankInt();
        if (r == levelRank) {
            // 级牌：红桃 > 其他
            return (c.getSuit() == Suit::Hearts) ? 180 : 170;
        }
        return r * 10; // 普通牌
    };
    int va = getVal(a);
    int vb = getVal(b);
    if (va != vb) return va < vb;
    // 同点数比花色
    return static_cast<int>(a.getSuit()) < static_cast<int>(b.getSuit());
}
bool isDoubleWinScenario(const std::vector<int>& placements, int totalPlayers) {
    if (placements.size() != 4) return false;
    // 假设 teammateOf(0) == 2, teammateOf(1) == 3
    // 如果第一名和第二名是队友（ID差2），则是双赢
    int p1 = placements[0];
    int p2 = placements[1];
    return std::abs(p1 - p2) == 2;
}
void Judge::debugSetLevel(int teamId, int level) {
    if (teamId >= 0 && teamId < static_cast<int>(teamLevels.size())) {
        teamLevels[teamId] = level;
        qDebug() << "调试：已将队伍" << teamId << "等级强制设为" << level;
    }
}
void Judge::debugSimulateGameEnd(const std::vector<int>& manualOrder) {
    if (manualOrder.size() != 4) {
        qWarning() << "调试失败：必须输入4个玩家的顺序";
        return;
    }

    // 1. 强制覆盖完赛顺序
    finishOrder = manualOrder;

    // 2. 清空所有玩家手牌（模拟打完了）
    // 防止 checkVictory 逻辑干扰，也为了视觉上的“结束”
    for (auto* p : players) {
        p->clearHand();
        emit playerHandChanged(p->getID());
    }

    // 3. 强制通知 UI 清空桌面
    emit tableCleared();

    // 4. 调用核心结算逻辑
    // 这会计算双下/双上，更新 teamLevels，并发出 gameFinished 或 matchFinished 信号
    finalizeGame();

    qDebug() << "调试：已强制按顺序结算 -> " << manualOrder;
}
void Judge::startTributePhase() {
    tributeList.clear();
    doubleTributeStaging.clear();
    isResolvingDoubleTribute = false;
    gamePhase = GamePhase::Tribute;

    if (previousPlacements.size() != 4) {
        finishTributePhase();
        return;
    }

    int p1 = previousPlacements[0]; // 头游
    int p2 = previousPlacements[1]; // 二游
    // 注意：previousPlacements是按完成顺序排的，所以back是末游
    int p3 = previousPlacements[2]; // 三游
    int p4 = previousPlacements[3]; // 末游

    bool isDoubleWin = isDoubleWinScenario(previousPlacements, 4);

    if (isDoubleWin) {
        // === 双贡逻辑 ===

        // 1. 检查全队抗贡（双输方的大王总数）
        int totalBigJokers = 0;
        // 统计 P3 和 P4 的大王
        for (const auto& c : players[p3]->getHandCopy()) if (c.getRank() == Rank::B) totalBigJokers++;
        for (const auto& c : players[p4]->getHandCopy()) if (c.getRank() == Rank::B) totalBigJokers++;

        if (totalBigJokers >= 2) {
            qInfo() << "双下队伍（" << p3 << "," << p4 << "）共有2张大王，触发全队抗贡！";
            emit tributeResisted(p3); // 通知 UI，参数传其中一人即可
            emit tributeResisted(p4);
            finishTributePhase(); // 直接结束
            return;
        }

        // 2. 进入比牌阶段（暂不确定谁给谁，先让两人选牌）
        isResolvingDoubleTribute = true;

        // 向 P3 和 P4 发起选牌请求
        // 如果是人类，通过信号通知；如果是AI，设置定时器
        auto requestCard = [&](int pid) {
            if (pid == 0) {
                emit askForTribute(pid, false); // Human
            } else {
                QTimer::singleShot(1000 + pid * 200, this, [this, pid]() {
                    Card c = findLargestCardForTribute(pid);
                    submitTribute(pid, c);
                });
            }
        };

        requestCard(p3);
        requestCard(p4);

        qInfo() << "进入双贡比牌阶段，等待玩家" << p3 << "和" << p4 << "选牌";
    }
    else {
        // === 单贡逻辑 ===
        int loser = p4;

        // 1. 检查单人抗贡
        int bigJokers = 0;
        for (const auto& c : players[loser]->getHandCopy()) if (c.getRank() == Rank::B) bigJokers++;

        if (bigJokers >= 2) {
            qInfo() << "玩家" << loser << "双大王抗贡";
            emit tributeResisted(loser);
            finishTributePhase();
            return;
        }

        // 2. 生成任务：末游 -> 头游
        tributeList.push_back({loser, p1, {}, true, false});
        executeNextTributeStep(); // 开始执行
    }
}
void Judge::executeNextTributeStep() {
    // 遍历任务列表，找到第一个未完成的任务
    // 顺序：先处理所有进贡，再处理所有还贡

    // PHASE 1: 进贡
    if (gamePhase == GamePhase::Tribute) {
        bool allDone = true;
        for (auto& trans : tributeList) {
            if (trans.active && !trans.cardSelected) {
                allDone = false;

                // 请求该玩家进贡
                if (trans.payer == 0) { // 人类
                    emit askForTribute(trans.payer, false); // false = 进贡
                } else {
                    // AI 自动进贡
                    QTimer::singleShot(800, this, [this, trans]() {
                        // AI 选最大的牌
                        Card c = findLargestCardForTribute(trans.payer);
                        submitTribute(trans.payer, c);
                    });
                }
                return; // 等待异步回调
            }
        }

        if (allDone) {
            // 所有进贡选牌完毕，执行移动牌并进入还贡阶段
            for (auto& trans : tributeList) {
                if (trans.active) {
                    players[trans.payer]->playCards({trans.card});
                    players[trans.receiver]->addCards({trans.card});
                    emit tributeResult(trans.payer, trans.receiver, trans.card, false);
                    // 重置标记以便还贡使用
                    trans.cardSelected = false;
                }
            }
            emit playerHandChanged(-1); // 刷新所有人手牌

            // 切换到还贡阶段
            gamePhase = GamePhase::ReturnTribute;
            QTimer::singleShot(1000, this, &Judge::executeNextTributeStep);
        }
    }
    // PHASE 2: 还贡
    else if (gamePhase == GamePhase::ReturnTribute) {
        bool allDone = true;
        for (auto& trans : tributeList) {
            if (trans.active && !trans.cardSelected) {
                allDone = false;

                // 注意：还贡方向相反，Receiver (赢家) 选牌还给 Payer (输家)
                int returner = trans.receiver;

                if (returner == 0) { // 人类
                    emit askForTribute(returner, true); // true = 还贡
                } else {
                    // AI 还贡
                    QTimer::singleShot(800, this, [this, trans, returner]() {
                        // AI 策略：选最小的牌（非红桃级牌）
                        // 这里简单实现：选最小的一张牌
                        auto hand = players[returner]->getHandCopy();
                        int level = getCurrentLevelRank();
                        // 排序：小 -> 大
                        std::sort(hand.begin(), hand.end(), [level](const Card& a, const Card& b){
                            return isCardSmaller(a, b, level);
                        });
                        // 选第一张（最小），但尽量不要还级牌
                        Card ret = hand.front();
                        for(const auto& c : hand) {
                            if (c.getRankInt() != level && c.getRank() != Rank::S && c.getRank() != Rank::B) {
                                ret = c;
                                break;
                            }
                        }
                        submitTribute(returner, ret);
                    });
                }
                return;
            }
        }

        if (allDone) {
            // 执行还贡移动
            for (auto& trans : tributeList) {
                if (trans.active) {
                    // 注意：trans.card 现在存的是还贡的牌
                    players[trans.receiver]->playCards({trans.card}); // 赢家出牌
                    players[trans.payer]->addCards({trans.card});     // 输家拿牌
                    emit tributeResult(trans.receiver, trans.payer, trans.card, true);
                }
            }
            emit playerHandChanged(-1);

            // 全部结束
            QTimer::singleShot(1000, this, &Judge::finishTributePhase);
        }
    }
}

bool Judge::submitTribute(int playerId, const Card& card) {
    if (gamePhase != GamePhase::Tribute && gamePhase != GamePhase::ReturnTribute) return false;

    // --- 双贡比牌阶段特殊处理 ---
    if (isResolvingDoubleTribute) {
        // 验证牌是否最大（规则同单贡）
        Card maxCard = findLargestCardForTribute(playerId);
        // 注意：这里需要严格比较，包括花色大小
        // 如果玩家选的不是最大的，拒绝（除非有多个同样大的）
        // 这里为了简化，假设校验逻辑与之前一致...
        if (isCardSmaller(card, maxCard, getCurrentLevelRank())) {
            qWarning() << "进贡违规：双贡也必须进贡最大的牌";
            return false;
        }

        // 暂存
        doubleTributeStaging[playerId] = card;
        qInfo() << "玩家" << playerId << "双贡选牌：" << QString::fromStdString(card.toString());

        // 检查是否两人都选好了
        int p3 = previousPlacements[2];
        int p4 = previousPlacements[3];

        if (doubleTributeStaging.count(p3) && doubleTributeStaging.count(p4)) {
            resolveDoubleTributeMatch();
        }
        return true;
    }

    if (gamePhase == GamePhase::Playing) return false;

    // 找到属于该玩家的当前任务
    for (auto& trans : tributeList) {
        if (!trans.active || trans.cardSelected) continue;

        int targetPlayer = (gamePhase == GamePhase::Tribute) ? trans.payer : trans.receiver;

        if (targetPlayer == playerId) {
            // 验证规则
            if (gamePhase == GamePhase::Tribute) {
                // 进贡规则：必须是除红桃级牌外最大的牌
                Card maxCard = findLargestCardForTribute(playerId);
                // 允许花色不同（如果有多个同样大的），但点数和等级必须一致
                // 简单校验：Value 必须相等
                if (isCardSmaller(card, maxCard, getCurrentLevelRank())) {
                    qWarning() << "进贡违规：必须进贡最大的牌！应为" << QString::fromStdString(maxCard.toString());
                    return false; // UI 应该提示错误
                }
                // 特殊：红桃级牌不能进贡（findLargestCardForTribute 已经过滤了）
            }
            else {
                // 还贡规则：通常不限制，可以是任意牌
                // 但不能是空
            }

            trans.card = card;
            trans.cardSelected = true;
            qInfo() << "玩家" << playerId << (gamePhase==GamePhase::Tribute?"进贡":"还贡") << QString::fromStdString(card.toString());

            // 继续下一步
            executeNextTributeStep();
            return true;
        }
    }
    return false;
}

void Judge::finishTributePhase() {
    gamePhase = GamePhase::Playing;

    // 进贡结束后，无论是否发生进贡，均由上一局头游先手
    int leader = previousPlacements.empty() ? 0 : previousPlacements[0];

    startNewRound(leader);

    // 触发第一轮
    if (leader != 0) {
        QTimer::singleShot(800, this, &Judge::aiPlay);
    } else {
        emit playerTurnStart(0);
    }
}

Card Judge::findLargestCardForTribute(int playerId) const {
    auto hand = players[playerId]->getHandCopy();
    int level = getCurrentLevelRank();

    // 过滤红桃级牌（红桃级牌不能进贡，除非手上全是红桃级牌——这在实战几乎不可能）
    std::vector<Card> candidates;
    for (const auto& c : hand) {
        if (c.getSuit() == Suit::Hearts && c.getRankInt() == level) continue;
        candidates.push_back(c);
    }

    if (candidates.empty()) return hand.back(); // 极端情况回退

    // 排序：大 -> 小
    std::sort(candidates.begin(), candidates.end(), [level](const Card& a, const Card& b){
        return !isCardSmaller(a, b, level); // 降序
    });

    return candidates[0];
}
// 核心：双贡比大小，分配进贡对象
void Judge::resolveDoubleTributeMatch() {
    isResolvingDoubleTribute = false; // 结束比牌阶段

    int p1 = previousPlacements[0]; // 头游
    int p2 = previousPlacements[1]; // 二游
    int p3 = previousPlacements[2]; // 三游
    int p4 = previousPlacements[3]; // 末游

    Card c3 = doubleTributeStaging[p3];
    Card c4 = doubleTributeStaging[p4];
    int level = getCurrentLevelRank();

    // 比较 c3 和 c4 的大小
    // 注意：isCardSmaller 返回 true 代表 a < b
    bool p3IsSmaller = isCardSmaller(c3, c4, level);
    bool p4IsSmaller = isCardSmaller(c4, c3, level);

    int bigPayer, smallPayer;
    Card bigCard, smallCard;

    if (!p3IsSmaller && p3IsSmaller != p4IsSmaller) {
        // P3 > P4
        bigPayer = p3; bigCard = c3;
        smallPayer = p4; smallCard = c4;
    } else if (!p4IsSmaller && p3IsSmaller != p4IsSmaller) {
        // P4 > P3
        bigPayer = p4; bigCard = c4;
        smallPayer = p3; smallCard = c3;
    } else {
        // 平局 (点数花色逻辑值完全一样，虽然一般花色不同大小也不同，但在某些规则下可能视为同级)
        // 规则：若大小相同，按顺位进贡。
        // 即：末游(P4) 进贡给 头游(P1)，三游(P3) 进贡给 二游(P2)
        // 注意：这里我们视为“原始位次”，不交换
        qInfo() << "进贡牌大小相同，按默认顺位进贡";

        // 直接生成任务
        tributeList.push_back({p4, p1, c4, true, true}); // 末游 -> 头游
        tributeList.push_back({p3, p2, c3, true, true}); // 三游 -> 二游
        goto EXECUTE; // 跳过下方赋值，直接执行
    }

    // 正常情况：大牌 -> P1, 小牌 -> P2
    tributeList.push_back({bigPayer, p1, bigCard, true, true});
    tributeList.push_back({smallPayer, p2, smallCard, true, true});

EXECUTE:
    // 任务已经生成，并且牌已经选好了 (cardSelected = true)
    // 我们可以直接调用 executeNextTributeStep 来执行“移动牌”和“进入还贡”
    qInfo() << "双贡分配完成：大牌进头游，小牌进二游";
    executeNextTributeStep();
}
