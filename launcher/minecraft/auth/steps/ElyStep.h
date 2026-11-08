// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "minecraft/auth/AuthStep.h"

#include <QNetworkReply>
#include <QTimer>

class ElyStep : public AuthStep {
    Q_OBJECT

   public:
    explicit ElyStep(AccountData* data, bool silent = false);
    ~ElyStep() noexcept override = default;

    void perform() override;
    QString describe() override;

   public slots:
    void abort() override;

   signals:
    void authorizeWithBrowser(const QUrl& url);

   private:
    void startLogin();
    void startRefresh();
    void pollResult();
    void handleTokenResponse(const QJsonObject& response);
    void fail(AccountTaskState state, const QString& message);

    bool m_silent = false;
    QString m_sessionId;
    QNetworkReply* m_reply = nullptr;
    QTimer m_pollTimer;
    int m_pollAttempts = 0;
};
