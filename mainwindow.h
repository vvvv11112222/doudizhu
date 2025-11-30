#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include "gameManager.h"
#include "judge.h"
#include "humanPlayer.h"
#include "AIPlayer.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private slots:
    void updateUI();
    void onSelectionChanged();
    void onPlayClicked();       // 点击“出牌”按钮
    void onPassClicked();       // 点击“过”按钮
    void onGameFinished();
    void onCardClicked(QListWidgetItem *item);
private:
    void setupUI();
    void setupConnections();
    GameManager *gameManager;
    Judge *judge;
    HumanPlayer *humanPlayer;
    AIPlayer * aiPlayer1;
    AIPlayer * aiPlayer2;
    AIPlayer * aiPlayer3;
    QListWidget *listAI1;
    QListWidget *listAI2;
    QListWidget *listAI3;
    QListWidget *listHuman;
    QListWidget *playTop;     // 中央上：显示 AI1 的出牌
    QListWidget *playLeft;    // 中央左：显示 AI2 的出牌
    QListWidget *playRight;   // 中央右：显示 AI3 的出牌
    QListWidget *playCenterBottom; // 中央下：显示 Human 的出牌（位于人类手牌上方）
    QLabel *lblLastPlay;
    QLabel *lblStatus;
    QPushButton *btnPass;
    QPushButton *btnNewGame;
    //出牌按键
    QPushButton *btnPlay;
    QPushButton *btnPlayerPlay;
};

#endif // MAINWINDOW_H
