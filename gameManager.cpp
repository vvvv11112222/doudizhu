// GameManager.cpp (关键逻辑示例)
#include "gamemanager.h"
#include "QDebug"
GameManager::GameManager(QObject* parent)
    : QObject(parent), judge_(new Judge(this))
{
}

void GameManager::setPlayers() {
    humanPlayer = new HumanPlayer(0, "人类玩家", this);  // 人类玩家
    aiPlayer1 = new AIPlayer(1, "AI 玩家 1", this);  // AI 玩家 1
    aiPlayer2 = new AIPlayer(2, "AI 玩家 2", this);  // AI 玩家 2
    aiPlayer3 = new AIPlayer(3, "AI 玩家 3", this);  // AI 玩家 3

    players_ = {humanPlayer, aiPlayer1, aiPlayer2, aiPlayer3};

    judge_->setPlayers(players_);
}
HumanPlayer* GameManager::getHumanPlayer(){
    return humanPlayer;
};
Judge* GameManager::getJudge(){
    return judge_;
}
AIPlayer* GameManager::getAIPlayer(int ID){
    if(ID==1){
        return aiPlayer1;
    }else if(ID==2){
        return aiPlayer2;
    }else{
        return aiPlayer3;
    }
}

void GameManager::startNextRound() {
    // 1. 清理手牌
    for (auto& player : players_) {
        player->clearHand();
    }

    // 2. 重置裁判状态（但不重置等级）
    judge_->resetForNewHand();

    // 3. 洗牌发牌
    deck_.buildDeck();
    deck_.shuffleDeck();
    auto hands = deck_.dealRoundRobin(static_cast<int>(players_.size()));

    for (int i = 0; i < players_.size(); ++i) {
        players_[i]->setHand(hands[i]);
        emit playerDealt(i);
    }

    emit gameStarted();
    judge_->startTributePhase();
}
void GameManager::startNewGame() {
    if (judge_) {
        judge_->resetGameLevels(); // 重置回打2
    }
    startNextRound();
}


