#include "mainwindow.h"

#include <QDesktopWidget>
#include <QPainter>
#include <QFileDialog>
#include <QClipboard>
#include <QAction>
#include <QMap>
#include <QStyleFactory>
#include <QShortcut>
#include <QKeySequence>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <DApplication>
DWIDGET_USE_NAMESPACE

namespace {
const int RECORD_MIN_SIZE = 10;
const int SPACING = 5;
const int TOOLBAR_Y_SPACING = 8;
}

MainWindow::MainWindow(QWidget *parent)
    : QLabel(parent)
{
//     startScreenshot();

}

MainWindow::~MainWindow()
{
}

void MainWindow::initUI() {
    this->setFocus();
    setMouseTracking(true);
    m_configSettings =  ConfigSettings::instance();
//    installEventFilter(this);
    m_hotZoneInterface->asyncCall("EnableZoneDetected", false);

    QPoint curPos = this->cursor().pos();
     m_screenNum = qApp->desktop()->screenNumber(curPos);
     QList<QScreen*> screenList = qApp->screens();
     if (m_screenNum != 0 && m_screenNum < screenList.length()) {
        m_backgroundRect = screenList[m_screenNum]->geometry();
     } else {
         m_backgroundRect =  qApp->primaryScreen()->geometry();
     }

     this->move(m_backgroundRect.x(), m_backgroundRect.y());
     this->setFixedSize(m_backgroundRect.size());
     initBackground();

    m_windowManager = new WindowManager();
    m_windowManager->setRootWindowRect(m_backgroundRect);

    m_rootWindowRect.x = 0;
    m_rootWindowRect.y = 0;
    m_rootWindowRect.width = m_backgroundRect.width();
    m_rootWindowRect.height = m_backgroundRect.height();

    if (m_screenNum == 0) {
        QList<xcb_window_t> windows = m_windowManager->getWindows();
        for (int i = 0; i < windows.length(); i++) {
            m_windowRects.append(m_windowManager->adjustRectInScreenArea(
                                     m_windowManager->getWindowRect(windows[i])));
            qDebug() << "uuuuuuuuuuuuuu" << m_windowRects[i].width << m_windowRects.length();
        }
    }

    m_sizeTips = new TopTips(this);
    m_sizeTips->hide();
    m_toolBar = new ToolBar(this);
    m_toolBar->hide();
    m_zoomIndicator = new ZoomIndicator(this);
    m_zoomIndicator->hide();

    m_menuController = new MenuController;

    m_isFirstDrag = false;
    m_isFirstMove = false;
    m_isFirstPressButton = false;
    m_isFirstReleaseButton = false;

    m_recordX = 0;
    m_recordY = 0;
    m_recordWidth = 0;
    m_recordHeight = 0;

    m_resizeBigPix = QPixmap(":/image/icons/resize_handle_big.png");
    m_resizeSmallPix = QPixmap(":/image/icons/resize_handle_small.png");

    m_dragRecordX = -1;
    m_dragRecordY = -1;

    m_needDrawSelectedPoint = false;
    m_mouseStatus = ShotMouseStatus::Shoting;

    m_selectAreaName = "";

    m_isShapesWidgetExist = false;
    connect(m_toolBar, &ToolBar::buttonChecked, this,  [=](QString shape){
        if (m_isShapesWidgetExist && shape != "color") {
            m_shapesWidget->setCurrentShape(shape);
        } else if (shape != "color") {
            initShapeWidget(shape);
            m_isShapesWidgetExist = true;
        }
    });

    connect(m_toolBar, &ToolBar::requestSaveScreenshot, this,
            &MainWindow::saveScreenshot);
    connect(m_menuController, &MenuController::shapePressed, m_toolBar,
            &ToolBar::shapePressed);
    connect(m_menuController, &MenuController::saveBtnPressed, m_toolBar,
            &ToolBar::saveBtnPressed);
//    connect(&m_eventMonitor, SIGNAL(buttonedPress(int, int)), this,
//            SLOT(showPressFeedback(int, int)), Qt::QueuedConnection);
//    connect(&m_eventMonitor, SIGNAL(buttonedDrag(int, int)), this,
//            SLOT(showDragFeedback(int, int)), Qt::QueuedConnection);
//    connect(&m_eventMonitor, SIGNAL(buttonedRelease(int, int)), this,
//            SLOT(showReleaseFeedback(int, int)), Qt::QueuedConnection);
//    connect(&m_eventMonitor, SIGNAL(pressEsc()), this,
//            SLOT(responseEsc()), Qt::QueuedConnection);
//    m_eventMonitor.start();
}

void MainWindow::initDBusInterface() {
    m_controlCenterDBInterface = new DBusControlCenter(this);
    m_notifyDBInterface = new DBusNotify(this);
    m_hotZoneInterface = new DBusZone(this);
    m_interfaceExist = true;
}

void MainWindow::initShortcut() {
    QShortcut* rectSC = new QShortcut(QKeySequence("Alt+1"), this);
    QShortcut* ovalSC = new QShortcut(QKeySequence("Alt+2"), this);
    QShortcut* arrowSC = new QShortcut(QKeySequence("Alt+3"), this);
    QShortcut* lineSC = new QShortcut(QKeySequence("Alt+4"), this);
    QShortcut* textSC = new QShortcut(QKeySequence("Alt+5"), this);
    QShortcut* colorSC = new QShortcut(QKeySequence("Alt+6"), this);

    connect(rectSC, &QShortcut::activated, this, [=]{
        emit m_toolBar->shapePressed("rectangle");
    });
    connect(ovalSC, &QShortcut::activated, this, [=]{
        emit m_toolBar->shapePressed("oval");
    });
    connect(arrowSC, &QShortcut::activated, this, [=]{
        emit m_toolBar->shapePressed("arrow");
    });
    connect(lineSC, &QShortcut::activated, this, [=]{
        emit m_toolBar->shapePressed("line");
    });
    connect(textSC, &QShortcut::activated, this, [=]{
        emit m_toolBar->shapePressed("text");
    });
    connect(colorSC, &QShortcut::activated, this, [=]{
        emit m_toolBar->shapePressed("color");
    });

    QShortcut* viewSC = new QShortcut(QKeySequence("Ctrl+Shift+/"), this);
    viewSC->setAutoRepeat(false);
    connect(viewSC,  SIGNAL(activated()), this, SLOT(onViewShortcut()));

    if (isCommandExist("dman")) {
        QShortcut* helpSC = new QShortcut(QKeySequence("F1"), this);
        helpSC->setAutoRepeat(false);
        connect(helpSC,  SIGNAL(activated()), this, SLOT(onHelp()));
    }
}

