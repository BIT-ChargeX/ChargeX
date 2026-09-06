#pragma once
#include <QWidget>
#include <QVector>

class QPushButton;
class QButtonGroup;

// Material 3 风格左侧导航（NavigationRail）：竖向图标+文字条目，
// 选中项为 secondaryContainer 胶囊。仅用于页面切换，不含业务逻辑。
class NavRail : public QWidget {
    Q_OBJECT
public:
    explicit NavRail(QWidget* parent = nullptr);

    int count() const;
    void addItem(const QString& label, int iconKind);
    void setCurrentIndex(int index);
    int currentIndex() const;

signals:
    void selectionChanged(int index);

private:
    void connectButton(QPushButton* btn, int index);

    QVector<QPushButton*> m_items;
    QButtonGroup* m_group;
    int m_current = 0;
};
