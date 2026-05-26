#include "notificationmanager.h"
#include "storagemanager.h"

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent), m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &NotificationManager::checkProducts);
}

void NotificationManager::startMonitoring(int intervalMs)
{
    checkProducts();
    m_timer->start(intervalMs);
}

void NotificationManager::stopMonitoring()
{
    m_timer->stop();
}

QStringList NotificationManager::getPendingNotifications() const
{
    return m_pendingNotifications;
}

void NotificationManager::clearNotifications()
{
    m_pendingNotifications.clear();
}

void NotificationManager::checkProducts()
{
    auto &storage = StorageManager::instance();
    QStringList newNotes;

    // Просроченные
    QList<Product> expired = storage.getFridgeExpired(0);
    for (const auto &p : expired) {
        QString msg = QString("🚨 ПРОСРОЧЕН: %1 (просрочен %2 дн. назад!)")
                          .arg(p.name()).arg(-p.daysUntilExpired());
        newNotes.append(msg);
        emit newNotification(msg);
    }

    // Заканчиваются
    QList<Product> soon = storage.getFridgeExpiringSoon(0, 3);
    for (const auto &p : soon) {
        QString msg = QString("⚠️ Истекает срок: %1 — осталось %2 дн.")
                          .arg(p.name()).arg(p.daysUntilExpired());
        newNotes.append(msg);
        emit newNotification(msg);
    }

    // Рекомендации по хранению
    QList<Product> all = storage.getFridgeProducts(0);
    for (const auto &p : all) {
        QString rec = p.storageWarning();
        if (rec.startsWith("⚠️")) {
            QString msg = QString("📋 Рекомендация: %1 — %2")
                              .arg(p.name()).arg(rec.mid(2));
            if (!m_pendingNotifications.contains(msg) && !newNotes.contains(msg)) {
                newNotes.append(msg);
                emit newNotification(msg);
            }
        }
    }

    m_pendingNotifications = newNotes;
}