void MainWindow::keyPressEvent(QKeyEvent *ev) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(ev);
    if (keyEvent->key() == Qt::Key_Escape ) {
        exitApp();
    } else if (qApp->keyboardModifiers() & Qt::ControlModifier) {
        if (keyEvent->key() == Qt::Key_Z) {
            qDebug() << "SDGF: ctrl+z !!!";
            emit unDo();
        }
    }

    bool needRepaint = false;
    if (m_isShapesWidgetExist) {
        if (keyEvent->key() == Qt::Key_Escape) {
            exitApp();
            return  ;
        }

        if (keyEvent->key() == Qt::Key_Shift) {
            m_isShiftPressed =  true;
            m_shapesWidget->setShiftKeyPressed(m_isShiftPressed);
        }

        if (keyEvent->key() == Qt::Key_S) {
            saveScreenshot();
        }

        if (keyEvent->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {
            if (keyEvent->key() == Qt::Key_Left) {
                m_shapesWidget->microAdjust("Ctrl+Shift+Left");
            } else if (keyEvent->key() == Qt::Key_Right) {
                m_shapesWidget->microAdjust("Ctrl+Shift+Right");
            } else if (keyEvent->key() == Qt::Key_Up) {
                m_shapesWidget->microAdjust("Ctrl+Shift+Up");
            } else if (keyEvent->key() == Qt::Key_Down) {
                m_shapesWidget->microAdjust("Ctrl+Shift+Down");
            }
        } else if (qApp->keyboardModifiers() & Qt::ControlModifier) {
            if (keyEvent->key() == Qt::Key_Left) {
                m_shapesWidget->microAdjust("Ctrl+Left");
            } else if (keyEvent->key() == Qt::Key_Right) {
                m_shapesWidget->microAdjust("Ctrl+Right");
            } else if (keyEvent->key() == Qt::Key_Up) {
                m_shapesWidget->microAdjust("Ctrl+Up");
            } else if (keyEvent->key() == Qt::Key_Down) {
                m_shapesWidget->microAdjust("Ctrl+Down");
            } else if (keyEvent->key() == Qt::Key_C) {
                ConfigSettings::instance()->setValue("save", "save_op", 3);
                saveScreenshot();
            }
        }  else {
            if (keyEvent->key() == Qt::Key_Left) {
                m_shapesWidget->microAdjust("Left");
            } else if (keyEvent->key() == Qt::Key_Right) {
                m_shapesWidget->microAdjust("Right");
            } else if (keyEvent->key() == Qt::Key_Up) {
                m_shapesWidget->microAdjust("Up");
            } else if (keyEvent->key() == Qt::Key_Down) {
                m_shapesWidget->microAdjust("Down");
            }
        }

        if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace) {
            emit  deleteShapes();
        } else {
            qDebug() << "ShapeWidget Exist keyEvent:" << keyEvent->key();
        }
        return  ;
    }

    if (keyEvent->key() == Qt::Key_Shift) {
        m_isShiftPressed = !m_isShiftPressed;
    }

    if (m_mouseStatus == ShotMouseStatus::Normal) {
        if (keyEvent->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {

            if (keyEvent->key() == Qt::Key_Left) {
                m_recordX = std::max(0, m_recordX + 1);
                m_recordWidth = std::max(std::min(m_recordWidth - 1,
                                                  m_rootWindowRect.width), RECORD_MIN_SIZE);

                needRepaint = true;
            } else if (keyEvent->key() == Qt::Key_Right) {
                m_recordWidth = std::max(std::min(m_recordWidth - 1,
                                                  m_rootWindowRect.width), RECORD_MIN_SIZE);

                needRepaint = true;
            } else if (keyEvent->key() == Qt::Key_Up) {
                m_recordY = std::max(0, m_recordY + 1);
                m_recordHeight = std::max(std::min(m_recordHeight - 1,
                                                   m_rootWindowRect.height), RECORD_MIN_SIZE);

                needRepaint = true;
            } else if (keyEvent->key() == Qt::Key_Down) {
                m_recordHeight = std::max(std::min(m_recordHeight - 1,
                                                   m_rootWindowRect.height), RECORD_MIN_SIZE);

                needRepaint = true;
            }
        } else if (qApp->keyboardModifiers() & Qt::ControlModifier) {
            if (keyEvent->key() == Qt::Key_S) {
                saveScreenshot();
            }

            if (keyEvent->key() == Qt::Key_C) {
                ConfigSettings::instance()->setValue("save", "save_op", 3);
                saveScreenshot();
            }

            if (keyEvent->key() == Qt::Key_Left) {
                m_recordX = std::max(0, m_recordX - 1);
                m_recordWidth = std::max(std::min(m_recordWidth + 1,
                                                  m_rootWindowRect.width), RECORD_MIN_SIZE);

                needRepaint = true;
            } else if (keyEvent->key() == Qt::Key_Right) {
                m_recordWidth = std::max(std::min(m_recordWidth + 1,
                                                  m_rootWindowRect.width), RECORD_MIN_SIZE);

                needRepaint = true;
            } else if (keyEvent->key() == Qt::Key_Up) {
                m_recordY = std::max(0, m_recordY - 1);
                m_recordHeight = std::max(std::min(m_recordHeight + 1,
                                                   m_rootWindowRect.height), RECORD_MIN_SIZE);

                needRepaint = true;
            } else if (keyEvent->key() == Qt::Key_Down) {
                m_recordHeight = std::max(std::min(m_recordHeight + 1,
                                                   m_rootWindowRect.height), RECORD_MIN_SIZE);

                needRepaint = true;
            }
        } else {
            if (keyEvent->key() == Qt::Key_Left) {
                m_recordX = std::max(0, m_recordX - 1);

                needRepaint = true;
            } else if (keyEvent->key() == Qt::Key_Right) {
                m_recordX = std::min(m_rootWindowRect.width - m_recordWidth,
                                     m_recordX + 1);

                needRepaint = true;
            } else if (keyEvent->key() == Qt::Key_Up) {
                m_recordY = std::max(0, m_recordY - 1);

                needRepaint = true;
            } else if (keyEvent->key() == Qt::Key_Down) {
                m_recordY = std::min(m_rootWindowRect.height -
                                     m_recordHeight, m_recordY + 1);

                needRepaint = true;
            }
        }

        if ( !m_needSaveScreenshot) {
            m_sizeTips->updateTips(QPoint(m_recordX, m_recordY),
                                   QString("%1X%2").arg(m_recordWidth).arg(m_recordHeight));

            QPoint toolbarPoint;
            toolbarPoint = QPoint(m_recordX + m_recordWidth - m_toolBar->width(),
                                  std::max(m_recordY + m_recordHeight + TOOLBAR_Y_SPACING, 0));

            if (m_toolBar->width() > m_recordX + m_recordWidth) {
                toolbarPoint.setX(m_recordX + 8);
            }
            if (toolbarPoint.y()>= m_backgroundRect.y() + m_backgroundRect.height()
                    - m_toolBar->height() - 28) {
                if (m_recordY > 28*2 + 10) {
                    toolbarPoint.setY(m_recordY - m_toolBar->height() - TOOLBAR_Y_SPACING);
                } else {
                    toolbarPoint.setY(m_recordY + TOOLBAR_Y_SPACING);
                }
            }
            m_toolBar->showAt(toolbarPoint);
        }
    }

    if (needRepaint) {
        update();
    }

    QLabel::keyPressEvent(ev);
}

