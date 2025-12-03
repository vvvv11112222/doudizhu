// HumanPlayer.h
#ifndef HUMANPLAYER_H
#define HUMANPLAYER_H

#include <QObject>
#include <vector>
#include "player.h"
#include "card.h"

// QObject 必须放在最前面
class HumanPlayer : public QObject, public Player {
    Q_OBJECT
public:
    explicit HumanPlayer(int id, const std::string& name = "", QObject* parent = nullptr);

    // 上层请求玩家开始出牌（异步）
    // lastPlay 可为空，或传入上家出的牌（用于界面提示）
    Q_INVOKABLE void requestMove(const std::vector<Card>& lastPlay);

    // UI -> HumanPlayer：用户点击某张牌（通过索引）
    Q_SLOT void toggleSelectCard(int index);    // 切换选牌状态
    Q_SLOT void onUserConfirmPlay();              // 用户点击“出牌”按钮
    Q_SLOT void onUserPass();                     // 用户点击“过”按钮
    Q_SLOT void onUserRequestHint();              // 用户请求提示（可与 Judge 协作）

    // 只读方法，供 MainWindow 获取显示数据
    Q_INVOKABLE std::vector<Card> getHandCopy() const;
    Q_INVOKABLE std::vector<Card> getSelectedCards() const; // 返回当前被选的牌索引
    int getSelectedCount() const { return static_cast<int>(selectedIndices_.size()); }
    bool isIndexSelected(int index) const;
    void resetSelection() { selectedIndices_.clear(); }
signals:
    // HumanPlayer 发给 GameManager 的信号
    void moveReady(const std::vector<Card>& cards); // 玩家确认出牌（候选）
    void passed();                                  // 玩家选择过
    void showHandRequested(int playerId, const std::vector<Card>& hand, const std::vector<Card>& lastPlay);
    void requestHintFromJudge(int playerId);

    // 用于 UI 提示（错误/非法出牌）
    void invalidMove(const QString& reason);

private:
    // 维护一份选中索引列表（UI 交互用）
    std::vector<int> selectedIndices_;

    // 内部帮助函数：把选中索引转换为 Card 向量
    std::vector<Card> selectedCardsFromIndices() const;
};

#endif // HUMANPLAYER_H
