// GameManager.h (只展示相关片段)
#ifndef GAMEMANAGER_H
#define GAMEMANAGER_H

#include <QObject>
#include <QVector>
#include "deck.h"
#include "player.h"
#include <deque>
#include "AIPlayer.h"
#include "humanPlayer.h"
#include "judge.h"
class GameManager : public QObject {
    Q_OBJECT

public:
    explicit GameManager(QObject* parent = nullptr);
    void setPlayers();
    void initTurnQueue();
    HumanPlayer* getHumanPlayer();
    Judge* getJudge();
    AIPlayer* getAIPlayer(int ID);
public slots:
    void startGame();
    void processNextTurn();

signals:
    void gameStarted();
    void playerDealt(int playerId);
    void gameFinished();
    void turnQueueUpdated();

private:
    Deck deck_;
    std::vector<Player*> players_;
    HumanPlayer *humanPlayer;
    AIPlayer * aiPlayer1;
    AIPlayer * aiPlayer2;
    AIPlayer * aiPlayer3;

    Judge* judge_;
    std::deque<int> turnQueue_;
};

#endif