void MainWindow::keyReleaseEvent(QKeyEvent *ev) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(ev);

    // NOTE: must be use 'isAutoRepeat' to filter KeyRelease event
    //send by Qt.
    bool needRepaint = false;
    if (!keyEvent->isAutoRepeat()) {
        //            if (keyEvent->modifiers() ==  (Qt::ShiftModifier | Qt::ControlModifier)) {
        //                QProcess::startDetached("killall deepin-shortcut-viewer");
        //            }
        //            if (keyEvent->key() == Qt::Key_Question) {
        //                QProcess::startDetached("killall deepin-shortcut-viewer");
        //            }

        if (keyEvent->key() == Qt::Key_Left || keyEvent->key()
                == Qt::Key_Right || keyEvent->key() == Qt::Key_Up ||
                keyEvent->key() == Qt::Key_Down) {
            needRepaint = true;
        }
    }
    if (needRepaint) {
        update();
    }
    QLabel::keyReleaseEvent(ev);
}

void MainWindow::mousePressEvent(QMouseEvent *ev) {
    if (ev->type() == QEvent::MouseButtonPress && !m_isShapesWidgetExist) {
        m_dragStartX = ev->x();
        m_dragStartY = ev->y();

        if (ev->button() == Qt::RightButton) {
            m_moving = false;
            if (!m_isFirstPressButton) {
                exitApp();
            }

            m_menuController->showMenu(ev->pos());
        }

        if (!m_isFirstPressButton) {
            m_isFirstPressButton = true;
        } else if (ev->button() == Qt::LeftButton) {
            m_moving = true;
            m_dragAction = getDirection(ev);

            m_dragRecordX = m_recordX;
            m_dragRecordY = m_recordY;
            m_dragRecordWidth = m_recordWidth;
            m_dragRecordHeight = m_recordHeight;

        }

        m_isPressButton = true;
        m_isReleaseButton = false;
    }
    QLabel::mousePressEvent(ev);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *ev) {
    saveScreenshot();
    QLabel::mouseDoubleClickEvent(ev);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *ev) {
    bool needRepaint = false;
    if (ev->type() == QEvent::MouseButtonRelease && !m_isShapesWidgetExist) {
           m_moving = false;
           if (!m_isFirstReleaseButton) {
               m_isFirstReleaseButton = true;

               m_mouseStatus = ShotMouseStatus::Normal;
               m_zoomIndicator->hide();

               QPoint toolbarPoint;
               toolbarPoint = QPoint(m_recordX + m_recordWidth - m_toolBar->width(),
                                                       std::max(m_recordY + m_recordHeight + TOOLBAR_Y_SPACING, 0));

               if (m_toolBar->width() > m_recordX + m_recordWidth) {
                   toolbarPoint.setX(m_recordX + 8);
               }
               if (toolbarPoint.y()>= m_backgroundRect.y() + m_backgroundRect.height()
                       - m_toolBar->height() - 28) {
                   if (m_recordY > 28*2 + 10) {
                       toolbarPoint.setY(m_recordY - m_toolBar->height() - TOOLBAR_Y_SPACING);
                   } else {
                       toolbarPoint.setY(m_recordY + TOOLBAR_Y_SPACING);
                   }
               }

               m_toolBar->showAt(toolbarPoint);
               updateCursor(ev);

               // Record select area name with window name if just click (no drag).
               if (!m_isFirstDrag) {
                   QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(ev);
                   for (int i = 0; i < m_windowRects.length(); i++) {
                       int wx = m_windowRects[i].x;
                       int wy = m_windowRects[i].y;
                       int ww = m_windowRects[i].width;
                       int wh = m_windowRects[i].height;
                       int ex = mouseEvent->x();
                       int ey = mouseEvent->y();
                       if (ex > wx && ex < wx + ww && ey > wy && ey < wy + wh) {
   //                        m_selectAreaName = m_windowNames[i];

                           break;
                       }
                   }

               } else {
                   // Make sure record area not too small.
                   m_recordWidth = m_recordWidth < RECORD_MIN_SIZE ?
                               RECORD_MIN_SIZE : m_recordWidth;
                   m_recordHeight = m_recordHeight < RECORD_MIN_SIZE ?
                               RECORD_MIN_SIZE : m_recordHeight;

                   if (m_recordX + m_recordWidth > m_rootWindowRect.width) {
                       m_recordX = m_rootWindowRect.width - m_recordWidth;
                   }

                   if (m_recordY + m_recordHeight > m_rootWindowRect.height) {
                       m_recordY = m_rootWindowRect.height - m_recordHeight;
                   }
               }

               needRepaint = true;
           }

           m_isPressButton = false;
           m_isReleaseButton = true;

           needRepaint = true;
       }

    if (needRepaint) {
        update();
    }
    QLabel::mouseReleaseEvent(ev);
}

