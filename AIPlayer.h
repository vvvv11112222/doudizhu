#ifndef AIPLAYER_H
#define AIPLAYER_H

#include <QObject>
#include <vector>
#include <random>
#include "player.h"
#include "card.h"
#include "handmatcher.h"

class AIPlayer : public QObject, public Player {
    Q_OBJECT
public:
    explicit AIPlayer(int id, const std::string& name = "AI", QObject* parent = nullptr);
    ~AIPlayer() override = default;
    // 方法1：判断牌型
    HandType evaluateHandType(const std::vector<Card>& cards) const;

    // 方法2：生成当前手牌的所有可能出牌（单张、对子、炸弹）
    std::vector<std::vector<Card>> generatePossiblePlays() const;

    // 方法3：根据上家牌选择出牌（空表示过）
    std::vector<Card> decideToMove(const std::vector<Card>& lastCards);
public slots:

    void aiPlay(const std::vector<Card>& lastPlay);

signals:
    // 与 HumanPlayer 相同的外部接口，交给 GameManager/Judge 处理
    void moveReady(const std::vector<Card>& cards); // 候选出牌
    void passed();                                  // AI 选择过
    void invalidMove(const QString& reason);        // （可选）用于调试或日志

private:
    // 辅助：得到牌型的主值（用于比较，例如对子/炸弹的点数）
    int primaryRank(const std::vector<Card>& cards) const;

    // 辅助：判断 candidate 能否压制 base（上家）
    bool canBeat(const std::vector<Card>& candidate, const std::vector<Card>& base) const;

    // 用于随机选择
    mutable std::mt19937 rng_;
    int thinkDelayMs_ = 300; // 模拟思考延迟（毫秒），可调整或设为 0
};

#endif // AIPLAYER_H
