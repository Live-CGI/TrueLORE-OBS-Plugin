#include <string>
#include <cstdlib>

#include <obs-frontend-api.h>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMetaObject>
#include <QMessageBox>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QStatusBar>
#include <QStringList>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#ifdef _WIN32
    #include <windows.h>
    #define SLEEP_MS(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

extern "C" {
#include "plugin-support.h"
#include "taffylabs_settings.h"
#include "lore-auth.h"
#include "lore-api.h"
#include "lore-deviceInfo.h"

}

const char* version = "0.1.1";
static QAction *g_actionSettings = nullptr;
static QPointer<QDialog> g_settingsDialog = nullptr;
static QLabel *g_idLabel = nullptr;

struct BroadcastDropdownContext {
	QPointer<QComboBox> combo;
	QPointer<QPushButton> loginButton;
	QPointer<QWidget> owner;
};

struct CreateBroadcastContext {
	QPointer<QComboBox> broadcastsDropdown;
	QPointer<QWidget> owner;
};
extern "C" void  taffylabs_show_message(const char *title, const char *message)
{
	blog(LOG_INFO, "[TaffyLabs] %s: %s", title, message);
    QMessageBox::information(
        nullptr	,
        title,
        message
    );
}

extern "C" void  taffylabs_show_warning(const char *title, const char *message)
{
	blog(LOG_WARNING, "[TaffyLabs] %s: %s", title, message);
    QMessageBox::warning(
        nullptr,
        title,
        message
    );
}

extern "C" void  taffylabs_show_error(const char *title, const char *message)
{
	blog(LOG_ERROR, "[TaffyLabs] %s: %s", title, message);
	QMessageBox::critical(
		nullptr,
		title,
		message
	);
}
static void taffylabs_add_status_indicator()
{
	void *main_window_ptr = obs_frontend_get_main_window();
	if (!main_window_ptr)
		return;

	QMainWindow *mainWindow = static_cast<QMainWindow *>(main_window_ptr);
	if (!mainWindow)
		return;

	// Make sure UI work happens on the Qt UI thread
	QMetaObject::invokeMethod(mainWindow, [mainWindow]() {
		if (g_idLabel)
			return; // already added

		QStatusBar *statusBar = mainWindow->statusBar();
		if (!statusBar)
			return;

		g_idLabel = new QLabel(QStringLiteral("ID: ABC-123"), statusBar);
		g_idLabel->setObjectName("taffylabsStatusIdLabel");
		g_idLabel->setToolTip(QStringLiteral("TaffyLabs broadcast ID"));

		// Appends on the permanent/right side
		statusBar->addPermanentWidget(g_idLabel);
	});
}

void open_browser(const std::string& url)
{
#ifdef _WIN32
    // Use ShellExecute (no console window)
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

#elif __APPLE__
    // macOS: fork + execl avoids spawning a visible shell
    pid_t pid = fork();
    if (pid == 0) {
        execl("/usr/bin/open", "open", url.c_str(), (char*)nullptr);
        _exit(1);
    }

#else
    // Linux: fork + execl avoids shell (no terminal popup)
    pid_t pid = fork();
    if (pid == 0) {
        execl("/usr/bin/xdg-open", "xdg-open", url.c_str(), (char*)nullptr);
        _exit(1);
    }

#endif
}

static void taffylabs_update_status_indicator(const char *idText)
{
	void *main_window_ptr = obs_frontend_get_main_window();
	if (!main_window_ptr)
		return;

	QMainWindow *mainWindow = static_cast<QMainWindow *>(main_window_ptr);
	if (!mainWindow)
		return;

	QString text = QString("%1").arg(idText ? idText : "");

	QMetaObject::invokeMethod(mainWindow, [text]() {
		if (g_idLabel)
			g_idLabel->setText(text);
	});
}


