#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include "notificationmanager.h"
#include "product.h"
#include "telegrambot.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnAddProduct_clicked();
    void on_btnShoppingList_clicked();
    void on_btnHistory_clicked();
    void on_btnTakeProduct_clicked();
    void on_btnReturnProduct_clicked();
    void on_btnDeleteProduct_clicked();
    void on_lineEditSearch_textChanged(const QString &text);
    void on_lineEditSearch_returnPressed();
    void on_comboBoxCategory_currentIndexChanged();
    void on_actionRefresh_triggered();
    void on_actionShowFavorites_triggered();
    void on_actionClearHistory_triggered();
    void on_actionExportReport_triggered();
    void on_actionAbout_triggered();
    void on_actionExit_triggered();
    void onNotificationReceived(const QString &message);
    void on_btnToggleFavorite_clicked();

private:
    TelegramBot *m_telegramBot;
    QTcpServer *m_scannerServer;
    Ui::MainWindow *ui;
    NotificationManager *m_notificationManager;
    bool m_showFavoritesOnly;

    Product m_lastTakenProduct;
    int m_lastTakenQuantity;
    bool m_hasLastTaken;

    void refreshTable();
    void refreshFridgeView();
    void updateStatusBar();
    void showStatusMessage(const QString &msg, int timeout = 5000);
    void applyFilter();
    void startScannerServer();
};

#endif // MAINWINDOW_H
