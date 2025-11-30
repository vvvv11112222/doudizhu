#ifndef JUDGE_H
#define JUDGE_H

#include <QObject>
#include <QString>
#include <vector>
#include <random>
#include "card.h"
#include "player.h"
class Judge : public QObject {
    Q_OBJECT
public:
    explicit Judge(QObject *parent = nullptr);

    QVector<Player*> players;

    void setPlayers(const std::vector<Player*>& newPlayers);
    void beginFirstTurn();

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
private:
    int currentTurn;
    std::vector<int> finishOrder;
    int lastPlayer;
    std::vector<Card> lastCards;
    bool lastWasPass;
     std::vector<bool> playerPassedRound;
    std::mt19937 rng;
     void startNewRound(int leaderId);
    void aiPlay();
    void checkVictory(int playerId);
};

#endif // JUDGE_H
