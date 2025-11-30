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
void GameManager::initTurnQueue() {
    turnQueue_.clear();

    for (int i = 0; i < players_.size(); ++i) {
        turnQueue_.push_back(i);
    }

    emit turnQueueUpdated();
}
void GameManager::startGame() {
    for (auto& player : players_) {
        player->clearHand(); // 假设你有一个 clearHand() 方法
    }

    // 1. 构建并洗牌（快速操作）
    deck_.buildDeck();
    deck_.shuffleDeck();

    // 2. 发牌（示例：轮发）
    auto hands = deck_.dealRoundRobin(static_cast<int>(players_.size()));
    // 3. 分发给 players，并通知 UI 每个玩家手牌变化
    for (int i = 0; i < players_.size(); ++i) {
        std::vector<Card> stdHand = hands[i];
        players_[i]->setHand(stdHand);
        emit playerDealt(i);
    }

    emit gameStarted();

    initTurnQueue();
    if (judge_) {
        judge_->beginFirstTurn();
    }
}
void GameManager::processNextTurn() {
    if (turnQueue_.empty()) return;

    int currentId = turnQueue_.front();

    turnQueue_.pop_front();
    turnQueue_.push_back(currentId);

    judge_->setCurrentTurn(currentId);

    Player* currentPlayer = players_[currentId];
    if (auto ai = dynamic_cast<AIPlayer*>(currentPlayer)) {
        // --- AI 出牌 ---
        std::vector<Card> aiChoice = ai->decideToMove(judge_->getLastCards());
        judge_->playAICards(currentId, aiChoice);

    } else if (auto human = dynamic_cast<HumanPlayer*>(currentPlayer)) {
        // --- 人类玩家出牌 ---
        // 通知 UI：该玩家可以出牌
        emit judge_->playerTurnStart(currentId);
    }
}