void MainWindow::hideEvent(QHideEvent *event) {
    qApp->setOverrideCursor(Qt::ArrowCursor);
    QLabel::hideEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *ev) {
    bool needRepaint = false;

    if (ev->type() == QEvent::MouseMove && !m_isShapesWidgetExist) {
            if (m_recordWidth > 0 && m_recordHeight >0 && !m_needSaveScreenshot && this->isVisible()) {
                m_sizeTips->updateTips(QPoint(m_recordX, m_recordY),
                    QString("%1X%2").arg(m_recordWidth).arg(m_recordHeight));

                if (m_toolBar->isVisible() && m_isPressButton) {
                    QPoint toolbarPoint;
                    toolbarPoint = QPoint(m_recordX + m_recordWidth - m_toolBar->width(),
                                                            std::max(m_recordY + m_recordHeight + TOOLBAR_Y_SPACING, 0));
                    if (m_toolBar->width() > m_recordX + m_recordWidth) {
                        toolbarPoint.setX(m_recordX + 8);
                    }

                    if (toolbarPoint.y()>= m_backgroundRect.y() + m_backgroundRect.height()
                            - m_toolBar->height() - 28 ) {
                        if (m_recordY > 28*2 + 10) {
                            toolbarPoint.setY(m_recordY - m_toolBar->height() - TOOLBAR_Y_SPACING);
                        } else {
                            toolbarPoint.setY(m_recordY + TOOLBAR_Y_SPACING);
                        }
                    }

                    m_toolBar->showAt(toolbarPoint);
                    m_zoomIndicator->hide();
                }
            }

            qDebug() << "There is mouse move!";

            if ( !m_isFirstMove) {
                m_isFirstMove = true;
                needRepaint = true;
            } else {
                if (!m_toolBar->isVisible() && !m_isFirstReleaseButton) {
                    QPoint curPos = this->cursor().pos();
                    QPoint tmpPoint;
                    tmpPoint = QPoint(std::min(curPos.x() + 5 - m_backgroundRect.x(), curPos.x() + 5),
                                      curPos.y() + 5);

                    if (curPos.x() >= m_backgroundRect.x() + m_backgroundRect.width() - m_zoomIndicator->width()) {
                        tmpPoint.setX(std::min(m_backgroundRect.width() - m_zoomIndicator->width() - 5, curPos.x() + 5));
                    }

                    if (curPos.y() >= m_backgroundRect.y() + m_backgroundRect.height() - m_zoomIndicator->height()) {
                        tmpPoint.setY(curPos.y()  - m_zoomIndicator->height() - 5);
                    }

                    m_zoomIndicator->showMagnifier(tmpPoint);
                }
            }

            if (m_isPressButton && m_isFirstPressButton) {
                if (!m_isFirstDrag) {
                    m_isFirstDrag = true;

                    m_selectAreaName = tr("Select area");
                }
            }

            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(ev);

            if (m_isFirstPressButton) {
                if (!m_isFirstReleaseButton) {
                    if (m_isPressButton && !m_isReleaseButton) {
                        m_recordX = std::min(m_dragStartX, mouseEvent->x());
                        m_recordY = std::min(m_dragStartY, mouseEvent->y());
                        m_recordWidth = std::abs(m_dragStartX - mouseEvent->x());
                        m_recordHeight = std::abs(m_dragStartY - mouseEvent->y());

                        needRepaint = true;
                    }
                } else if (m_isPressButton) {
                    if (m_mouseStatus != ShotMouseStatus::Wait && m_dragRecordX >= 0
                            && m_dragRecordY >= 0) {
                        if (m_dragAction == ResizeDirection::Moving && m_moving) {
                            m_recordX = std::max(std::min(m_dragRecordX +
                                mouseEvent->x() - m_dragStartX, m_rootWindowRect.width
                                - m_recordWidth), 1);
                            m_recordY = std::max(std::min(m_dragRecordY + mouseEvent->y()
                                - m_dragStartY, m_rootWindowRect.height - m_recordHeight), 1);
                        } else if (m_dragAction == ResizeDirection::TopLeft) {
                            resizeDirection(ResizeDirection::Top, mouseEvent);
                            resizeDirection(ResizeDirection::Left, mouseEvent);
                        } else if (m_dragAction == ResizeDirection::TopRight) {
                            resizeDirection(ResizeDirection::Top, mouseEvent);
                            resizeDirection(ResizeDirection::Right, mouseEvent);
                        } else if (m_dragAction == ResizeDirection::BottomLeft) {
                            resizeDirection(ResizeDirection::Bottom, mouseEvent);
                            resizeDirection(ResizeDirection::Left, mouseEvent);
                        } else if (m_dragAction == ResizeDirection::BottomRight) {
                            resizeDirection(ResizeDirection::Bottom, mouseEvent);
                            resizeDirection(ResizeDirection::Right, mouseEvent);
                        } else if (m_dragAction == ResizeDirection::Top) {
                            resizeDirection(ResizeDirection::Top, mouseEvent);
                        } else if (m_dragAction == ResizeDirection::Bottom) {
                            resizeDirection(ResizeDirection::Bottom, mouseEvent);
                        } else if (m_dragAction == ResizeDirection::Left) {
                            resizeDirection(ResizeDirection::Left, mouseEvent);
                        } else if (m_dragAction == ResizeDirection::Right) {
                            resizeDirection(ResizeDirection::Right, mouseEvent);
                        }
                        needRepaint = true;
                    }
                }

                updateCursor(ev);

                int mousePosition =  getDirection(ev);
                bool drawPoint = mousePosition != ResizeDirection::Moving;
                if (drawPoint != m_needDrawSelectedPoint) {
                    m_needDrawSelectedPoint = drawPoint;
                    needRepaint = true;
                }
            } else {
                if (m_screenNum == 0) {
                    for (int i = 0; i < m_windowRects.length(); i++) {
                        int wx = m_windowRects[i].x;
                        int wy = m_windowRects[i].y;
                        int ww = m_windowRects[i].width;
                        int wh = m_windowRects[i].height;
                        int ex = mouseEvent->x();
                        int ey = mouseEvent->y();
                        if (ex > wx && ex < wx + ww && ey > wy && ey < wy + wh) {
                            m_recordX = wx;
                            m_recordY = wy;
                            m_recordWidth = ww;
                            m_recordHeight = wh;

                            needRepaint = true;

                            break;
                        }
                    }
                } else {
                    m_recordX = 0;
                    m_recordY = 0;
                    m_recordWidth = m_rootWindowRect.width;
                    m_recordHeight = m_rootWindowRect.height;
                    needRepaint = true;
                }
            }
        }

    if (needRepaint) {
        update();
    }

    QLabel::mouseMoveEvent(ev);
}

int MainWindow::getDirection(QEvent *event) {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    int cursorX = mouseEvent->x();
    int cursorY = mouseEvent->y();

    if (cursorX > m_recordX - SPACING
        && cursorX < m_recordX + SPACING
        && cursorY > m_recordY - SPACING
        && cursorY < m_recordY + SPACING) {
        // Top-Left corner.
        return ResizeDirection::TopLeft;
    } else if (cursorX > m_recordX + m_recordWidth - SPACING
               && cursorX < m_recordX + m_recordWidth + SPACING
               && cursorY > m_recordY + m_recordHeight - SPACING
               && cursorY < m_recordY + m_recordHeight + SPACING) {
        // Bottom-Right corner.
        return  ResizeDirection::BottomRight;
    } else if (cursorX > m_recordX + m_recordWidth - SPACING
               && cursorX < m_recordX + m_recordWidth + SPACING
               && cursorY > m_recordY - SPACING
               && cursorY < m_recordY + SPACING) {
        // Top-Right corner.
        return  ResizeDirection::TopRight;
    } else if (cursorX > m_recordX - SPACING
               && cursorX < m_recordX + SPACING
               && cursorY > m_recordY + m_recordHeight - SPACING
               && cursorY < m_recordY + m_recordHeight + SPACING) {
        // Bottom-Left corner.
        return  ResizeDirection::BottomLeft;
    } else if (cursorX > m_recordX - SPACING
               && cursorX < m_recordX + SPACING) {
        // Left.
        return ResizeDirection::Left;
    } else if (cursorX > m_recordX + m_recordWidth - SPACING
               && cursorX < m_recordX + m_recordWidth + SPACING) {
        // Right.
        return  ResizeDirection::Right;
    } else if (cursorY > m_recordY - SPACING
               && cursorY < m_recordY + SPACING) {
        // Top.
        return ResizeDirection::Top;
    } else if (cursorY > m_recordY + m_recordHeight - SPACING
               && cursorY < m_recordY + m_recordHeight + SPACING) {
        // Bottom.
        return  ResizeDirection::Bottom;
    } else if (cursorX > m_recordX && cursorX < m_recordX + m_recordWidth
               && cursorY > m_recordY && cursorY < m_recordY + m_recordHeight) {
        return ResizeDirection::Moving;
    } else {
        return ResizeDirection::Outting;
    }
}
void MainWindow::paintEvent(QPaintEvent *event)  {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRect backgroundRect = QRect(m_rootWindowRect.x, m_rootWindowRect.y,
                                                              m_rootWindowRect.width, m_rootWindowRect.height);
    // Draw background.
    if (!m_isFirstMove) {
        painter.setBrush(QBrush("#000000"));
        painter.setOpacity(0.5);
        painter.drawRect(backgroundRect);
    } else if (m_recordWidth > 0 && m_recordHeight > 0 && !m_drawNothing) {
        QRect frameRect = QRect(m_recordX + 1, m_recordY + 1, m_recordWidth - 2, m_recordHeight - 2);
        // Draw frame.
        if (m_mouseStatus != ShotMouseStatus::Wait) {
            painter.setRenderHint(QPainter::Antialiasing, false);
            painter.setBrush(QBrush("#000000"));
            painter.setOpacity(0.2);
            painter.setClipping(true);
            painter.setClipRegion(QRegion(backgroundRect).subtracted(QRegion(frameRect)));
            painter.drawRect(backgroundRect);

            painter.setClipRegion(backgroundRect);
            QPen framePen(QColor("#01bdff"));
            framePen.setWidth(2);
            painter.setOpacity(1);
            painter.setBrush(Qt::transparent);
            painter.setPen(framePen);
            painter.drawRect(QRect(frameRect.x(), frameRect.y(), frameRect.width(), frameRect.height()));
            painter.setClipping(false);
        }

        // Draw drag pint.
        if (m_mouseStatus != ShotMouseStatus::Wait && m_needDrawSelectedPoint) {
            painter.setOpacity(1);
            int margin = m_resizeBigPix.width() / 2 + 1;
            int paintX = frameRect.x() - margin;
            int paintY = frameRect.y() - margin;
            int paintWidth = frameRect.x() + frameRect.width() - margin;
            int paintHeight = frameRect.y() + frameRect.height() - margin;
            int paintHalfWidth = frameRect.x() + frameRect.width()/2 - margin;
            int paintHalfHeight = frameRect.y() + frameRect.height()/2 - margin;
            paintSelectedPoint(painter, QPoint(paintX, paintY), m_resizeBigPix);
            paintSelectedPoint(painter, QPoint(paintWidth, paintY), m_resizeBigPix);
            paintSelectedPoint(painter, QPoint(paintX, paintHeight), m_resizeBigPix);
            paintSelectedPoint(painter, QPoint(paintWidth, paintHeight), m_resizeBigPix);

            paintSelectedPoint(painter, QPoint(paintX, paintHalfHeight), m_resizeBigPix);
            paintSelectedPoint(painter, QPoint(paintHalfWidth, paintY), m_resizeBigPix);
            paintSelectedPoint(painter, QPoint(paintWidth, paintHalfHeight), m_resizeBigPix);
            paintSelectedPoint(painter, QPoint(paintHalfWidth, paintHeight), m_resizeBigPix);
        }
    }

}