static void populate_broadcast_dropdown_from_json(QComboBox *combo, const QByteArray &jsonBytes)
{
	if (!combo)
		return;

	taffylabs_set_broadcast_array(jsonBytes.constData());
	
	combo->clear();
	const char* selBroadCast = taffylabs_get_selected_broadcast_object();
	QJsonParseError parseError, parseError2;
	QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseError);
	QJsonDocument selectedDoc = QJsonDocument::fromJson(QByteArray(selBroadCast), &parseError2);
	QString selId = "NONE SELECTED";
	if(parseError2.error == QJsonParseError::NoError){
    	QJsonObject selectedObj = selectedDoc.object();
		selId = selectedObj.value("id").toString();
	}

	if (parseError.error != QJsonParseError::NoError) {
		blog(LOG_WARNING,
		     "[TaffyLabs] Failed to parse broadcasts JSON: %s",
		     parseError.errorString().toUtf8().constData());
		return;
	}

	if (!doc.isArray()) {
		blog(LOG_WARNING, "[TaffyLabs] broadcasts JSON was not an array");
		return;
	}
	int iSelectedIndex = -1;
	
	
	QJsonArray arr = doc.array();
	QJsonDocument sbDoc = QJsonDocument::fromJson(QByteArray(selBroadCast), &parseError2);
	QJsonObject qoBroadcast = QJsonObject();
	if (parseError2.error == QJsonParseError::NoError) {
		qoBroadcast = sbDoc.object();
	}	

	for (const QJsonValue value : arr) {
		if (!value.isObject())
			continue;
		iSelectedIndex++;
		QJsonObject obj = value.toObject();
		QString skey = obj.value("stream_key").toString();
		QJsonObject metadata = obj.value("metadata").toObject();
		QString title = metadata.value("title").toString();
		QString id = obj.value("id").toString();

		if (title.isEmpty())
			title = "(Untitled Broadcast)";

		// Show title, store id as hidden item data
		combo->addItem(title, id);
		if(id == selId){
			combo->setCurrentIndex(iSelectedIndex);
		}
	}
}

static void lore_broadcasts_dropdown_callback(const lore_api_result *result, void *user_data)
{
	BroadcastDropdownContext *ctx =
		static_cast<BroadcastDropdownContext *>(user_data);
	
	if (!ctx){
		blog(LOG_WARNING, "[TaffyLabs] lore_broadcasts_dropdown_callback context=%p", user_data);
		return;
	}
	if(result->ok != 1){
		blog(LOG_WARNING, "[TaffyLabs] lore_broadcasts_dropdown_callback called with status=%s", result->body_text);		 		
	}

	QPointer<QComboBox> combo = ctx->combo;
	QPointer<QWidget> owner = ctx->owner;
	QPointer<QPushButton> loginButton = ctx->loginButton;

	// We no longer need the heap context after this function.
	// Capture what we need first.
	delete ctx;

	if (!owner || !combo)
		return;

	if (!result) {
		QMetaObject::invokeMethod(owner, [combo, loginButton]() {
			if (!combo)
				return;
			combo->clear();
			combo->addItem("Failed to load broadcasts");
			if (loginButton)
				loginButton->setText("Login");
		}, Qt::QueuedConnection);
		return;
	}

	if (!result->ok || !result->body_text) {
		const QString errorText =
			result->error_text ? QString::fromUtf8(result->error_text)
			                   : QString("Unknown error");

		blog(LOG_WARNING,
		     "[TaffyLabs] lore_broadcastsGet failed: status=%ld error=%s",
		     result->status_code,
		     errorText.toUtf8().constData());

		QMetaObject::invokeMethod(owner, [combo, errorText, loginButton]() {
			if (!combo)
				return;
			combo->clear();
			combo->addItem("Failed to load broadcasts");
			if (loginButton)
				loginButton->setText("Login");
		}, Qt::QueuedConnection);

		return;
	}

	const QByteArray body = QByteArray(result->body_text);
	
	QMetaObject::invokeMethod(owner, [combo, body, loginButton]() {
		if (!combo)
			return;

		populate_broadcast_dropdown_from_json(combo, body);
		if (loginButton)
			loginButton->setText("Logout");
		if (combo->count() == 0)
			combo->addItem("No broadcasts found");
	}, Qt::QueuedConnection);
}

