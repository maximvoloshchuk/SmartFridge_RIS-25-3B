#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include <QObject>
#include <QTimer>
#include <QStringList>
#include "product.h"

class NotificationManager : public QObject
{
    Q_OBJECT

public:
    explicit NotificationManager(QObject *parent = nullptr);

    void startMonitoring(int intervalMs = 30000);
    void stopMonitoring();
    QStringList getPendingNotifications() const;
    void clearNotifications();

signals:
    void newNotification(const QString &message);
    void productExpiringAlert(const Product &product);
    void favoriteLowStockAlert(const Product &product);

private slots:
    void checkProducts();

private:
    QTimer *m_timer;
    QStringList m_pendingNotifications;
};

#endif // NOTIFICATIONMANAGER_H
