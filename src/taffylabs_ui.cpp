#include <string>
#include <cstdlib>

#include <obs-frontend-api.h>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
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
#include <QStyle>
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
	blog(LOG_INFO, "[TrueLORE] %s: %s", title, message);
    QMessageBox::information(
        nullptr	,
        title,
        message
    );
}

extern "C" void  taffylabs_show_warning(const char *title, const char *message)
{
	blog(LOG_WARNING, "[TrueLORE] %s: %s", title, message);
    QMessageBox::warning(
        nullptr,
        title,
        message
    );
}

extern "C" void  taffylabs_show_error(const char *title, const char *message)
{
	blog(LOG_ERROR, "[TrueLORE] %s: %s", title, message);
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

extern "C" void taffylabs_update_status_indicator(const char *idText)
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

// Settings dialog look & feel. Colors and font sizes are derived from the active
// OBS theme so the dialog fits both dark and light themes.

enum class AuthUiState { Checking, SignedOut, Starting, Waiting, SignedIn, Failed };

static QString css_rgba(QColor color, qreal alpha = -1.0)
{
	if (alpha >= 0.0)
		color.setAlphaF(alpha);
	return QString("rgba(%1, %2, %3, %4)")
		.arg(color.red())
		.arg(color.green())
		.arg(color.blue())
		.arg(color.alpha());
}

static void repolish(QWidget *widget)
{
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
	widget->update();
}

// Fits the dialog's height to its content. sizeHint() overestimates word-wrapped
// labels, so ask the layout for the height at the current width instead.
static void fit_dialog_height(QWidget *dialog)
{
	QLayout *layout = dialog->layout();
	if (!layout)
		return;
	const int height = layout->hasHeightForWidth() ? layout->totalHeightForWidth(dialog->width())
						       : layout->totalSizeHint().height();
	dialog->resize(dialog->width(), height);
}

static QString settings_dialog_stylesheet(const QPalette &palette, qreal basePointSize)
{
	const QColor text = palette.color(QPalette::WindowText);
	const QColor accent = palette.color(QPalette::Highlight);
	const QColor accentText = palette.color(QPalette::HighlightedText);
	const QColor warning(210, 153, 34);

	QString qss = QStringLiteral(R"(
		QLabel#loreTitle { font-size: @titleSize@pt; font-weight: 600; }
		QLabel#loreSectionLabel { color: @muted@; font-size: @smallSize@pt; font-weight: 600; }
		QLabel[loreMuted="true"] { color: @muted@; }
		QFrame#loreCard { background-color: @base@; border: 1px solid @border@; border-radius: 8px; }
		QFrame#loreDivider { background-color: @border@; border: none; min-height: 1px; max-height: 1px; }
		QFrame#loreCallout { background-color: @warningBg@; border: 1px solid @warningBorder@; border-radius: 6px; }
		QLabel#loreAuthStatus { font-weight: 600; }
		QLabel#loreAuthDot {
			min-width: 10px; max-width: 10px; min-height: 10px; max-height: 10px;
			border-radius: 5px; background-color: @idle@;
		}
		QLabel#loreAuthDot[loreTone="ok"] { background-color: #2EA043; }
		QLabel#loreAuthDot[loreTone="busy"] { background-color: #D29922; }
		QLabel#loreAuthDot[loreTone="error"] { background-color: #DA3633; }
		QPushButton[loreRole="primary"] {
			background-color: @accent@; border-color: @accent@; color: @accentText@; font-weight: 600;
		}
		QPushButton[loreRole="primary"]:hover { background-color: @accentHover@; border-color: @accentHover@; }
		QPushButton[loreRole="primary"]:focus { border-color: @accentHover@; }
		QPushButton[loreRole="primary"]:pressed { background-color: @accentPressed@; border-color: @accentPressed@; }
		QPushButton[loreRole="primary"]:disabled {
			background-color: @accentDisabled@; border-color: transparent; color: @accentTextDisabled@;
		}
	)");

	const QHash<QString, QString> tokens = {
		{"@titleSize@", QString::number(basePointSize * 1.6, 'f', 1)},
		{"@smallSize@", QString::number(basePointSize * 0.85, 'f', 1)},
		{"@muted@", css_rgba(text, 0.62)},
		{"@idle@", css_rgba(text, 0.35)},
		{"@border@", css_rgba(text, 0.12)},
		{"@base@", css_rgba(palette.color(QPalette::Base))},
		{"@warningBg@", css_rgba(warning, 0.14)},
		{"@warningBorder@", css_rgba(warning, 0.5)},
		{"@accent@", css_rgba(accent)},
		{"@accentHover@", css_rgba(accent.lighter(115))},
		{"@accentPressed@", css_rgba(accent.darker(115))},
		{"@accentDisabled@", css_rgba(accent, 0.4)},
		{"@accentText@", css_rgba(accentText)},
		{"@accentTextDisabled@", css_rgba(accentText, 0.6)},
	};
	for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it)
		qss.replace(it.key(), it.value());

	return qss;
}