static void load_broadcasts_into_dropdown(QComboBox *combo, QPushButton *loginButton, QWidget *owner)
{
	if (!combo || !owner)
		return;

	combo->clear();
	combo->addItem("Loading broadcasts...");

	const char *bearer = taffylabs_get_bearer_token();
	const char *keyId = nullptr;
    

	BroadcastDropdownContext *ctx = new BroadcastDropdownContext{
		combo,
		loginButton,
		owner
	};

	lore_api_result immediate =
		lore_broadcastsGet(
			bearer,
			keyId,
			lore_broadcasts_dropdown_callback,
			ctx);

	// If your API wrapper reports an immediate failure synchronously,
	// handle that here.
	if (!immediate.ok) {
		blog(LOG_ERROR,
		     "[TaffyLabs] lore_broadcastsGet immediate failure: status=%ld error=%s",
		     immediate.status_code,
		     immediate.error_text ? immediate.error_text : "(none)");

		combo->clear();
		combo->addItem("Failed to load broadcasts");
		delete ctx;
	}

	
}

struct LoginContext {
    QPointer<QWidget> owner;
    QPointer<QPushButton> loginButton;
    QPointer<QComboBox> broadcastsDropdown;
    std::string hexId;
    std::string authUrl;
    std::string authState;
    std::string authCode;
    std::string accessToken;
    int countDown = 90;
    bool abort = false;
};



static void on_device_login_tokenReceived(const lore_api_result *result, void *user_data)
{
    LoginContext *ctx = static_cast<LoginContext*>(user_data);
    if (!ctx) {
        blog(LOG_WARNING, "[TaffyLabs] on_device_login_tokenReceived called with null context");
        return;
    }

    if (result->ok == 1) {
        QJsonDocument doc = QJsonDocument::fromJson(
            QByteArray(result->body_text ? result->body_text : ""));
        QJsonObject obj = doc.object();
        std::string accessToken = obj.value("accessToken").toString().toStdString();

        taffylabs_set_bearer_token(accessToken.c_str());

        QPointer<QComboBox> broadcastsDropdown = ctx->broadcastsDropdown;
        QPointer<QPushButton> loginButton = ctx->loginButton;
        QPointer<QWidget> owner = ctx->owner;

        QMetaObject::invokeMethod(
            qApp,
            [broadcastsDropdown, loginButton, owner, accessToken]() {
                load_broadcasts_into_dropdown(broadcastsDropdown, loginButton, owner);

                if (loginButton) {
                    loginButton->setText("Authentication Succeeded!");
                    loginButton->setEnabled(true);

                    QTimer::singleShot(3000, loginButton, [loginButton]() {
                        if (loginButton) {
                            loginButton->setText("Login");
                        }
                    });
                }
            },
            Qt::QueuedConnection);
        delete ctx;
    } else {
        blog(LOG_WARNING,
             "[TaffyLabs] Authentication exchange code failed: status=%ld error=%s",
             result->status_code,
             result->error_text ? result->error_text : "(none)");
        delete ctx;
    }
}