void MainWindow::initShapeWidget(QString type) {
    qDebug() << "show shapesWidget";
    m_shapesWidget = new ShapesWidget(this);
    m_shapesWidget->setShiftKeyPressed(m_isShiftPressed);

    if (type != "color")
        m_shapesWidget->setCurrentShape(type);

    m_shapesWidget->show();
    m_shapesWidget->setFixedSize(m_recordWidth - 4, m_recordHeight - 4);
    m_shapesWidget->move(m_recordX + 2, m_recordY + 2);

    QPoint toolbarPoint;
    toolbarPoint = QPoint(m_recordX + m_recordWidth - m_toolBar->width(),
                          std::max(m_recordY + m_recordHeight + TOOLBAR_Y_SPACING, 0));

    if (m_toolBar->width() > m_recordX + m_recordWidth) {
        toolbarPoint.setX(m_recordX + 8);
    }

    if (toolbarPoint.y()>= m_backgroundRect.y() + m_backgroundRect.height()
            - m_toolBar->height() - 28) {
        if (m_recordY > 28*2 + 10) {
            toolbarPoint.setY(m_recordY - m_toolBar->height() - TOOLBAR_Y_SPACING);
        } else {
            toolbarPoint.setY(m_recordY + TOOLBAR_Y_SPACING);
        }
    }

    m_toolBar->showAt(toolbarPoint);
    m_toolBar->raise();
    m_needDrawSelectedPoint = false;
    update();

    connect(m_toolBar, &ToolBar::updateColor,
            m_shapesWidget, &ShapesWidget::setPenColor);
    connect(m_shapesWidget, &ShapesWidget::reloadEffectImg,
            this, &MainWindow::reloadImage);
    connect(this, &MainWindow::deleteShapes, m_shapesWidget,
            &ShapesWidget::deleteCurrentShape);
    connect(m_shapesWidget, &ShapesWidget::requestScreenshot,
            this, &MainWindow::saveScreenshot);
    connect(m_shapesWidget, &ShapesWidget::shapePressed,
            m_toolBar, &ToolBar::shapePressed);
    connect(m_shapesWidget, &ShapesWidget::saveBtnPressed,
            m_toolBar, &ToolBar::saveBtnPressed);
    connect(m_shapesWidget, &ShapesWidget::requestExit, this, &MainWindow::exitApp);
    connect(this, &MainWindow::unDo, m_shapesWidget, &ShapesWidget::undoDrawShapes);
}

void MainWindow::updateCursor(QEvent *event)
{
    if (m_mouseStatus == ShotMouseStatus::Normal) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        int cursorX = mouseEvent->x();
        int cursorY = mouseEvent->y();

        if (cursorX > m_recordX - SPACING
            && cursorX < m_recordX + SPACING
            && cursorY > m_recordY - SPACING
            && cursorY < m_recordY + SPACING) {
            // Top-Left corner.
            qApp->setOverrideCursor(Qt::SizeFDiagCursor);
        } else if (cursorX > m_recordX + m_recordWidth - SPACING
                   && cursorX < m_recordX + m_recordWidth + SPACING
                   && cursorY > m_recordY + m_recordHeight - SPACING
                   && cursorY < m_recordY + m_recordHeight + SPACING) {
            // Bottom-Right corner.
            qApp->setOverrideCursor(Qt::SizeFDiagCursor);
        } else if (cursorX > m_recordX + m_recordWidth - SPACING
                   && cursorX < m_recordX + m_recordWidth + SPACING
                   && cursorY > m_recordY - SPACING
                   && cursorY < m_recordY + SPACING) {
            // Top-Right corner.
            qApp->setOverrideCursor(Qt::SizeBDiagCursor);
        } else if (cursorX > m_recordX - SPACING
                   && cursorX < m_recordX + SPACING
                   && cursorY > m_recordY + m_recordHeight - SPACING
                   && cursorY < m_recordY + m_recordHeight + SPACING) {
            // Bottom-Left corner.
            qApp->setOverrideCursor(Qt::SizeBDiagCursor);
        } else if (cursorX > m_recordX - SPACING
                   && cursorX < m_recordX + SPACING) {
            // Left.
            qApp->setOverrideCursor(Qt::SizeHorCursor);
        } else if (cursorX > m_recordX + m_recordWidth - SPACING
                   && cursorX < m_recordX + m_recordWidth + SPACING) {
            // Right.
            qApp->setOverrideCursor(Qt::SizeHorCursor);
        } else if (cursorY > m_recordY - SPACING
                   && cursorY < m_recordY + SPACING) {
            // Top.
            qApp->setOverrideCursor(Qt::SizeVerCursor);
        } else if (cursorY > m_recordY + m_recordHeight - SPACING
                   && cursorY < m_recordY + m_recordHeight + SPACING) {
            // Bottom.
            qApp->setOverrideCursor(Qt::SizeVerCursor);
        } /*else if (recordButton->geometry().contains(cursorX, cursorY) ||
             recordOptionPanel->geometry().contains(cursorX, cursorY)) {
            // Record area.
            qApp->setOverrideCursor(Qt::ArrowCursor);
        }*/ else {
            if (m_isPressButton) {
                qApp->setOverrideCursor(Qt::ClosedHandCursor);
            } else {
                qApp->setOverrideCursor(Qt::OpenHandCursor);
            }
        }
    }
}