static QLabel *make_section_label(const QString &text, QWidget *parent)
{
	QLabel *label = new QLabel(text.toUpper(), parent);
	label->setObjectName("loreSectionLabel");
	QFont font = label->font();
	font.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
	label->setFont(font);
	return label;
}

static QFrame *make_card(QWidget *parent)
{
	QFrame *card = new QFrame(parent);
	card->setObjectName("loreCard");
	QVBoxLayout *layout = new QVBoxLayout(card);
	layout->setContentsMargins(16, 14, 16, 14);
	layout->setSpacing(6);
	return card;
}

static QLabel *make_hint(const QString &text, QWidget *parent, QCheckBox *alignWith = nullptr)
{
	QLabel *hint = new QLabel(text, parent);
	hint->setProperty("loreMuted", true);
	hint->setWordWrap(true);
	if (alignWith) {
		// Line the hint up with the checkbox text rather than its indicator.
		QStyle *style = alignWith->style();
		const int indent = style->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, alignWith) +
				   style->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, nullptr, alignWith);
		hint->setContentsMargins(indent, 0, 0, 0);
	}
	return hint;
}

// Updates the account card (status dot, status text, detail line) and the
// sign-in button together. Must be called on the Qt UI thread.
static void set_auth_state(QPushButton *button, AuthUiState state, const QString &detail = QString())
{
	if (!button)
		return;

	QString tone = "idle";
	QString statusText;
	QString detailText = detail;
	QString buttonText = "Sign In";
	bool buttonEnabled = true;
	bool primary = true;

	switch (state) {
	case AuthUiState::Checking:
		statusText = "Checking your account…";
		buttonEnabled = false;
		break;
	case AuthUiState::SignedOut:
		statusText = "Not signed in";
		if (detailText.isEmpty())
			detailText = "Sign in to choose a broadcast and stream to TrueLORE.";
		break;
	case AuthUiState::Starting:
		tone = "busy";
		statusText = "Starting sign-in…";
		buttonEnabled = false;
		break;
	case AuthUiState::Waiting:
		tone = "busy";
		statusText = "Waiting for browser sign-in";
		if (detailText.isEmpty())
			detailText = "Continue in your browser to finish signing in.";
		buttonText = "Waiting…";
		buttonEnabled = false;
		break;
	case AuthUiState::SignedIn:
		tone = "ok";
		statusText = "Signed in";
		buttonText = "Sign Out";
		primary = false;
		break;
	case AuthUiState::Failed:
		tone = "error";
		statusText = "Sign-in didn't complete";
		if (detailText.isEmpty())
			detailText = "Something went wrong. Please try again.";
		buttonText = "Try Again";
		break;
	}

	button->setProperty("loreAuthState", static_cast<int>(state));
	button->setProperty("loreRole", primary ? "primary" : "secondary");
	button->setText(buttonText);
	button->setEnabled(buttonEnabled);
	repolish(button);

	QWidget *dialog = button->window();
	if (QLabel *dot = dialog->findChild<QLabel *>("loreAuthDot")) {
		dot->setProperty("loreTone", tone);
		repolish(dot);
	}
	if (QLabel *status = dialog->findChild<QLabel *>("loreAuthStatus"))
		status->setText(statusText);
	if (QLabel *detailLabel = dialog->findChild<QLabel *>("loreAuthDetail")) {
		const bool wasShown = !detailLabel->isHidden();
		detailLabel->setText(detailText);
		detailLabel->setVisible(!detailText.isEmpty());
		if (wasShown != !detailLabel->isHidden() && dialog->isVisible())
			QTimer::singleShot(0, dialog, [dialog]() { fit_dialog_height(dialog); });
	}
}

