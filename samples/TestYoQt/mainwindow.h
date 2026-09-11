#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QByteArray>
#include <QMainWindow>
#include <QString>

#include "yoauthorize/sdk/client.h"
#include "yoauthorize/sdk/local_authorizer.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QLabel;
class QLineEdit;
class QListWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;
class QTabWidget;

struct StartupOptions {
  QString productId;
  QString issuerKeyId;
  QString issuerPublicKey;
  QString licenseFile;
  QString activationUrl;
  QString activationCode;
  QString caCertificate;
  QString machineIdFile;
  bool autoExit = false;
};

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(StartupOptions options = {}, QWidget *parent = nullptr);
  ~MainWindow() override;

 signals:
  void authorizationFinished(bool authorized);

 private:
  void buildInterface();
  void applyStartupOptions();
  void chooseLicenseFile();
  void chooseIssuerKey();
  void chooseCaCertificate();
  void authorizeFromFile();
  void activateOnline();
  void handleActivationFinished();
  void applyAuthorization(
      const yoauthorize::sdk::LocalAuthorizationResult &result);
  void denyAuthorization(const QString &message);
  void updateProtectedActions();
  bool hasFeature(const char *feature) const;

  Ui::MainWindow *ui;
  StartupOptions options_;
  yoauthorize::sdk::LicenseSnapshot snapshot_;
  QNetworkAccessManager *network_ = nullptr;
  QNetworkReply *activationReply_ = nullptr;
  QByteArray activationResponse_;
  QTabWidget *sourceTabs_ = nullptr;
  QLineEdit *productEdit_ = nullptr;
  QLineEdit *keyIdEdit_ = nullptr;
  QLineEdit *keyPathEdit_ = nullptr;
  QLineEdit *licensePathEdit_ = nullptr;
  QLineEdit *activationUrlEdit_ = nullptr;
  QLineEdit *activationCodeEdit_ = nullptr;
  QLineEdit *caPathEdit_ = nullptr;
  QLabel *stateLabel_ = nullptr;
  QLabel *expiryLabel_ = nullptr;
  QLabel *detailLabel_ = nullptr;
  QListWidget *featureList_ = nullptr;
  QPushButton *localAuthorizeButton_ = nullptr;
  QPushButton *onlineActivateButton_ = nullptr;
  QPushButton *captureButton_ = nullptr;
  QPushButton *exportButton_ = nullptr;
};
#endif  // MAINWINDOW_H
