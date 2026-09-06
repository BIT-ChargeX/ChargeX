#pragma once
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QPixmap;
class PointsWidget;

// 账户模块-需求6：用户个人信息维护（头像/昵称/余额/碳积分/环保足迹）
// 负责人：肇子杰   命令：USER_UPDATE_PROFILE / USER_GET_BALANCE / USER_CARBON_STATS
class ProfileWidget : public QWidget {
    Q_OBJECT
public:
    explicit ProfileWidget(QWidget* parent = nullptr);

    // 切到本页时调用，刷新余额等实时数据
    void refresh();
    void setAvatarPixmap(const QPixmap& pm);

signals:
    void requestRecharge();   // 交给主页弹出充值页
    void logoutRequested();   // 退出登录

private slots:
    void onChooseAvatar();
    void onSaveProfile();
    void onLogoutClicked();
    void onBalanceChanged(double balance);
    void onLoginChanged();
    void onLoggedOut();
    void onPointsClicked();

private:
    void applySession();
    void resetEcoFootprint();

    QLabel* m_avatarLabel;
    QLabel* m_phoneLabel;
    QLineEdit* m_nicknameEdit;
    QPushButton* m_changeAvatarBtn;
    QPushButton* m_saveBtn;
    QPushButton* m_rechargeBtn;
    QPushButton* m_logoutBtn;
    QLabel* m_balanceLabel;
    QLabel* m_hintLabel;

    QLabel* m_energyLabel;
    QLabel* m_carbonLabel;
    QLabel* m_treesLabel;
    QLabel* m_levelLabel;
    QPushButton* m_pointsBtn;

    PointsWidget* m_pointsWidget;

    QString m_pendingAvatarPath;
};