static bool is_signed_in_state(QPushButton *button)
{
	return button &&
	       button->property("loreAuthState").toInt() == static_cast<int>(AuthUiState::SignedIn);
}

static void sortByUpdatedAt(QJsonArray& arr, const QString& sortBy)
{
    QVector<QJsonValue> vec;
    vec.reserve(arr.size());


    for (QJsonValue v : arr)
        vec.push_back(v);
    
    std::sort(vec.begin(), vec.end(),
        [sortBy](const QJsonValue& a, const QJsonValue& b)
        {
            QString aTime = a.toObject().value(sortBy).toString();
            QString bTime = b.toObject().value(sortBy).toString();

            return aTime > bTime;
        });

    arr = QJsonArray();
    for (const auto& v : vec)
        arr.append(v);
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
		     "[TrueLORE] Failed to parse broadcasts JSON: %s",
		     parseError.errorString().toUtf8().constData());
		return;
	}

	if (!doc.isArray()) {
		blog(LOG_WARNING, "[TrueLORE] broadcasts JSON was not an array");
		return;
	}
	int iSelectedIndex = -1;
	
	
	QJsonArray arr = doc.array();
	QJsonDocument sbDoc = QJsonDocument::fromJson(QByteArray(selBroadCast), &parseError2);
	QJsonObject qoBroadcast = QJsonObject();
	if (parseError2.error == QJsonParseError::NoError) {
		qoBroadcast = sbDoc.object();
	}	
	//sortByUpdatedAt(arr, "updatedAt");
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
		blog(LOG_WARNING, "[TrueLORE] lore_broadcasts_dropdown_callback context=%p", user_data);
		return;
	}
	if(result->ok != 1){
		blog(LOG_WARNING, "[TrueLORE] lore_broadcasts_dropdown_callback called with status=%s", result->body_text);		 		
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
			set_auth_state(loginButton, AuthUiState::SignedOut);
		}, Qt::QueuedConnection);
		return;
	}

	if (!result->ok || !result->body_text) {
		const QString errorText =
			result->error_text ? QString::fromUtf8(result->error_text)
			                   : QString("Unknown error");

		blog(LOG_WARNING,
		     "[TrueLORE] lore_broadcastsGet failed: status=%ld error=%s",
		     result->status_code,
		     errorText.toUtf8().constData());

		const bool unauthorized = result->status_code == 401 || result->status_code == 403;
		QMetaObject::invokeMethod(owner, [combo, errorText, loginButton, unauthorized]() {
			if (!combo)
				return;
			combo->clear();
			combo->addItem(unauthorized ? "Sign in to see your broadcasts"
						    : "Failed to load broadcasts");
			set_auth_state(loginButton, AuthUiState::SignedOut);
		}, Qt::QueuedConnection);

		return;
	}

	const QByteArray body = QByteArray(result->body_text);
	
	QMetaObject::invokeMethod(owner, [combo, body, loginButton]() {
		if (!combo)
			return;

		populate_broadcast_dropdown_from_json(combo, body);
		if (loginButton && !is_signed_in_state(loginButton))
			set_auth_state(loginButton, AuthUiState::SignedIn);
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
		     "[TrueLORE] lore_broadcastsGet immediate failure: status=%ld error=%s",
		     immediate.status_code,
		     immediate.error_text ? immediate.error_text : "(none)");

		combo->clear();
		combo->addItem("Failed to load broadcasts");
		set_auth_state(loginButton, AuthUiState::SignedOut);
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
        blog(LOG_WARNING, "[TrueLORE] on_device_login_tokenReceived called with null context");
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
                set_auth_state(loginButton, AuthUiState::SignedIn, "Signed in successfully.");
                load_broadcasts_into_dropdown(broadcastsDropdown, loginButton, owner);
            },
            Qt::QueuedConnection);
        delete ctx;
    } else {
        blog(LOG_WARNING,
             "[TrueLORE] Authentication exchange code failed: status=%ld error=%s",
             result->status_code,
             result->error_text ? result->error_text : "(none)");

        QPointer<QPushButton> loginButton = ctx->loginButton;
        if (loginButton) {
            QMetaObject::invokeMethod(
                qApp,
                [loginButton]() {
                    set_auth_state(loginButton, AuthUiState::Failed,
                                   "Couldn't finish signing in. Please try again.");
                },
                Qt::QueuedConnection);
        }

        delete ctx;
    }
}

