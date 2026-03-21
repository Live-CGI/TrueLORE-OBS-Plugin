#pragma once

#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;

namespace lore_auth {

class AuthManager : public QObject
{
	Q_OBJECT

public:
	static AuthManager &instance();

	QString authenticate(const QString &apiBaseUrl, bool forceLogin = false);
	QString accessToken() const;
	bool isAuthenticated() const;
	void clearToken();

signals:
	void authenticationStarted();
	void authenticationSucceeded(const QString &token);
	void authenticationFailed(const QString &reason);

private slots:
	void onNewConnection();

private:
	explicit AuthManager(QObject *parent = nullptr);
	~AuthManager() override;

	AuthManager(const AuthManager &) = delete;
	AuthManager &operator=(const AuthManager &) = delete;

	bool startCallbackServer();
	void stopCallbackServer();
	void openLoginBrowser();
	QString buildLoginUrl() const;

	void handleCallbackRequest(QTcpSocket *socket, const QString &requestText);
	void sendHttpResponse(QTcpSocket *socket, const QString &html);

private:
	QTcpServer *server_ = nullptr;
	QString apiBaseUrl_;
	QString pendingState_;
	int callbackPort_ = 0;
	bool loginInProgress_ = false;
};

} // namespace lore_auth
