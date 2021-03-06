#ifndef MENUCONTROLLER_H
#define MENUCONTROLLER_H

#include <QMenu>

class MenuController : public QObject {
    Q_OBJECT
public:
    MenuController(QObject* parent = 0);
    ~MenuController();

signals:
    void shapePressed(QString currentShape);
    void saveBtnPressed(int index);

public slots:
    void showMenu(QPoint pos);

private:
    QMenu* m_menu;

};
#endif // MENUCONTROLLER_H