void MainWindow::resizeDirection(ResizeDirection direction,
                                 QMouseEvent *e) {
    int offsetX = e->x() - m_dragStartX;
    int offsetY = e->y() - m_dragStartY;

    switch (direction) {
    case ResizeDirection::Top: {
        m_recordY = std::max(std::min(m_dragRecordY + offsetY,
                    m_dragRecordY + m_dragRecordHeight - RECORD_MIN_SIZE), 1);
        m_recordHeight = std::max(std::min(m_dragRecordHeight -
                      offsetY, m_rootWindowRect.height), RECORD_MIN_SIZE);
        break;
    };
    case ResizeDirection::Bottom: {
        m_recordHeight = std::max(std::min(m_dragRecordHeight + offsetY,
                                         m_rootWindowRect.height), RECORD_MIN_SIZE);
        break;
    };
    case ResizeDirection::Left: {
        m_recordX = std::max(std::min(m_dragRecordX + offsetX,
                    m_dragRecordX + m_dragRecordWidth - RECORD_MIN_SIZE), 1);
        m_recordWidth = std::max(std::min(m_dragRecordWidth - offsetX,
                    m_rootWindowRect.width), RECORD_MIN_SIZE);
        break;
    };
    case ResizeDirection::Right: {
        m_recordWidth = std::max(std::min(m_dragRecordWidth + offsetX,
                        m_rootWindowRect.width), RECORD_MIN_SIZE);
        break;
    };
    default:break;
    }

}

void MainWindow::fullScreenshot() {
    m_mouseStatus = ShotMouseStatus::Shoting;
    repaint();
    qApp->setOverrideCursor(setCursorShape("start"));
    initDBusInterface();
    this->setFocus();
    m_configSettings =  ConfigSettings::instance();
    installEventFilter(this);
    m_hotZoneInterface->asyncCall("EnableZoneDetected", false);

    QPoint curPos = this->cursor().pos();
     m_screenNum = qApp->desktop()->screenNumber(curPos);
     QList<QScreen*> screenList = qApp->screens();
     if (m_screenNum != 0 && m_screenNum < screenList.length()) {
        m_backgroundRect = screenList[m_screenNum]->geometry();
     } else {
         m_backgroundRect =  qApp->primaryScreen()->geometry();
     }

     this->move(m_backgroundRect.x(), m_backgroundRect.y());
     this->setFixedSize(m_backgroundRect.size());
     m_needSaveScreenshot = true;
     shotFullScreen();
     m_toolBar = new ToolBar(this);
     m_toolBar->hide();
     m_hotZoneInterface->asyncCall("EnableZoneDetected",  true);

    using namespace utils;
    QPixmap screenShotPix(TMP_FULLSCREEN_FILE);
    saveAction(screenShotPix);
    sendNotify(m_saveIndex, m_saveFileName);
}

void MainWindow::savePath(const QString &path) {
    if (!QFileInfo(path).dir().exists()) {
        exitApp();
    }

    startScreenshot();
    m_toolBar->specificedSavePath();
    m_specificedPath = path;

    connect(m_toolBar, &ToolBar::saveSpecifiedPath, this, [=]{
        emit releaseEvent();
        m_needSaveScreenshot = true;
        saveSpecificedPath(m_specificedPath);
    });
}

void MainWindow::saveSpecificedPath(QString path) {
    QString savePath;
    QString baseName = QFileInfo(path).baseName();
    QString suffix = QFileInfo(path).completeSuffix();
    if (!baseName.isEmpty()) {
        if (isValidFormat(suffix)) {
            savePath = path;
        } else if (suffix.isEmpty()) {
            savePath = path + ".png";
        } else {
            qWarning() << "Invalid image format! Screenshot will quit, suffix:" << suffix;
            exitApp();
        }
    } else {
        QDateTime currentDate;
        QString currentTime =  currentDate.currentDateTime().
                toString("yyyyMMddHHmmss");
        savePath = path + QString(tr("DeepinScreenshot%1").arg(currentTime));
    }

    m_hotZoneInterface->asyncCall("EnableZoneDetected",  true);
    using namespace utils;
    m_toolBar->setVisible(false);
    m_sizeTips->setVisible(false);

    shotCurrentImg();

    QPixmap screenShotPix(TMP_FILE);
    screenShotPix.save(savePath);
    QStringList actions;
    actions << "_open" << tr("View");
    QVariantMap hints;
    QString fileDir = QUrl::fromLocalFile(QFileInfo(savePath).absoluteDir().absolutePath()).toString();
    QString filePath =  QUrl::fromLocalFile(savePath).toString();
    QString command;
    if (QFile("/usr/bin/dde-file-manager").exists()) {
        command = QString("/usr/bin/dde-file-manager,%1?selectUrl=%2"
                          ).arg(fileDir).arg(filePath);
    } else {
        command = QString("xdg-open,%1").arg(filePath);
    }

    hints["x-deepin-action-_open"] = command;

    QString summary = QString(tr("Picture has been saved to %1")).arg(savePath);

    m_notifyDBInterface->Notify("Deepin Screenshot", 0,  "deepin-screenshot", "",
                                summary, actions, hints, 0);
    exitApp();
}

void MainWindow::delayScreenshot(int num) {
    initDBusInterface();
    QString summary = QString(tr("Deepin Screenshot will start after %1 second.").arg(num));
    QStringList actions = QStringList();
    QVariantMap hints;
    if (num >= 2) {
        m_notifyDBInterface->Notify("Deepin Screenshot", 0,  "deepin-screenshot", "",
                                    summary, actions, hints, 0);
        QTimer* timer = new QTimer;
        timer->setSingleShot(true);
        timer->start(1000*num);
        connect(timer, &QTimer::timeout, this, [=]{
            m_notifyDBInterface->CloseNotification(0);
            initUI();
            initShortcut();
            this->show();
        });
    } else {
        initUI();
        initShortcut();
        this->show();
    }
}

void MainWindow::noNotify() {
    m_controlCenterDBInterface = new DBusControlCenter(this);
    m_hotZoneInterface = new DBusZone(this);
    m_interfaceExist = true;
    m_noNotify = true;
    initUI();
    initShortcut();
    this->show();
}