static void on_device_login_check(const lore_api_result *result, void *user_data)
{
	LoginContext *ctx = static_cast<LoginContext*>(user_data);
	if (!ctx) {
		blog(LOG_WARNING, "[TaffyLabs] on_device_login_check called with null context");
		return;
	}

	QPointer<QPushButton> loginButton = ctx->loginButton;

	if (ctx->countDown > 0 && ctx->abort == false) {
		int countDown = ctx->countDown;

		if (loginButton) {
			QMetaObject::invokeMethod(
				qApp,
				[loginButton, countDown]() {
					if (loginButton) {
						loginButton->setText(
							QString("Waiting for Authentication... (%1s)").arg(countDown));
					}
				},
				Qt::QueuedConnection);
		}

		if (result->ok == 1) {
			blog(LOG_INFO, "[TaffyLabs] Authentication Code Received!");

			QJsonDocument doc = QJsonDocument::fromJson(
				QByteArray(result->body_text ? result->body_text : ""));
			QJsonObject obj = doc.object();
			ctx->authCode = obj.value("code").toString().toStdString();

			std::string authCode = ctx->authCode;

			lore_deviceLoginCheckExchangeCode(
				authCode.c_str(),
				on_device_login_tokenReceived,
				ctx);

			// IMPORTANT:
			// Do NOT delete ctx here. Ownership passes to the next async callback.
			return;
		}
		else if (result->status_code == 401) {
			ctx->countDown--;

			// This sleep is on the background thread, which is okay if intentional.
			SLEEP_MS(1000);

			std::string hexId = ctx->hexId;
			std::string authState = ctx->authState;

			lore_deviceLoginCheck(
				hexId.c_str(),
				authState.c_str(),
				on_device_login_check,
				ctx);
			return;
		}
		else {
			blog(LOG_WARNING,
				"[TaffyLabs] Authentication check failed: status=%ld error=%s",
				result->status_code,
				result->error_text ? result->error_text : "(none)");

			if (loginButton) {
				QMetaObject::invokeMethod(
					qApp,
					[loginButton]() {
						if (!loginButton)
							return;

						loginButton->setText("Authentication Failed!");
						loginButton->setEnabled(true);

						QTimer::singleShot(3000, loginButton, [loginButton]() {
							if (loginButton)
								loginButton->setText("Login");
						});
					},
					Qt::QueuedConnection);
			}

			delete ctx;
			return;
		}
	}
	else {
		QString finalStatus = ctx->abort ? "Aborted by User" : "Timed Out";
		blog(LOG_WARNING, "[TaffyLabs] Authentication %s", finalStatus.toUtf8().constData());

		if (loginButton) {
			QMetaObject::invokeMethod(
				qApp,
				[loginButton, finalStatus]() {
					if (!loginButton)
						return;

					loginButton->setText("Authentication " + finalStatus + "!");
					loginButton->setEnabled(true);

					QTimer::singleShot(3000, loginButton, [loginButton]() {
						if (loginButton)
							loginButton->setText("Login");
					});
				},
				Qt::QueuedConnection);
		}

		delete ctx;
	}
}


static void on_device_login(const lore_api_result *result, void *user_data)
{
	LoginContext *ctx = static_cast<LoginContext*>(user_data);
	if (!ctx) {
		blog(LOG_WARNING, "[TaffyLabs] on_device_login called with null context");
		return;
	}

	// Copy out only what we need from ctx before any async/UI handoff.
	QPointer<QPushButton> loginButton = ctx->loginButton;

	if (!result || !result->ok) {
		if (loginButton) {
			QMetaObject::invokeMethod(
				qApp,
				[loginButton]() {
					if (!loginButton)
						return;

					loginButton->setText("Authentication Failed!");
					loginButton->setEnabled(true);

					QTimer::singleShot(3000, loginButton, [loginButton]() {
						if (loginButton)
							loginButton->setText("Login");
					});
				},
				Qt::QueuedConnection);
		}

		delete ctx;
		return;
	}

	QJsonDocument doc = QJsonDocument::fromJson(
		QByteArray(result->body_text ? result->body_text : ""));
	QJsonObject obj = doc.object();

	ctx->authUrl = obj.value("url").toString().toStdString();
	ctx->authState = obj.value("state").toString().toStdString();

	std::string authUrl = ctx->authUrl;
	std::string authState = ctx->authState;
	std::string hexId = ctx->hexId;

	if (loginButton) {
		QMetaObject::invokeMethod(
			qApp,
			[loginButton]() {
				if (loginButton)
					loginButton->setText("Waiting for Authentication...");
			},
			Qt::QueuedConnection);
	}

	open_browser(authUrl.c_str());

	// next async step
	ctx->countDown = 90; // 90 second timeout for user to complete auth
	lore_api_result started = lore_deviceLoginCheck(
		hexId.c_str(),
		authState.c_str(),
		on_device_login_check,
		ctx);

	if (!started.ok) {
		if (loginButton) {
			QMetaObject::invokeMethod(
				qApp,
				[loginButton]() {
					if (!loginButton)
						return;

					loginButton->setText("Authentication Failed!");
					loginButton->setEnabled(true);

					QTimer::singleShot(3000, loginButton, [loginButton]() {
						if (loginButton)
							loginButton->setText("Login");
					});
				},
				Qt::QueuedConnection);
		}

		blog(LOG_WARNING, "[TaffyLabs] Failed to start device login check");
		delete ctx;
	}
}