static void on_device_login_check(const lore_api_result *result, void *user_data)
{
	LoginContext *ctx = static_cast<LoginContext*>(user_data);
	if (!ctx) {
		blog(LOG_WARNING, "[TrueLORE] on_device_login_check called with null context");
		return;
	}

	QPointer<QPushButton> loginButton = ctx->loginButton;

	if (ctx->countDown > 0 && ctx->abort == false) {
		int countDown = ctx->countDown;

		if (loginButton) {
			QMetaObject::invokeMethod(
				qApp,
				[loginButton, countDown]() {
					set_auth_state(loginButton, AuthUiState::Waiting,
						       QString("Continue in your browser to finish signing in (%1s left).")
							       .arg(countDown));
				},
				Qt::QueuedConnection);
		}

		if (result->ok == 1) {
			blog(LOG_INFO, "[TrueLORE] Authentication Code Received!");

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
				"[TrueLORE] Authentication check failed: status=%ld error=%s",
				result->status_code,
				result->error_text ? result->error_text : "(none)");

			if (loginButton) {
				QMetaObject::invokeMethod(
					qApp,
					[loginButton]() {
						set_auth_state(loginButton, AuthUiState::Failed);
					},
					Qt::QueuedConnection);
			}

			delete ctx;
			return;
		}
	}
	else {
		QString finalStatus = ctx->abort ? "Aborted by User" : "Timed Out";
		blog(LOG_WARNING, "[TrueLORE] Authentication %s", finalStatus.toUtf8().constData());

		const QString detail = ctx->abort ? "Sign-in was cancelled."
						  : "Sign-in timed out. Please try again.";
		if (loginButton) {
			QMetaObject::invokeMethod(
				qApp,
				[loginButton, detail]() {
					set_auth_state(loginButton, AuthUiState::Failed, detail);
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
		blog(LOG_WARNING, "[TrueLORE] on_device_login called with null context");
		return;
	}

	// Copy out only what we need from ctx before any async/UI handoff.
	QPointer<QPushButton> loginButton = ctx->loginButton;

	if (!result || !result->ok) {
		if (loginButton) {
			QMetaObject::invokeMethod(
				qApp,
				[loginButton]() {
					set_auth_state(loginButton, AuthUiState::Failed,
						       "Couldn't reach TrueLORE. Check your connection and try again.");
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
				set_auth_state(loginButton, AuthUiState::Waiting);
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
					set_auth_state(loginButton, AuthUiState::Failed);
				},
				Qt::QueuedConnection);
		}

		blog(LOG_WARNING, "[TrueLORE] Failed to start device login check");
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
	dlg->setWindowTitle("TrueLORE Settings");
	dlg->setMinimumWidth(500);

	// Polish first so the dialog picks up the OBS theme's font size and palette.
	dlg->ensurePolished();
	const qreal basePointSize = dlg->font().pointSizeF() > 0 ? dlg->font().pointSizeF() : 10.0;
	dlg->setStyleSheet(settings_dialog_stylesheet(dlg->palette(), basePointSize));

	QObject::connect(dlg, &QDialog::finished,
    [](int result) {
        if (result == QDialog::Accepted) {
            blog(LOG_INFO, "[TrueLORE] Dialog accepted");
        } else {

            blog(LOG_INFO, "[TrueLORE] Dialog rejected/closed");
        }
    });

	QVBoxLayout *rootLayout = new QVBoxLayout(dlg);
	rootLayout->setContentsMargins(20, 18, 20, 18);
	rootLayout->setSpacing(6);

	// Header
	QHBoxLayout *headerLayout = new QHBoxLayout();
	QVBoxLayout *headerTextLayout = new QVBoxLayout();
	headerTextLayout->setSpacing(0);

	QLabel *titleLabel = new QLabel("TrueLORE", dlg);
	titleLabel->setObjectName("loreTitle");
	QLabel *subtitleLabel = new QLabel("by Taffy Labs", dlg);
	subtitleLabel->setProperty("loreMuted", true);
	headerTextLayout->addWidget(titleLabel);
	headerTextLayout->addWidget(subtitleLabel);

	QLabel *versionLabel = new QLabel("v" + QString::fromUtf8(PLUGIN_VERSION), dlg);
	versionLabel->setProperty("loreMuted", true);

	headerLayout->addLayout(headerTextLayout);
	headerLayout->addStretch();
	headerLayout->addWidget(versionLabel, 0, Qt::AlignTop);
	rootLayout->addLayout(headerLayout);
	rootLayout->addSpacing(10);

	// Account
	rootLayout->addWidget(make_section_label("Account", dlg));
	QFrame *accountCard = make_card(dlg);

	QLabel *authDot = new QLabel(accountCard);
	authDot->setObjectName("loreAuthDot");
	QLabel *authStatusLabel = new QLabel(accountCard);
	authStatusLabel->setObjectName("loreAuthStatus");
	QLabel *authDetailLabel = make_hint(QString(), accountCard);
	authDetailLabel->setObjectName("loreAuthDetail");
	authDetailLabel->setContentsMargins(18, 0, 0, 0); // dot width + spacing

	QHBoxLayout *authStatusRow = new QHBoxLayout();
	authStatusRow->setSpacing(8);
	authStatusRow->addWidget(authDot, 0, Qt::AlignVCenter);
	authStatusRow->addWidget(authStatusLabel, 1);

	QVBoxLayout *authTextLayout = new QVBoxLayout();
	authTextLayout->setSpacing(2);
	authTextLayout->addStretch();
	authTextLayout->addLayout(authStatusRow);
	authTextLayout->addWidget(authDetailLabel);
	authTextLayout->addStretch();

	QPushButton *loginButton = new QPushButton(accountCard);
	loginButton->setAutoDefault(false);
	loginButton->setDefault(false);
	loginButton->setMinimumWidth(100);

	QHBoxLayout *accountLayout = new QHBoxLayout();
	accountLayout->setSpacing(12);
	accountLayout->addLayout(authTextLayout, 1);
	accountLayout->addWidget(loginButton, 0, Qt::AlignVCenter);
	static_cast<QVBoxLayout *>(accountCard->layout())->addLayout(accountLayout);
	rootLayout->addWidget(accountCard);
	rootLayout->addSpacing(10);

	// Streaming
	rootLayout->addWidget(make_section_label("Streaming", dlg));
	QFrame *streamingCard = make_card(dlg);
	QVBoxLayout *streamingLayout = static_cast<QVBoxLayout *>(streamingCard->layout());

	QCheckBox *enableLoreStreamCheck =
		new QCheckBox("Stream to TrueLORE when I go live", streamingCard);
	enableLoreStreamCheck->setChecked(taffylabs_get_enabled());
	streamingLayout->addWidget(enableLoreStreamCheck);
	streamingLayout->addWidget(make_hint(
		"Sends a copy of your stream to TrueLORE whenever you go live.",
		streamingCard, enableLoreStreamCheck));

	QFrame *streamingDivider = new QFrame(streamingCard);
	streamingDivider->setObjectName("loreDivider");
	streamingLayout->addSpacing(4);
	streamingLayout->addWidget(streamingDivider);
	streamingLayout->addSpacing(4);

	QCheckBox *enforce2kCheck =
		new QCheckBox("Keep keyframe interval at 2 seconds", streamingCard);
	enforce2kCheck->setChecked(taffylabs_get_enforce_2k_fps());
	streamingLayout->addWidget(enforce2kCheck);
	streamingLayout->addWidget(make_hint(
		"Applied when you go live if your encoder is set higher or to auto.",
		streamingCard, enforce2kCheck));

	QFrame *enforceWarningCallout = new QFrame(streamingCard);
	enforceWarningCallout->setObjectName("loreCallout");
	QVBoxLayout *calloutLayout = new QVBoxLayout(enforceWarningCallout);
	calloutLayout->setContentsMargins(12, 8, 12, 8);
	QLabel *enforceWarningLabel = new QLabel(
		"<b>Strongly recommended.</b> Most streaming platforms already require a keyframe "
		"at least every 2 seconds.",
		enforceWarningCallout);
	enforceWarningLabel->setWordWrap(true);
	calloutLayout->addWidget(enforceWarningLabel);
	enforceWarningCallout->setVisible(!taffylabs_get_enforce_2k_fps());
	streamingLayout->addSpacing(2);
	streamingLayout->addWidget(enforceWarningCallout);

	QObject::connect(enforce2kCheck, &QCheckBox::toggled, [enforceWarningCallout, dlg](bool checked) {
		enforceWarningCallout->setVisible(!checked);
		QTimer::singleShot(0, dlg, [dlg]() { fit_dialog_height(dlg); });
	});

	rootLayout->addWidget(streamingCard);
	rootLayout->addSpacing(10);

	// Broadcast
	rootLayout->addWidget(make_section_label("Broadcast", dlg));
	QFrame *broadcastCard = make_card(dlg);
	QVBoxLayout *broadcastLayout = static_cast<QVBoxLayout *>(broadcastCard->layout());

	QFormLayout *formLayout = new QFormLayout();
	formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
	formLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
	formLayout->setHorizontalSpacing(14);
	formLayout->setVerticalSpacing(8);

	QComboBox *broadcastsDropdown = new QComboBox(broadcastCard);

	QLineEdit *titleEdit = new QLineEdit(broadcastCard);
	titleEdit->setPlaceholderText("Broadcast title");
	titleEdit->setText(taffylabs_get_title());

	QTextEdit *descriptionEdit = new QTextEdit(broadcastCard);
	descriptionEdit->setPlaceholderText("What's this broadcast about?");
	descriptionEdit->setAcceptRichText(false);
	descriptionEdit->setPlainText(taffylabs_get_description());
	descriptionEdit->setFixedHeight(96);

	formLayout->addRow("Broadcast", broadcastsDropdown);
	formLayout->addRow("Title", titleEdit);
	formLayout->addRow("Description", descriptionEdit);
	if (QWidget *descriptionLabel = formLayout->labelForField(descriptionEdit))
		formLayout->setAlignment(descriptionLabel, Qt::AlignLeft | Qt::AlignTop);
	broadcastLayout->addLayout(formLayout);

	QHBoxLayout *metadataButtonLayout = new QHBoxLayout();
	metadataButtonLayout->addStretch();
	QPushButton *updateMetadataButton = new QPushButton("Update Broadcast", broadcastCard);
	updateMetadataButton->setAutoDefault(false);
	metadataButtonLayout->addWidget(updateMetadataButton);
	broadcastLayout->addSpacing(2);
	broadcastLayout->addLayout(metadataButtonLayout);

	rootLayout->addWidget(broadcastCard);

	// Footer
	rootLayout->addStretch(1);
	rootLayout->addSpacing(14);
	QHBoxLayout *buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();

	QPushButton *cancelButton = new QPushButton("Cancel", dlg);
	cancelButton->setAutoDefault(false);
	QPushButton *okButton = new QPushButton("Save", dlg);
	okButton->setProperty("loreRole", "primary");
	okButton->setDefault(true);
	okButton->setMinimumWidth(90);
	buttonLayout->addWidget(cancelButton);
	buttonLayout->addWidget(okButton);

	rootLayout->addLayout(buttonLayout);

	set_auth_state(loginButton, AuthUiState::Checking);
	dlg->resize(600, dlg->sizeHint().height());

	QObject::connect(cancelButton, &QPushButton::clicked, dlg, &QDialog::reject);

	QObject::connect(loginButton, &QPushButton::clicked,
    	[loginButton, broadcastsDropdown, titleEdit, descriptionEdit, dlg]() {
		if (is_signed_in_state(loginButton)) {
			QMessageBox::StandardButton reply = QMessageBox::question(dlg, "Sign Out", "Are you sure you want to sign out of TrueLORE?", QMessageBox::Yes|QMessageBox::No);
			if (reply == QMessageBox::No) {
				return;
			}
			taffylabs_clear_settings();
			set_auth_state(loginButton, AuthUiState::SignedOut);
			titleEdit->clear();
			descriptionEdit->clear();
			load_broadcasts_into_dropdown(broadcastsDropdown, loginButton, dlg);
			return;
		}
        set_auth_state(loginButton, AuthUiState::Starting);
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
            set_auth_state(loginButton, AuthUiState::Failed,
                           "Couldn't start sign-in. Check your connection and try again.");
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
				     "[TrueLORE] Broadcast array JSON was not an array");
				return;
			}

			QJsonArray arr = doc.array();
			if (iSelected < 0 || iSelected >= arr.size()) {
				blog(LOG_WARNING,
				     "[TrueLORE] Broadcast dropdown index out of range: %d",
				     iSelected);
				titleEdit->clear();
				descriptionEdit->clear();
				return;
			}

			QJsonObject obj = arr.at(iSelected).toObject();
			QJsonObject metadata = obj.value("metadata").toObject();

			taffylabs_set_selected_broadcast_object(QJsonDocument(obj).toJson(QJsonDocument::Compact).constData());
			titleEdit->setText(metadata.value("title").toString());
			descriptionEdit->setPlainText(
				metadata.value("description").toString());
		});

	// Update metadata callback
	QObject::connect(updateMetadataButton, &QPushButton::clicked,
		[titleEdit, descriptionEdit, broadcastsDropdown, loginButton, dlg]() {
			int iSelected = broadcastsDropdown->currentIndex();
			blog(LOG_INFO,
			     "[TrueLORE] Update metadata clicked for index %d",
			     iSelected);

				taffylabs_set_title(titleEdit->text().toUtf8().constData());
				taffylabs_set_description(
				descriptionEdit->toPlainText().toUtf8().constData());
			
			QJsonObject obj = QJsonDocument::fromJson(QByteArray(taffylabs_get_selected_broadcast_object())).object();
			QJsonObject metadataObj = obj.value("metadata").toObject();			
			QJsonObject updateObj;
			updateObj.insert("metadata", metadataObj);
			
			QByteArray jsonBytes = QJsonDocument(updateObj).toJson(QJsonDocument::Compact);
			lore_api_result result = lore_broadcastsUpdate(taffylabs_get_bearer_token(), taffylabs_get_streamID(), jsonBytes.constData(), nullptr);
			
			if (!result.ok) {
				taffylabs_show_error("Error", "Could not update broadcast metadata");
				blog(LOG_WARNING, "[TrueLORE] Could not update broadcast metadata: %s", result.error_text ? result.error_text : "no response");
			}
			// Put your API/update logic here
			blog(LOG_INFO,
			     "[TrueLORE] Updated broadcast metadata: title='%s'",
			     titleEdit->text().toUtf8().constData());
			load_broadcasts_into_dropdown(broadcastsDropdown, loginButton, dlg);	
		});
	
	// Save and close
	QObject::connect(okButton, &QPushButton::clicked,
		[dlg,
		 enableLoreStreamCheck,
		 enforce2kCheck,
		 broadcastsDropdown,
		 titleEdit,
		 descriptionEdit]() {
			taffylabs_set_enabled(enableLoreStreamCheck->isChecked());
			taffylabs_set_enforce_2k_fps(enforce2kCheck->isChecked());

			blog(LOG_INFO,
			     "[TrueLORE] Selected dropdown value: %s",
			     broadcastsDropdown->currentText().toUtf8().constData());

			taffylabs_set_title(titleEdit->text().toUtf8().constData());
			taffylabs_set_description(
				descriptionEdit->toPlainText().toUtf8().constData());

			std::string mnuUpdate =
				std::string("TaffyLabs Broadcast: ") + taffylabs_get_title();
			taffylabs_update_status_indicator(mnuUpdate.c_str());
			taffylabs_save_settings();

			blog(LOG_INFO,
			     "[TrueLORE] settings saved: enabled=%s",
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
	QTimer::singleShot(0, dlg, [dlg]() { fit_dialog_height(dlg); });

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
	taffylabs_update_status_indicator("Taffylabs TrueLORE Initializing!");
	taffylabs_load_settings();
	if(lore_authConfirm(taffylabs_get_bearer_token())){
		blog(LOG_INFO, "[TrueLORE] Authentication confirmed successfully on UI init.");		
		taffylabs_update_status_indicator("Taffylabs TrueLORE Authenticated Successfully!");
	}else{
		taffylabs_clear_settings();
		taffylabs_update_status_indicator("Please Authenticate Taffylabs TrueLORE!");
	}

	//if(false){
	//	const char *selBroadCast = taffylabs_get_selected_broadcast_object();
	//	if (!selBroadCast || !*selBroadCast) {
	//		taffylabs_update_status_indicator("Please check your TaffyLabs Settings!");
	//	} else {
	//		QJsonParseError parseError;
	//		QJsonDocument sbDoc =
	//			QJsonDocument::fromJson(QByteArray(selBroadCast), &parseError);
	//		
	//		if (parseError.error != QJsonParseError::NoError || !sbDoc.isObject()) {
	//			taffylabs_update_status_indicator("Please check your TaffyLabs Settings!");
	//		} else {
	//			QJsonObject rootObj = sbDoc.object();
	//			QJsonObject metadataObj = rootObj.value("metadata").toObject();
	//			QString title = QStringLiteral("TaffyLabs Broadcast: ") + metadataObj.value("title").toString();

	//			if (title.isEmpty()) {
	//				taffylabs_update_status_indicator("Please check your TaffyLabs Settings!");
	//			} else {
	//				QByteArray titleUtf8 = title.toUtf8();
	//				taffylabs_update_status_indicator(titleUtf8.constData());
	//			}
	//		}
	//	}
	//}
	

	g_actionSettings = static_cast<QAction *>(
		obs_frontend_add_tools_menu_qaction((std::string("TaffyLabs TrueLORE Settings")).c_str()));

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