void MainWindow::topWindow() {
    initDBusInterface();
    initUI();
    if (m_screenNum == 0) {
        QList<xcb_window_t> windows = m_windowManager->getWindows();
        for (int i = 0; i < windows.length(); i++) {
            m_windowRects.append(m_windowManager->adjustRectInScreenArea(
                                     m_windowManager->getWindowRect(windows[i])));
        }
        m_recordX = m_windowRects[0].x;
        m_recordY = m_windowRects[0].y;
        m_recordWidth = m_windowRects[0].width;
        m_recordHeight = m_windowRects[0].height;
    } else {
        m_recordX = m_backgroundRect.x();
        m_recordY = m_backgroundRect.y();
        m_recordWidth = m_backgroundRect.width();
        m_recordHeight = m_backgroundRect.height();
    }

    this->hide();
    using namespace utils;
    QPixmap screenShotPix = QPixmap(TMP_FULLSCREEN_FILE).copy(m_recordX, m_recordY,
                                                              m_recordWidth, m_recordHeight);
    m_needSaveScreenshot = true;
    saveAction(screenShotPix);
    sendNotify(m_saveIndex, m_saveFileName);
}

void MainWindow::startScreenshot() {
    m_mouseStatus = ShotMouseStatus::Shoting;
    repaint();
    qApp->setOverrideCursor(setCursorShape("start"));
    initDBusInterface();
    initUI();
    initShortcut();
    this->show();
}

void MainWindow::showPressFeedback(int x, int y)
{
    if (m_mouseStatus == ShotMouseStatus::Shoting) {
//        buttonFeedback->showPressFeedback(x, y);
    }
}

void MainWindow::showDragFeedback(int x, int y)
{
    if (m_mouseStatus == ShotMouseStatus::Shoting) {
//        buttonFeedback->showDragFeedback(x, y);
    }
}

void MainWindow::showReleaseFeedback(int x, int y)
{
    if (m_mouseStatus == ShotMouseStatus::Shoting) {
//        buttonFeedback->showReleaseFeedback(x, y);
    }
}

void MainWindow::responseEsc()
{
//    if (recordButtonStatus != ShotMouseStatus::Shoting) {
        exitApp();
//    }
}

void MainWindow::initBackground() {
    QList<QScreen*> screenList = qApp->screens();
    QPixmap tmpImg =  screenList[m_screenNum]->grabWindow(
                qApp->desktop()->screen(m_screenNum)->winId(),
                m_backgroundRect.x(), m_backgroundRect.y(),
                m_backgroundRect.width(), m_backgroundRect.height());

    using namespace utils;
    tmpImg.save(TMP_FULLSCREEN_FILE, "png");
    this->setStyleSheet(QString("MainWindow{ border-image: url(%1); }"
                                       ).arg(TMP_FULLSCREEN_FILE));
}

void MainWindow::shotFullScreen() {
    QList<QScreen*> screenList = qApp->screens();
    QPixmap tmpImg =  screenList[m_screenNum]->grabWindow(
                qApp->desktop()->screen(m_screenNum)->winId(),
                m_backgroundRect.x(), m_backgroundRect.y(),
                m_backgroundRect.width(), m_backgroundRect.height());

    using namespace utils;
    tmpImg.save(TMP_FULLSCREEN_FILE, "png");
}

void MainWindow::shotCurrentImg() {
    if (m_recordWidth == 0 || m_recordHeight == 0)
        return;

    m_needDrawSelectedPoint = false;
    m_drawNothing = true;
    update();

    QEventLoop eventloop1;
    QTimer::singleShot(100, &eventloop1, SLOT(quit()));
    eventloop1.exec();

    qDebug() << "shotCurrentImg shotFullScreen";
    using namespace utils;
    shotFullScreen();
    if (m_isShapesWidgetExist) {
        m_shapesWidget->hide();
    }

    this->hide();
    emit hideScreenshotUI();

    QPixmap tmpImg(TMP_FULLSCREEN_FILE);
    tmpImg = tmpImg.copy(QRect(m_recordX, m_recordY, m_recordWidth, m_recordHeight));
    tmpImg.save(TMP_FILE, "png");
}

void MainWindow::shotImgWidthEffect() {
    if (m_recordWidth == 0 || m_recordHeight == 0)
        return;

    m_needDrawSelectedPoint = false;
    m_drawNothing = true;
    update();

    QEventLoop eventloop;
    QTimer::singleShot(100, &eventloop, SLOT(quit()));
    eventloop.exec();

    qDebug() << m_toolBar->isVisible() << m_sizeTips->isVisible();
    QList<QScreen*> screenList = qApp->screens();
    QPixmap tmpImg =  screenList[m_screenNum]->grabWindow(
                qApp->desktop()->screen(m_screenNum)->winId(),
                m_recordX + m_backgroundRect.x(), m_recordY, m_recordWidth, m_recordHeight);
    qDebug() << tmpImg.isNull() << tmpImg.size();

    using namespace utils;
    tmpImg.save(TMP_FILE, "png");
    m_drawNothing = false;
    update();
}

void MainWindow::saveScreenshot() {
    emit releaseEvent();

    m_hotZoneInterface->asyncCall("EnableZoneDetected",  true);
    m_needSaveScreenshot = true;

    m_toolBar->setVisible(false);
    m_sizeTips->setVisible(false);

    shotCurrentImg();

    using namespace utils;
    saveAction(utils::TMP_FILE);
    sendNotify(m_saveIndex, m_saveFileName);
}