static void open_taffylabs_settings_dialog()
{
	if (g_settingsDialog) {
		g_settingsDialog->show();
		g_settingsDialog->raise();
		g_settingsDialog->activateWindow();
		return;
	}

	QWidget *parent =
		reinterpret_cast<QWidget *>(obs_frontend_get_main_window());

	QDialog *dlg = new QDialog(parent);
	dlg->setAttribute(Qt::WA_DeleteOnClose, true);
	dlg->setWindowTitle("TaffyLabs Lore Settings v" + QString::fromUtf8(PLUGIN_VERSION));
	dlg->resize(500, 520);

	QObject::connect(dlg, &QDialog::finished,
    [](int result) {
        if (result == QDialog::Accepted) {
            blog(LOG_INFO, "[TaffyLabs] Dialog accepted");
        } else {

            blog(LOG_INFO, "[TaffyLabs] Dialog rejected/closed");
        }
    });

	QVBoxLayout *rootLayout = new QVBoxLayout(dlg);

	// Enable Lore Stream (top)
	QCheckBox *enableLoreStreamCheck =
		new QCheckBox("Enable Lore Stream", dlg);
	enableLoreStreamCheck->setChecked(taffylabs_get_enabled());
	rootLayout->addWidget(enableLoreStreamCheck);

	// Login button
	QPushButton *loginButton = new QPushButton("Please Wait...", dlg);
	rootLayout->addWidget(loginButton);

	// Manual bearer token + dropdown
	QFormLayout *authFormLayout = new QFormLayout();

	//QLineEdit *bearerTokenEdit = new QLineEdit(dlg);
	//bearerTokenEdit->setText(taffylabs_get_bearer_token());
	//authFormLayout->addRow("Manual Bearer Token", bearerTokenEdit);

	QComboBox *broadcastsDropdown = new QComboBox(dlg);
	authFormLayout->addRow("Available Broadcasts", broadcastsDropdown);

	rootLayout->addLayout(authFormLayout);

	// Add New Broadcast button directly under dropdown, outside groupbox
	QHBoxLayout *addNewLayout = new QHBoxLayout();
	addNewLayout->addStretch();

	QCheckBox *enableAutoNewBroadcast =
		new QCheckBox("Auto Generate New Broadcasts", dlg);
	enableAutoNewBroadcast->setChecked(taffylabs_get_autoBroadcast());
	rootLayout->addWidget(enableAutoNewBroadcast);

	QPushButton *addNewMetadataButton =
		new QPushButton("Add New Broadcast", dlg);
	addNewLayout->addWidget(addNewMetadataButton);

	rootLayout->addLayout(addNewLayout);

	// Broadcast Metadata group
	QGroupBox *metadataGroup = new QGroupBox("Broadcast Metadata", dlg);
	QVBoxLayout *metadataLayout = new QVBoxLayout(metadataGroup);

	QFormLayout *formLayout = new QFormLayout();

	QLineEdit *titleEdit = new QLineEdit(metadataGroup);
	titleEdit->setText(taffylabs_get_title());

	QTextEdit *descriptionEdit = new QTextEdit(metadataGroup);
	descriptionEdit->setPlainText(taffylabs_get_description());
	descriptionEdit->setFixedHeight(120);

	formLayout->addRow("Title", titleEdit);
	formLayout->addRow("Description", descriptionEdit);

	metadataLayout->addLayout(formLayout);

	// Metadata action buttons
	QHBoxLayout *metadataButtonLayout = new QHBoxLayout();
	metadataButtonLayout->addStretch();

	QPushButton *updateMetadataButton =
		new QPushButton("Update", metadataGroup);

	metadataButtonLayout->addWidget(updateMetadataButton);
	metadataLayout->addLayout(metadataButtonLayout);

	rootLayout->addWidget(metadataGroup);

	// OK button row
	QHBoxLayout *buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();

	QPushButton *okButton = new QPushButton("OK", dlg);
	buttonLayout->addWidget(okButton);

	rootLayout->addLayout(buttonLayout);
	
	QObject::connect(loginButton, &QPushButton::clicked,
    	[loginButton, broadcastsDropdown, dlg]() {
		if(loginButton->text().contains("Logout")){
			taffylabs_clear_settings();
			loginButton->setText("Login");
			load_broadcasts_into_dropdown(broadcastsDropdown, loginButton, dlg);
			return;
		}
        loginButton->setEnabled(false);
        loginButton->setText("Generating Authentication URL...");
        const char *hexID = random_hex_32();
        LoginContext *ctx = new LoginContext();
		ctx->owner = dlg;
        ctx->loginButton = loginButton;
        ctx->broadcastsDropdown = broadcastsDropdown;
        ctx->hexId = std::string(hexID);
        lore_api_result started = lore_deviceLogin(
            hexID,
            on_device_login,
            ctx);

        if (!started.ok) {
            loginButton->setText("Authentication Failed To Start!");
            loginButton->setEnabled(true);
            delete ctx;
        }
    });

	// Broadcast dropdown selection changed
	QObject::connect(
		broadcastsDropdown,
		QOverload<int>::of(&QComboBox::currentIndexChanged),
		[broadcastsDropdown, titleEdit, descriptionEdit](int iSelected) {
			QJsonDocument doc = QJsonDocument::fromJson(
				QByteArray(taffylabs_get_broadcast_array()));
			if (!doc.isArray()) {
				blog(LOG_WARNING,
				     "[TaffyLabs] Broadcast array JSON was not an array");
				return;
			}

			QJsonArray arr = doc.array();
			if (iSelected < 0 || iSelected >= arr.size()) {
				blog(LOG_WARNING,
				     "[TaffyLabs] Broadcast dropdown index out of range: %d",
				     iSelected);
				return;
			}

			QJsonObject obj = arr.at(iSelected).toObject();
			QJsonObject metadata = obj.value("metadata").toObject();

			taffylabs_set_selected_broadcast_object(
				QJsonDocument(obj).toJson(QJsonDocument::Compact).constData());
			taffylabs_set_streamKey(
				obj.value("stream_key").toString().toUtf8().constData());

			titleEdit->setText(metadata.value("title").toString());
			descriptionEdit->setPlainText(
				metadata.value("description").toString());
		});

	// Update metadata callback
	QObject::connect(updateMetadataButton, &QPushButton::clicked,
		[titleEdit, descriptionEdit, broadcastsDropdown]() {
			int iSelected = broadcastsDropdown->currentIndex();
			blog(LOG_INFO,
			     "[TaffyLabs] Update metadata clicked for index %d",
			     iSelected);

			taffylabs_set_title(titleEdit->text().toUtf8().constData());
			taffylabs_set_description(
				descriptionEdit->toPlainText().toUtf8().constData());

			// Put your API/update logic here
			blog(LOG_INFO,
			     "[TaffyLabs] Updating broadcast metadata: title='%s'",
			     titleEdit->text().toUtf8().constData());
		});

	// Add new broadcast callback
	QObject::connect(addNewMetadataButton, &QPushButton::clicked,
			[titleEdit, descriptionEdit, broadcastsDropdown, loginButton, dlg]() {
			blog(LOG_INFO, "[TaffyLabs] Add New clicked");

		QJsonObject metadata;
		metadata["title"] = titleEdit->text();
		metadata["description"] = descriptionEdit->toPlainText();
		//metadata["deviceInfo"] = taffylabs_get_unique_device_id_json();
		const char *deviceJson = taffylabs_get_unique_device_id_json();

		QJsonParseError err;
		auto deviceDoc = QJsonDocument::fromJson(QByteArray(deviceJson), &err);

		metadata["deviceInfo"] =
			(err.error == QJsonParseError::NoError && deviceDoc.isObject())
				? QJsonValue(deviceDoc.object())
				: QJsonValue(QJsonValue::Null);

		QJsonObject payload;
		payload["metadata"] = metadata;

		QJsonDocument doc(payload);
		QByteArray jsonBytes = doc.toJson(QJsonDocument::Compact);

		blog(LOG_INFO,
		     "[TaffyLabs] Creating new broadcast: metadata='%s'",
		     jsonBytes.constData());

		CreateBroadcastContext *cbCtx = new CreateBroadcastContext();
		cbCtx->broadcastsDropdown = broadcastsDropdown;
		cbCtx->owner = dlg;

		lore_api_result rNew = lore_broadcastsCreate(
			taffylabs_get_bearer_token(),
			jsonBytes.constData(),
			[](const lore_api_result *result, void *user_data) {
				CreateBroadcastContext *ctx =
					static_cast<CreateBroadcastContext *>(user_data);

				if (!result) {
					taffylabs_show_error(
						"Error",
						"No response from server when creating broadcast");
					delete ctx;
					return;
				}

				if (!result->ok) {
					blog(LOG_INFO,
					     "[TaffyLabs] Failed to create broadcast: status=%ld error=%s",
					     result->status_code,
					     result->error_text ? result->error_text : "(none)");

					taffylabs_show_error(
						"Error",
						QString("Failed to create broadcast: %1")
							.arg(result->error_text ? result->error_text
							                        : "Unknown error")
							.toUtf8()
							.constData());

					delete ctx;
					return;
				}

				blog(LOG_INFO,
				     "[TaffyLabs] Broadcast created successfully: %s",
				     result->body_text ? result->body_text : "(no body)");

				taffylabs_set_selected_broadcast_object(result->body_text);
				if (ctx->broadcastsDropdown && ctx->owner) {
					QMetaObject::invokeMethod(
						ctx->owner,
						[combo = ctx->broadcastsDropdown,
						 owner = ctx->owner]() {
							if (combo && owner)
								load_broadcasts_into_dropdown(combo, nullptr, owner);
						},
						Qt::QueuedConnection);
				}

				delete ctx;
			},
			cbCtx);

		if (!rNew.ok) {
			blog(LOG_WARNING,
			     "[TaffyLabs] Failed to start create broadcast request: %s",
			     rNew.error_text ? rNew.error_text : "(unknown)");
			taffylabs_show_error(
				"Error",
				rNew.error_text ? rNew.error_text
				                : "Failed to start create broadcast request");
			delete cbCtx;
		}
	});
	
	// Save and close
	QObject::connect(okButton, &QPushButton::clicked,
		[dlg,
		 enableLoreStreamCheck,
		 enableAutoNewBroadcast,
		 broadcastsDropdown,
		 titleEdit,
		 descriptionEdit]() {
			taffylabs_set_enabled(enableLoreStreamCheck->isChecked());
			taffylabs_set_autoBroadcast(enableAutoNewBroadcast->isChecked());

			blog(LOG_INFO,
			     "[TaffyLabs] Selected dropdown value: %s",
			     broadcastsDropdown->currentText().toUtf8().constData());

			taffylabs_set_title(titleEdit->text().toUtf8().constData());
			taffylabs_set_description(
				descriptionEdit->toPlainText().toUtf8().constData());

			std::string mnuUpdate =
				std::string("TaffyLabs Broadcast: ") + taffylabs_get_title();
			taffylabs_update_status_indicator(mnuUpdate.c_str());
			taffylabs_save_settings();

			blog(LOG_INFO,
			     "[TaffyLabs] settings saved: enabled=%s",
			     enableLoreStreamCheck->isChecked() ? "true" : "false");

			dlg->accept();
		});

	QObject::connect(dlg, &QObject::destroyed, []() {
		g_settingsDialog = nullptr;
	});

	g_settingsDialog = dlg;
	
	dlg->show();
	dlg->raise();
	dlg->activateWindow();
	
	load_broadcasts_into_dropdown(broadcastsDropdown, loginButton, dlg);		

}




