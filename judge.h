#ifndef JUDGE_H
#define JUDGE_H

#include <QObject>
#include <QString>
#include <vector>
#include <random>
#include <array>
#include "card.h"
#include "player.h"
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

signals:
    void playerHandChanged(int playerId);
    void turnChanged();
    void lastPlayUpdated(int playerId);
    void tableCleared();
    void playerFinished(int playerId, int place);
    void gameFinished();
    void playerTurnStart(int currentTurn);
    void playerReported(int playerId, int remainCards);
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
    Card pickTributeCard(Player* donor) const;
    Card pickReturnCard(Player* winner) const;
    int teammateOf(int playerId) const;
};

#endif // JUDGE_H