void MainWindow::saveAction(QPixmap pix) {
    emit releaseEvent();

    using namespace utils;
    QPixmap screenShotPix = pix;
    QDateTime currentDate;
    QString currentTime =  currentDate.currentDateTime().
            toString("yyyyMMddHHmmss");
    m_saveFileName = "";

    QStandardPaths::StandardLocation saveOption = QStandardPaths::TempLocation;
    bool copyToClipboard = false;
    m_saveIndex =  ConfigSettings::instance()->value("save", "save_op").toInt();
    switch (m_saveIndex) {
    case 0: {
        saveOption = QStandardPaths::DesktopLocation;
        ConfigSettings::instance()->setValue("common", "default_savepath", QStandardPaths::writableLocation(
                                                 QStandardPaths::DesktopLocation));
        break;
    }
    case 1: {
        QString defaultSaveDir = ConfigSettings::instance()->value("common", "default_savepath").toString();
        if (defaultSaveDir.isEmpty()) {
            saveOption = QStandardPaths::DesktopLocation;
        } else if (defaultSaveDir == "clipboard") {
            copyToClipboard = true;
            m_saveIndex = 3;
        } else {
            m_saveFileName = QString("%1/%2%3.png").arg(defaultSaveDir).arg(tr(
                                                                           "DeepinScreenshot")).arg(currentTime);
        }
        break;
    }
    case 2: {
        this->hide();
        this->releaseKeyboard();
        QFileDialog fileDialog;
        QString  lastFileName = QString("%1/%2%3.png").arg(QStandardPaths::writableLocation(
                        QStandardPaths::PicturesLocation)).arg(tr("DeepinScreenshot")).arg(currentTime);
        m_saveFileName =  fileDialog.getSaveFileName(this, "Save",  lastFileName,
                                                     tr("PNG (*.png);;JPEG (*.jpg *.jpeg);; BMP (*.bmp);; PGM (*.pgm);;"
                                                        "XBM (*.xbm);;XPM(*.xpm);;"));
        if (m_saveFileName.isEmpty()) {
            exitApp();
        }

        QString fileSuffix = QFileInfo(m_saveFileName).completeSuffix();
        if ( !isValidFormat(fileSuffix)) {
            qWarning() << "The fileName has invalid suffix!" << fileSuffix << m_saveFileName;
            exitApp();
        }

        ConfigSettings::instance()->setValue("common", "default_savepath",
                                             QFileInfo(m_saveFileName).dir().absolutePath());
        break;
    }
    case 3: {
        copyToClipboard = true;
        ConfigSettings::instance()->setValue("common", "default_savepath",   "clipboard");
        break;
    }
    case 4: {
        copyToClipboard = true;
        QString defaultSaveDir = ConfigSettings::instance()->value("common", "default_savepath").toString();
        if (defaultSaveDir.isEmpty()) {
            saveOption = QStandardPaths::DesktopLocation;
        } else if (defaultSaveDir == "clipboard") {
            m_saveIndex = 3;
        } else  {
            m_saveFileName = QString("%1/%2%3.png").arg(defaultSaveDir).arg(tr(
                                                                               "DeepinScreenshot")).arg(currentTime);
        }
        break;
    }
    default:
        break;
    }

    int toolBarSaveQuality = std::min(ConfigSettings::instance()->value("save",
                                                            "save_quality").toInt(), 100);

    if (toolBarSaveQuality != 100) {
       qreal saveQuality = qreal(toolBarSaveQuality)*5/1000 + 0.5;

       int pixWidth = screenShotPix.width();
       int pixHeight = screenShotPix.height();
        screenShotPix = screenShotPix.scaled(pixWidth*saveQuality, pixHeight*saveQuality,
                                                                             Qt::KeepAspectRatio, Qt::FastTransformation);
        screenShotPix = screenShotPix.scaled(pixWidth,  pixHeight,
                                                                            Qt::KeepAspectRatio, Qt::FastTransformation);
    }

    if (m_saveIndex ==2 && m_saveFileName.isEmpty()) {
        exitApp();
        return;
    } else if (m_saveIndex == 2 || !m_saveFileName.isEmpty()) {
        screenShotPix.save(m_saveFileName,  QFileInfo(m_saveFileName).suffix().toLocal8Bit());
    } else if (saveOption != QStandardPaths::TempLocation || m_saveFileName.isEmpty()) {
        m_saveFileName = QString("%1/%2%3.png").arg(QStandardPaths::writableLocation(
                             saveOption)).arg(tr("DeepinScreenshot")).arg(currentTime);
        screenShotPix.save(m_saveFileName,  "PNG");
    }

    if (copyToClipboard) {
        Q_ASSERT(!screenShotPix.isNull());
        QClipboard* cb = qApp->clipboard();
        cb->setPixmap(screenShotPix, QClipboard::Clipboard);
    }
}

void MainWindow::sendNotify(int saveIndex, QString saveFilePath) {
    QStringList actions;
    actions << "_open" << tr("View");
    QVariantMap hints;
    QString fileDir = QUrl::fromLocalFile(QFileInfo(saveFilePath).absoluteDir().absolutePath()).toString();
    QString filePath =  QUrl::fromLocalFile(saveFilePath).toString();
    QString command;
    if (QFile("/usr/bin/dde-file-manager").exists()) {
        command = QString("/usr/bin/dde-file-manager,%1?selectUrl=%2"
                          ).arg(fileDir).arg(filePath);
    } else {
        command = QString("xdg-open,%1").arg(filePath);
    }

    hints["x-deepin-action-_open"] = command;

    qDebug() << "saveFilePath:" << saveFilePath;

   QString summary;
   if (saveIndex == 3) {
       summary = QString(tr("Picture has been saved to clipboard"));
   } else {
       summary = QString(tr("Picture has been saved to %1")).arg(saveFilePath);
   }

   if (saveIndex == 3 && !m_noNotify) {
       QVariantMap emptyMap;
       m_notifyDBInterface->Notify("Deepin Screenshot", 0,  "deepin-screenshot", "",
                               summary,  QStringList(), emptyMap, 0);
   }  else if ( !m_noNotify &&  !(m_saveIndex == 2 && m_saveFileName.isEmpty())) {
       m_notifyDBInterface->Notify("Deepin Screenshot", 0,  "deepin-screenshot", "",
                               summary, actions, hints, 0);
   }

   QTimer::singleShot(4000, this, [=]{
          qApp->quit();
   });
}

void MainWindow::reloadImage(QString effect) {
    //**save tmp image file
    shotImgWidthEffect();
    using namespace utils;
    const int radius = 10;
    QPixmap tmpImg(TMP_FILE);
    int imgWidth = tmpImg.width();
    int imgHeight = tmpImg.height();
    if (effect == "blur") {
        if (!tmpImg.isNull()) {
            tmpImg = tmpImg.scaled(imgWidth/radius, imgHeight/radius,
                                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            tmpImg = tmpImg.scaled(imgWidth, imgHeight, Qt::IgnoreAspectRatio,
                                     Qt::SmoothTransformation);
            tmpImg.save(TMP_BLUR_FILE, "png");
        }
    } else {
        if (!tmpImg.isNull()) {
            tmpImg = tmpImg.scaled(imgWidth/radius, imgHeight/radius,
                                   Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            tmpImg = tmpImg.scaled(imgWidth, imgHeight);
            tmpImg.save(TMP_MOSA_FILE, "png");
        }
    }
}

void MainWindow::onViewShortcut() {
    QRect rect = window()->geometry();
    QPoint pos(rect.x() + rect.width()/2, rect.y() + rect.height()/2);
    Shortcut sc;
    QStringList shortcutString;
    QString param1 = "-j=" + sc.toStr();
    QString param2 = "-p=" + QString::number(pos.x()) + "," + QString::number(pos.y());
    shortcutString << param1 << param2;

    QProcess* shortcutViewProc = new QProcess();
    shortcutViewProc->startDetached("deepin-shortcut-viewer", shortcutString);

    connect(shortcutViewProc, SIGNAL(finished(int)), shortcutViewProc, SLOT(deleteLater()));
}

void MainWindow::onHelp() {
    using namespace utils;
    if (m_manualPro.isNull()) {
        const QString pro = "dman";
        const QStringList args("deepin-screenshot");
        m_manualPro = new QProcess();
        connect(m_manualPro.data(), SIGNAL(finished(int)),
                m_manualPro.data(), SLOT(deleteLater()));
        this->hide();
        m_manualPro->start(pro, args);

        QTimer::singleShot(1000, this, [=]{
            exitApp();
        });
    }
}

void MainWindow::exitApp() {
    if (m_interfaceExist && nullptr != m_hotZoneInterface) {
        m_hotZoneInterface->asyncCall("EnableZoneDetected",  true);
    }
    qApp->quit();
}