static void taffylabs_remove_status_indicator()
{
	void *main_window_ptr = obs_frontend_get_main_window();
	if (!main_window_ptr)
		return;

	QMainWindow *mainWindow = static_cast<QMainWindow *>(main_window_ptr);
	if (!mainWindow)
		return;

	QMetaObject::invokeMethod(mainWindow, [mainWindow]() {
		if (!g_idLabel)
			return;

		QStatusBar *statusBar = mainWindow->statusBar();
		if (statusBar)
			statusBar->removeWidget(g_idLabel);

		delete g_idLabel;
		g_idLabel = nullptr;
	});
}

extern "C" void taffylabs_ui_init()
{
	taffylabs_add_status_indicator();
	taffylabs_update_status_indicator("TaffyLabs Lore Initializing!");
	taffylabs_load_settings();
	if(lore_authConfirm(taffylabs_get_bearer_token())){
		blog(LOG_INFO, "[TaffyLabs] Authentication confirmed successfully on UI init.");		
	}else{
		taffylabs_clear_settings();
		taffylabs_update_status_indicator("Please Authenticate Taffylabs Lore!");
	}

	const char *selBroadCast = taffylabs_get_selected_broadcast_object();
	if (!selBroadCast || !*selBroadCast) {
		taffylabs_update_status_indicator("Please check your TaffyLabs Settings!");
	} else {
		QJsonParseError parseError;
		QJsonDocument sbDoc =
			QJsonDocument::fromJson(QByteArray(selBroadCast), &parseError);

		if (parseError.error != QJsonParseError::NoError || !sbDoc.isObject()) {
			taffylabs_update_status_indicator("Please check your TaffyLabs Settings!");
		} else {
			QJsonObject rootObj = sbDoc.object();
			QJsonObject metadataObj = rootObj.value("metadata").toObject();
			QString title = QStringLiteral("TaffyLabs Broadcast: ") + metadataObj.value("title").toString();

			if (title.isEmpty()) {
				taffylabs_update_status_indicator("Please check your TaffyLabs Settings!");
			} else {
				QByteArray titleUtf8 = title.toUtf8();
				taffylabs_update_status_indicator(titleUtf8.constData());
			}
		}
	}

	g_actionSettings = static_cast<QAction *>(
		obs_frontend_add_tools_menu_qaction((std::string("TaffyLabs Lore Settings")).c_str()));

	if (!g_actionSettings)
		return;

	QObject::connect(g_actionSettings, &QAction::triggered, []() {
		open_taffylabs_settings_dialog();
	});
	
}

extern "C" void taffylabs_ui_shutdown()
{
	if (g_settingsDialog) {
		g_settingsDialog->close();
		g_settingsDialog = nullptr;
	}

	if (g_actionSettings) {
		//g_actionSettings->disconnect();
		g_actionSettings = nullptr;
	}

	taffylabs_remove_status_indicator();
}
