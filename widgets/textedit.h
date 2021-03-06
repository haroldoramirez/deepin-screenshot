#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QPainter>
#include <QMouseEvent>

class TextEdit : public QPlainTextEdit {
    Q_OBJECT
public:
    TextEdit(int index, QWidget* parent);
    ~TextEdit();

public slots:
    void setColor(QColor c);
     int getIndex();
    void updateCursor();
    void setCursorVisible(bool visible);
    void keepReadOnlyStatus();
    void setFontSize(int fontsize);

signals:
     void repaintTextRect(TextEdit* edit,  QRectF newPositiRect);
     void backToEditing();
     void textEditSelected(int index);

protected:
    void mousePressEvent(QMouseEvent* e);
    void enterEvent(QEnterEvent* e);
    void mouseMoveEvent(QMouseEvent* e);
    void mouseReleaseEvent(QMouseEvent* e);
    bool eventFilter(QObject* watched, QEvent* event);

private:
     int m_index;
     QColor m_textColor;
     QPainter* m_painter;

     QPointF m_pressPoint;
     bool m_isPressed;
};

#endif // TEXTEDIT_H
