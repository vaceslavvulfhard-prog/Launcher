// SPDX-License-Identifier: GPL-3.0-only
/*
 * KilmorRPG Ely.by OAuth bridge integration.
 * The Ely.by client secret is kept exclusively by the HTTPS bridge service.
 */
#include "ElyStep.h"
#include "Application.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUrl>

namespace {
const QUrl bridgeUrl(QStringLiteral("https://kilmorrpg-ely-auth.onrender.com"));
constexpr int PollIntervalMs = 1500;
constexpr int MaximumPollAttempts = 400;
}

ElyStep::ElyStep(AccountData* data, bool silent) : AuthStep(data), m_silent(silent)
{
    m_pollTimer.setInterval(PollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &ElyStep::pollResult);
}

QString ElyStep::describe()
{
    return m_silent ? tr("Refreshing Ely.by session.") : tr("Signing in with Ely.by.");
}

void ElyStep::perform()
{
    if (m_silent) {
        startRefresh();
        return;
    }
    const auto accountType = m_data->type;
    *m_data = AccountData();
    m_data->type = accountType;
    startLogin();
}

void ElyStep::startLogin()
{
    QNetworkRequest request(bridgeUrl.resolved(QUrl("/auth/start")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Cache-Control", "no-store");
    m_reply = APPLICATION->network()->post(request, QByteArray("{}"));
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        auto* reply = m_reply;
        m_reply = nullptr;
        const auto payload = reply->readAll();
        const auto error = reply->error();
        const auto errorText = reply->errorString();
        reply->deleteLater();
        if (error != QNetworkReply::NoError) {
            fail(AccountTaskState::STATE_OFFLINE, tr("Could not start Ely.by sign-in: %1").arg(errorText));
            return;
        }
        QJsonParseError parseError;
        const auto object = QJsonDocument::fromJson(payload, &parseError).object();
        const auto sessionId = object.value("session_id").toString();
        const QUrl authorizationUrl(object.value("authorization_url").toString());
        if (parseError.error != QJsonParseError::NoError || sessionId.isEmpty() ||
            !authorizationUrl.isValid() || authorizationUrl.scheme() != "https" ||
            authorizationUrl.host() != "account.ely.by") {
            fail(AccountTaskState::STATE_FAILED_SOFT, tr("The Ely.by sign-in service returned an invalid response."));
            return;
        }
        m_sessionId = sessionId;
        emit authorizeWithBrowser(authorizationUrl);
        m_pollAttempts = 0;
        m_pollTimer.start();
    });
}

void ElyStep::startRefresh()
{
    if (m_data->msaToken.refresh_token.isEmpty()) {
        fail(AccountTaskState::STATE_DISABLED, tr("The saved Ely.by refresh token is missing. Please sign in again."));
        return;
    }
    QNetworkRequest request(bridgeUrl.resolved(QUrl("/auth/refresh")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Cache-Control", "no-store");
    QJsonObject body;
    body.insert("refresh_token", m_data->msaToken.refresh_token);
    m_reply = APPLICATION->network()->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        auto* reply = m_reply;
        m_reply = nullptr;
        const auto payload = reply->readAll();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = reply->error();
        const auto errorText = reply->errorString();
        reply->deleteLater();
        if (error != QNetworkReply::NoError) {
            if (status == 400 || status == 401)
                fail(AccountTaskState::STATE_FAILED_HARD, tr("The Ely.by session has expired. Please sign in again."));
            else
                fail(AccountTaskState::STATE_OFFLINE, tr("Could not refresh the Ely.by session: %1").arg(errorText));
            return;
        }
        QJsonParseError parseError;
        const auto object = QJsonDocument::fromJson(payload, &parseError).object();
        if (parseError.error != QJsonParseError::NoError || object.value("status").toString() != "complete" ||
            !object.value("tokens").isObject()) {
            fail(AccountTaskState::STATE_FAILED_SOFT, tr("The Ely.by refresh service returned an invalid response."));
            return;
        }
        handleTokenResponse(object.value("tokens").toObject());
    });
}

void ElyStep::pollResult()
{
    if (m_reply) return;
    if (++m_pollAttempts > MaximumPollAttempts) {
        m_pollTimer.stop();
        fail(AccountTaskState::STATE_FAILED_SOFT, tr("Ely.by sign-in timed out. Please try again."));
        return;
    }
    const auto encodedId = QString::fromLatin1(QUrl::toPercentEncoding(m_sessionId));
    QNetworkRequest request(bridgeUrl.resolved(QUrl("/auth/result/" + encodedId)));
    request.setRawHeader("Cache-Control", "no-store");
    m_reply = APPLICATION->network()->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        auto* reply = m_reply;
        m_reply = nullptr;
        const auto payload = reply->readAll();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = reply->error();
        const auto errorText = reply->errorString();
        reply->deleteLater();
        if (status == 202) return;
        if (error != QNetworkReply::NoError) {
            m_pollTimer.stop();
            if (status == 400 || status == 404)
                fail(AccountTaskState::STATE_FAILED_HARD, tr("Ely.by sign-in was rejected or expired. Please try again."));
            else
                fail(AccountTaskState::STATE_OFFLINE, tr("Could not retrieve Ely.by sign-in result: %1").arg(errorText));
            return;
        }
        QJsonParseError parseError;
        const auto object = QJsonDocument::fromJson(payload, &parseError).object();
        if (parseError.error != QJsonParseError::NoError || object.value("status").toString() != "complete" ||
            !object.value("tokens").isObject()) {
            m_pollTimer.stop();
            fail(AccountTaskState::STATE_FAILED_SOFT, tr("The Ely.by service returned an invalid sign-in result."));
            return;
        }
        m_pollTimer.stop();
        handleTokenResponse(object.value("tokens").toObject());
    });
}

void ElyStep::handleTokenResponse(const QJsonObject& tokens)
{
    const auto accessToken = tokens.value("access_token").toString();
    if (accessToken.isEmpty()) {
        fail(AccountTaskState::STATE_FAILED_SOFT, tr("Ely.by did not return an access token."));
        return;
    }
    const auto previousRefreshToken = m_data->msaToken.refresh_token;
    m_data->msaToken.issueInstant = QDateTime::currentDateTimeUtc();
    const auto expiresIn = tokens.value("expires_in").toVariant().toLongLong();
    if (expiresIn > 0) m_data->msaToken.notAfter = m_data->msaToken.issueInstant.addSecs(expiresIn);
    m_data->msaToken.token = accessToken;
    m_data->msaToken.refresh_token = tokens.value("refresh_token").toString();
    if (m_data->msaToken.refresh_token.isEmpty()) m_data->msaToken.refresh_token = previousRefreshToken;
    m_data->msaToken.extra = tokens.toVariantMap();
    m_data->msaToken.validity = Validity::Certain;
    m_data->validity_ = Validity::Certain;
    emit finished(AccountTaskState::STATE_WORKING, tr("Ely.by authorization succeeded."));
}

void ElyStep::fail(AccountTaskState state, const QString& message)
{
    m_pollTimer.stop();
    m_data->errorString = message;
    emit finished(state, message);
}

void ElyStep::abort()
{
    m_pollTimer.stop();
    if (m_reply) {
        auto* reply = m_reply;
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}
