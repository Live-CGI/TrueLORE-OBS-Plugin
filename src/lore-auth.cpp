#include "lore-auth.h"

#include <obs-module.h>

#include <QTcpServer>
#include <QTcpSocket>
#include <QDesktopServices>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

extern "C" {
#include "taffylabs_settings.h"
}

namespace lore_auth {

static QString randomStateToken()
{
	quint64 a = QRandomGenerator::global()->generate64();
	quint64 b = QRandomGenerator::global()->generate64();
	return QString::number(a, 16) + QString::number(b, 16);
}

AuthManager &AuthManager::instance()
{
	static AuthManager s_instance;
	return s_instance;
}

AuthManager::AuthManager(QObject *parent)
	: QObject(parent)
{
}

AuthManager::~AuthManager()
{
	stopCallbackServer();
}

QString AuthManager::accessToken() const
{
	const char *token = taffylabs_get_bearer_token();
	return token ? QString::fromUtf8(token) : QString();
}

bool AuthManager::isAuthenticated() const
{
	return !accessToken().trimmed().isEmpty();
}

void AuthManager::clearToken()
{
	taffylabs_set_bearer_token("");
	taffylabs_save_settings();
}

QString AuthManager::authenticate(const QString &apiBaseUrl, bool forceLogin)
{
	apiBaseUrl_ = apiBaseUrl.trimmed();

	if (apiBaseUrl_.endsWith('/'))
		apiBaseUrl_.chop(1);

	if (!forceLogin) {
		QString token = accessToken();
		if (!token.isEmpty())
			return token;
	}

	clearToken();

	if (loginInProgress_) {
		blog(LOG_INFO, "[TaffyLabs] auth already in progress");
		return QString();
	}

	if (!startCallbackServer()) {
		emit authenticationFailed("Failed to start localhost callback server");
		return QString();
	}

	loginInProgress_ = true;
	emit authenticationStarted();

	openLoginBrowser();
	return QString();
}

bool AuthManager::startCallbackServer()
{
	stopCallbackServer();

	server_ = new QTcpServer(this);

	if (!QObject::connect(server_, &QTcpServer::newConnection,
			      this, &AuthManager::onNewConnection)) {
		delete server_;
		server_ = nullptr;
		return false;
	}

	if (!server_->listen(QHostAddress::LocalHost, 0)) {
		blog(LOG_WARNING, "[TaffyLabs] failed to listen on localhost: %s",
		     server_->errorString().toUtf8().constData());
		delete server_;
		server_ = nullptr;
		return false;
	}

	callbackPort_ = static_cast<int>(server_->serverPort());
	pendingState_ = randomStateToken();

	blog(LOG_INFO, "[TaffyLabs] auth callback server listening on 127.0.0.1:%d",
	     callbackPort_);

	return true;
}

void AuthManager::stopCallbackServer()
{
	if (server_) {
		server_->close();
		server_->deleteLater();
		server_ = nullptr;
	}

	callbackPort_ = 0;
	pendingState_.clear();
	loginInProgress_ = false;
}

QString AuthManager::buildLoginUrl() const
{
	QUrl url(apiBaseUrl_ + "/auth/google");

	QUrl callbackUrl;
	callbackUrl.setScheme("http");
	callbackUrl.setHost("127.0.0.1");
	callbackUrl.setPort(callbackPort_);
	callbackUrl.setPath("/auth/callback");

	QUrlQuery query;
	query.addQueryItem("redirect_uri", callbackUrl.toString());
	query.addQueryItem("state", pendingState_);

	url.setQuery(query);
	return url.toString();
}

void AuthManager::openLoginBrowser()
{
	QString loginUrl = buildLoginUrl();

	blog(LOG_INFO, "[TaffyLabs] opening auth URL: %s",
	     loginUrl.toUtf8().constData());

	bool ok = QDesktopServices::openUrl(QUrl(loginUrl));
	if (!ok) {
		emit authenticationFailed("Failed to open browser");
		stopCallbackServer();
	}
}

void AuthManager::onNewConnection()
{
	if (!server_)
		return;

	while (server_->hasPendingConnections()) {
		QTcpSocket *socket = server_->nextPendingConnection();
		if (!socket)
			continue;

		if (!socket->waitForReadyRead(5000)) {
			socket->disconnectFromHost();
			socket->deleteLater();
			continue;
		}

		QByteArray raw = socket->readAll();
		QString requestText = QString::fromUtf8(raw);

		handleCallbackRequest(socket, requestText);

		socket->disconnectFromHost();
		socket->deleteLater();

		stopCallbackServer();
	}
}

void AuthManager::handleCallbackRequest(QTcpSocket *socket, const QString &requestText)
{
	QRegularExpression re("^GET\\s+([^\\s]+)\\s+HTTP/",
			      QRegularExpression::MultilineOption);
	QRegularExpressionMatch match = re.match(requestText);

	if (!match.hasMatch()) {
		emit authenticationFailed("Malformed callback request");
		return;
	}

	QString target = match.captured(1);
	QUrl url("http://127.0.0.1" + target);
	QUrlQuery query(url);

	QString state = query.queryItemValue("state");
	QString token = query.queryItemValue("accessToken");
	QString error = query.queryItemValue("error");

	if (!error.isEmpty()) {
		if (socket) {
			sendHttpResponse(
				socket,
				"<html><body><h3>Login failed</h3><p>You can close this window.</p></body></html>");
		}
		emit authenticationFailed(error);
		return;
	}

	if (!pendingState_.isEmpty() && state != pendingState_) {
		if (socket) {
			sendHttpResponse(
				socket,
				"<html><body><h3>Invalid state</h3><p>You can close this window.</p></body></html>");
		}
		emit authenticationFailed("State mismatch in auth callback");
		return;
	}

	if (token.isEmpty()) {
		if (socket) {
			sendHttpResponse(
				socket,
				"<html><body><h3>No token received</h3><p>You can close this window.</p></body></html>");
		}
		emit authenticationFailed("Callback did not include accessToken");
		return;
	}

	taffylabs_set_bearer_token(token.toUtf8().constData());
	taffylabs_save_settings();

	blog(LOG_INFO, "[TaffyLabs] authentication succeeded; token stored");

	if (socket) {
		sendHttpResponse(
			socket,
			"<html><body><h3>Login successful</h3><p>You can close this window and return to OBS.</p></body></html>");
	}

	emit authenticationSucceeded(token);
}

void AuthManager::sendHttpResponse(QTcpSocket *socket, const QString &html)
{
	if (!socket)
		return;

	QByteArray body = html.toUtf8();
	QByteArray response;
	response += "HTTP/1.1 200 OK\r\n";
	response += "Content-Type: text/html; charset=utf-8\r\n";
	response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
	response += "Connection: close\r\n";
	response += "\r\n";
	response += body;

	socket->write(response);
	socket->waitForBytesWritten(3000);
}

} // namespace lore_auth