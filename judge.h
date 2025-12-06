#ifndef JUDGE_H
#define JUDGE_H

#include <QObject>
#include <QString>
#include <vector>
#include <random>
#include <array>
#include "card.h"
#include "player.h"
#include "AIPlayer.h"
enum class GamePhase {
    Playing,        // 正常打牌
    Tribute,        // 进贡阶段
    ReturnTribute   // 还贡阶段
};
struct TributeTrans {
    int payer;      // 进贡者
    int receiver;   // 收贡者
    Card card;      // 进贡的牌
    bool active;    // 是否生效（抗贡则为false）
    bool cardSelected; // 是否已经选好牌
};
class Judge : public QObject {
    Q_OBJECT
public:
    explicit Judge(QObject *parent = nullptr);

    QVector<Player*> players;

    void setPlayers(const std::vector<Player*>& newPlayers);
    void beginFirstTurn();
    void resetForNewHand();
    void applyTributeAndReturn();
    //出牌接口
    bool playHumanCard(const std::vector<Card>& playCards);
    void playAICards(int aiId, const std::vector<Card>& aiChosen);
    void humanPass();
    void nextTurn();

    //状态查询
    int getCurrentTurn() const;
    int getLastPlayer() const;
    std::vector<Card> getLastCards() const;
    int getPlayerHandCount(int playerId) const;
    std::vector<Card> getPlayerLastPlay(int playerId) const;
    bool hasPlayerPassed(int playerId) const;
    int getTeamLevel(int teamId) const;
    int getCurrentLevelTeam() const;
    int getCurrentLevelRank() const;
    std::vector<int> getPreviousPlacements() const;
    std::vector<std::vector<Card>> playerLastPlays;

    //规则判断
    bool isValidPlay(const std::vector<Card>& playCards) const;


    QString lastPlayString() const;
    int currentPlayerIndex() const { return currentTurn; }
    void setCurrentTurn(int turn);
    void resetGameLevels();
    //直接获胜，方便测试
    void debugDirectWin(int playerId);
    void debugSetLevel(int teamId, int level);
    void debugSimulateGameEnd(const std::vector<int>& manualOrder);

    //进贡
    void startTributePhase();
    bool submitTribute(int playerId, const Card& card);
    GamePhase getGamePhase() const { return gamePhase; }
    std::vector<TributeTrans> getPendingTributes() const { return tributeList; }
signals:
    //贡
    void askForTribute(int playerId, bool isReturn);
    void tributeResult(int payer, int receiver, const Card& card, bool isReturn);
    void tributeResisted(int playerId);

    void playerHandChanged(int playerId);
    void turnChanged();
    void lastPlayUpdated(int playerId);
    void tableCleared();
    void playerFinished(int playerId, int place);
    void gameFinished();
    void playerTurnStart(int currentTurn);
    void playerReported(int playerId, int remainCards);
    void matchFinished(int winningTeam);
private:
    int currentTurn;
    // 出牌方向：1 表示逆时针（0 -> 1 -> 2 -> 3），-1 表示顺时针
    int direction{1};
    std::array<int,2> teamLevels{2,2};
    std::vector<int> finishOrder;
    int lastPlayer;
    std::vector<Card> lastCards;
    bool lastWasPass;
     std::vector<bool> playerPassedRound;
    std::vector<int> previousPlacements;
    bool tributePending{false};
    std::mt19937 rng;
     void startNewRound(int leaderId);
    void aiPlay();
    void checkVictory(int playerId);
    void finalizeGame();
    bool allOthersPassed() const;
    int advanceTurnIndex(int startFrom) const;
    int teammateOf(int playerId) const;

    GamePhase gamePhase = GamePhase::Playing;
    std::vector<TributeTrans> tributeList;

    void executeNextTributeStep(); // 执行下一步（处理AI或等待Human）
    void finishTributePhase(); // 结束进贡，开始打牌
    Card findLargestCardForTribute(int playerId) const;
    //双贡
    // 增加一个状态标记
    bool isResolvingDoubleTribute = false;

    // 暂存双贡候选牌：map<playerID, Card>
    std::map<int, Card> doubleTributeStaging;

    // 辅助：处理双贡比大小并生成最终进贡任务
    void resolveDoubleTributeMatch();
};

#endif // JUDGE_H
