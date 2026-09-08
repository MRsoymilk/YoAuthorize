#include "mainwindow.h"

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <span>

#include "ui_mainwindow.h"
#include "yoauthorize/sdk/local_authorizer.h"

namespace sdk = yoauthorize::sdk;

namespace {

constexpr qsizetype kMaxActivationResponseSize = 2 * 1024 * 1024;

struct AuthorizationConfigResult {
  sdk::Status status;
  sdk::LocalAuthorizationConfig config;
};

AuthorizationConfigResult makeAuthorizationConfig(
    const QString &product, const QString &keyId, const QString &keyPath,
    const QString &machineIdPath) {
  if (product.trimmed().isEmpty() || keyId.trimmed().isEmpty() ||
      keyPath.trimmed().isEmpty()) {
    return {
        .status = {
            .error = sdk::ClientError::InvalidConfig,
            .message = "Product, issuer key ID, and public key are required."}};
  }
  const QFileInfo keyInfo(keyPath);
  if (keyInfo.isSymLink() || !keyInfo.isFile()) {
    return {.status = {.error = sdk::ClientError::InvalidConfig,
                       .message = "Issuer public key must be a regular file."}};
  }
  QFile keyFile(keyPath);
  if (!keyFile.open(QIODevice::ReadOnly)) {
    return {.status = {.error = sdk::ClientError::InvalidConfig,
                       .message = "Issuer public key could not be opened."}};
  }
  const QByteArray bytes = keyFile.readAll();
  if (bytes.size() != static_cast<qsizetype>(yoauthorize::crypto::kKeySize)) {
    return {.status = {
                .error = sdk::ClientError::InvalidConfig,
                .message = "Issuer public key must contain exactly 32 bytes."}};
  }
  yoauthorize::crypto::Key key{};
  std::copy(bytes.begin(), bytes.end(), reinterpret_cast<char *>(key.data()));
  sdk::LocalAuthorizationConfig config{
      .product_id = product.trimmed().toStdString(),
      .trusted_license_keys = {{keyId.trimmed().toStdString(), key}},
  };
  if (!machineIdPath.trimmed().isEmpty()) {
    config.machine_id_path = machineIdPath.trimmed().toStdString();
    config.product_uuid_path =
        machineIdPath.trimmed().toStdString() + ".product-uuid";
  }
  return {.config = std::move(config)};
}

QString stateText(sdk::ClientState state) {
  switch (state) {
    case sdk::ClientState::Valid:
      return QObject::tr("VALID");
    case sdk::ClientState::Expired:
      return QObject::tr("EXPIRED");
    case sdk::ClientState::Revoked:
      return QObject::tr("REVOKED");
    case sdk::ClientState::Grace:
      return QObject::tr("GRACE");
    case sdk::ClientState::Error:
      return QObject::tr("DISABLED");
    default:
      return QObject::tr("NOT AUTHORIZED");
  }
}

}  // namespace

MainWindow::MainWindow(StartupOptions options, QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      options_(std::move(options)) {
  ui->setupUi(this);
  buildInterface();
  applyStartupOptions();
}

MainWindow::~MainWindow() {
  if (activationReply_) activationReply_->abort();
  delete ui;
}

void MainWindow::buildInterface() {
  setWindowTitle(tr("YoAuthorize Feature Console"));
  resize(860, 640);
  network_ = new QNetworkAccessManager(this);
  auto *page = new QVBoxLayout(ui->centralwidget);
  page->setContentsMargins(28, 24, 28, 24);
  page->setSpacing(18);

  auto *title = new QLabel(tr("Authorization Control"), this);
  QFont titleFont = title->font();
  titleFont.setPointSize(20);
  titleFont.setBold(true);
  title->setFont(titleFont);
  page->addWidget(title);
  auto *subtitle = new QLabel(tr("Load a signed local license or activate "
                                 "through a trusted HTTPS endpoint."),
                              this);
  subtitle->setWordWrap(true);
  page->addWidget(subtitle);

  auto *identityBox = new QGroupBox(tr("License Identity"), this);
  auto *identityForm = new QFormLayout(identityBox);
  productEdit_ = new QLineEdit(identityBox);
  productEdit_->setPlaceholderText(tr("product-demo"));
  keyIdEdit_ = new QLineEdit(identityBox);
  keyIdEdit_->setPlaceholderText(tr("issuer-demo"));
  keyPathEdit_ = new QLineEdit(identityBox);
  auto *keyBrowse = new QPushButton(tr("Browse..."), identityBox);
  auto *keyRow = new QHBoxLayout;
  keyRow->addWidget(keyPathEdit_);
  keyRow->addWidget(keyBrowse);
  identityForm->addRow(tr("Product ID"), productEdit_);
  identityForm->addRow(tr("Issuer key ID"), keyIdEdit_);
  identityForm->addRow(tr("Issuer public key"), keyRow);
  page->addWidget(identityBox);

  sourceTabs_ = new QTabWidget(this);
  auto *localPage = new QWidget(sourceTabs_);
  auto *localLayout = new QVBoxLayout(localPage);
  auto *licenseRow = new QHBoxLayout;
  licensePathEdit_ = new QLineEdit(localPage);
  licensePathEdit_->setPlaceholderText(tr("Signed .yalc license file"));
  auto *licenseBrowse = new QPushButton(tr("Browse..."), localPage);
  licenseRow->addWidget(licensePathEdit_);
  licenseRow->addWidget(licenseBrowse);
  localAuthorizeButton_ = new QPushButton(tr("Authorize from File"), localPage);
  localLayout->addLayout(licenseRow);
  localLayout->addWidget(localAuthorizeButton_, 0, Qt::AlignRight);
  sourceTabs_->addTab(localPage, tr("Local License"));

  auto *onlinePage = new QWidget(sourceTabs_);
  auto *onlineForm = new QFormLayout(onlinePage);
  activationUrlEdit_ = new QLineEdit(onlinePage);
  activationUrlEdit_->setPlaceholderText(
      tr("https://license.example.com/v1/activate"));
  activationCodeEdit_ = new QLineEdit(onlinePage);
  activationCodeEdit_->setEchoMode(QLineEdit::Password);
  caPathEdit_ = new QLineEdit(onlinePage);
  auto *caBrowse = new QPushButton(tr("Browse..."), onlinePage);
  auto *caRow = new QHBoxLayout;
  caRow->addWidget(caPathEdit_);
  caRow->addWidget(caBrowse);
  onlineActivateButton_ = new QPushButton(tr("Activate Online"), onlinePage);
  onlineForm->addRow(tr("Activation URL"), activationUrlEdit_);
  onlineForm->addRow(tr("Activation code"), activationCodeEdit_);
  onlineForm->addRow(tr("Additional CA certificate"), caRow);
  onlineForm->addRow(QString(), onlineActivateButton_);
  sourceTabs_->addTab(onlinePage, tr("Online Activation"));
  page->addWidget(sourceTabs_);

  auto *statusBox = new QGroupBox(tr("Authorization Status"), this);
  auto *statusLayout = new QVBoxLayout(statusBox);
  auto *summaryRow = new QHBoxLayout;
  stateLabel_ = new QLabel(tr("NOT AUTHORIZED"), statusBox);
  QFont stateFont = stateLabel_->font();
  stateFont.setBold(true);
  stateLabel_->setFont(stateFont);
  expiryLabel_ = new QLabel(tr("Expiration: --"), statusBox);
  summaryRow->addWidget(stateLabel_);
  summaryRow->addStretch();
  summaryRow->addWidget(expiryLabel_);
  detailLabel_ = new QLabel(tr("Protected features are disabled."), statusBox);
  detailLabel_->setWordWrap(true);
  featureList_ = new QListWidget(statusBox);
  featureList_->setMaximumHeight(96);
  statusLayout->addLayout(summaryRow);
  statusLayout->addWidget(detailLabel_);
  statusLayout->addWidget(featureList_);
  page->addWidget(statusBox);

  auto *actions = new QHBoxLayout;
  captureButton_ = new QPushButton(tr("Capture"), this);
  exportButton_ = new QPushButton(tr("Export"), this);
  actions->addWidget(captureButton_);
  actions->addWidget(exportButton_);
  actions->addStretch();
  page->addLayout(actions);

  connect(keyBrowse, &QPushButton::clicked, this, &MainWindow::chooseIssuerKey);
  connect(licenseBrowse, &QPushButton::clicked, this,
          &MainWindow::chooseLicenseFile);
  connect(caBrowse, &QPushButton::clicked, this,
          &MainWindow::chooseCaCertificate);
  connect(localAuthorizeButton_, &QPushButton::clicked, this,
          &MainWindow::authorizeFromFile);
  connect(onlineActivateButton_, &QPushButton::clicked, this,
          &MainWindow::activateOnline);
  connect(captureButton_, &QPushButton::clicked, this, [this] {
    if (!hasFeature("capture")) {
      denyAuthorization(tr("The capture feature is not authorized."));
      return;
    }
    statusBar()->showMessage(tr("Capture operation authorized."), 3000);
  });
  connect(exportButton_, &QPushButton::clicked, this, [this] {
    if (!hasFeature("export")) {
      denyAuthorization(tr("The export feature is not authorized."));
      return;
    }
    statusBar()->showMessage(tr("Export operation authorized."), 3000);
  });
  updateProtectedActions();
}

void MainWindow::applyStartupOptions() {
  productEdit_->setText(options_.productId);
  keyIdEdit_->setText(options_.issuerKeyId);
  keyPathEdit_->setText(options_.issuerPublicKey);
  licensePathEdit_->setText(options_.licenseFile);
  activationUrlEdit_->setText(options_.activationUrl);
  activationCodeEdit_->setText(options_.activationCode);
  caPathEdit_->setText(options_.caCertificate);
  if (!options_.activationUrl.isEmpty()) sourceTabs_->setCurrentIndex(1);
  if (!options_.licenseFile.isEmpty()) {
    QTimer::singleShot(0, this, &MainWindow::authorizeFromFile);
  } else if (!options_.activationUrl.isEmpty()) {
    QTimer::singleShot(0, this, &MainWindow::activateOnline);
  }
}

void MainWindow::chooseLicenseFile() {
  const auto path = QFileDialog::getOpenFileName(
      this, tr("Select Signed License"), {},
      tr("YoAuthorize License (*.yalc);;All Files (*)"));
  if (!path.isEmpty()) licensePathEdit_->setText(path);
}

void MainWindow::chooseIssuerKey() {
  const auto path =
      QFileDialog::getOpenFileName(this, tr("Select Issuer Public Key"), {},
                                   tr("Public Key (*.pub);;All Files (*)"));
  if (!path.isEmpty()) keyPathEdit_->setText(path);
}

void MainWindow::chooseCaCertificate() {
  const auto path = QFileDialog::getOpenFileName(
      this, tr("Select CA Certificate"), {},
      tr("PEM Certificate (*.pem *.crt);;All Files (*)"));
  if (!path.isEmpty()) caPathEdit_->setText(path);
}

void MainWindow::authorizeFromFile() {
  const auto config =
      makeAuthorizationConfig(productEdit_->text(), keyIdEdit_->text(),
                              keyPathEdit_->text(), options_.machineIdFile);
  if (!config.status) {
    denyAuthorization(QString::fromStdString(config.status.message));
    return;
  }
  if (licensePathEdit_->text().trimmed().isEmpty()) {
    denyAuthorization(tr("Select a signed license file."));
    return;
  }
  applyAuthorization(sdk::authorizeLicenseFile(
      licensePathEdit_->text().trimmed().toStdString(), config.config));
}

void MainWindow::activateOnline() {
  if (activationReply_) return;
  const auto config =
      makeAuthorizationConfig(productEdit_->text(), keyIdEdit_->text(),
                              keyPathEdit_->text(), options_.machineIdFile);
  if (!config.status) {
    denyAuthorization(QString::fromStdString(config.status.message));
    return;
  }
  const QUrl url(activationUrlEdit_->text().trimmed());
  if (!url.isValid() || url.scheme() != QStringLiteral("https") ||
      url.host().isEmpty()) {
    denyAuthorization(tr("Activation URL must be a valid HTTPS URL."));
    return;
  }
  if (activationCodeEdit_->text().isEmpty()) {
    denyAuthorization(tr("Activation code is required."));
    return;
  }
  const auto identity = sdk::machineIdentity(config.config);
  if (!identity) {
    denyAuthorization(QString::fromStdString(identity.status.message));
    return;
  }

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/json"));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  if (!caPathEdit_->text().trimmed().isEmpty()) {
    const auto certificates =
        QSslCertificate::fromPath(caPathEdit_->text().trimmed());
    if (certificates.isEmpty()) {
      denyAuthorization(tr("The additional CA certificate is invalid."));
      return;
    }
    auto ssl = request.sslConfiguration();
    auto authorities = ssl.caCertificates();
    authorities.append(certificates);
    ssl.setCaCertificates(authorities);
    request.setSslConfiguration(ssl);
  }
  const QJsonObject payload{
      {QStringLiteral("protocol_version"), 1},
      {QStringLiteral("product_id"), productEdit_->text().trimmed()},
      {QStringLiteral("activation_code"), activationCodeEdit_->text()},
      {QStringLiteral("machine_id"),
       QString::fromStdString(identity.machine_id)},
  };
  activationResponse_.clear();
  activationReply_ = network_->post(
      request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
  localAuthorizeButton_->setEnabled(false);
  onlineActivateButton_->setEnabled(false);
  detailLabel_->setText(tr("Contacting activation server..."));
  connect(activationReply_, &QIODevice::readyRead, this, [this] {
    activationResponse_.append(activationReply_->readAll());
    if (activationResponse_.size() > kMaxActivationResponseSize) {
      activationReply_->abort();
    }
  });
  connect(activationReply_, &QNetworkReply::finished, this,
          &MainWindow::handleActivationFinished);
  QTimer::singleShot(10000, activationReply_, [reply = activationReply_] {
    if (reply->isRunning()) reply->abort();
  });
}

void MainWindow::handleActivationFinished() {
  QNetworkReply *reply = activationReply_;
  activationReply_ = nullptr;
  activationResponse_.append(reply->readAll());
  const auto networkError = reply->error();
  const auto httpStatus =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  reply->deleteLater();
  activationCodeEdit_->clear();
  localAuthorizeButton_->setEnabled(true);
  onlineActivateButton_->setEnabled(true);
  if (networkError != QNetworkReply::NoError || httpStatus < 200 ||
      httpStatus >= 300 ||
      activationResponse_.size() > kMaxActivationResponseSize) {
    denyAuthorization(tr("Activation server request failed."));
    return;
  }
  QJsonParseError parseError;
  const auto document =
      QJsonDocument::fromJson(activationResponse_, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    denyAuthorization(tr("Activation server returned invalid JSON."));
    return;
  }
  const auto encodedLicense =
      document.object().value(QStringLiteral("license")).toString();
  const auto license = QByteArray::fromBase64(
      encodedLicense.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
  if (license.isEmpty() || license.size() > 1024 * 1024) {
    denyAuthorization(tr("Activation server returned an invalid license."));
    return;
  }
  const auto config =
      makeAuthorizationConfig(productEdit_->text(), keyIdEdit_->text(),
                              keyPathEdit_->text(), options_.machineIdFile);
  if (!config.status) {
    denyAuthorization(QString::fromStdString(config.status.message));
    return;
  }
  const std::span bytes(reinterpret_cast<const std::uint8_t *>(license.data()),
                        static_cast<std::size_t>(license.size()));
  applyAuthorization(sdk::authorizeLicense(bytes, config.config));
}

void MainWindow::applyAuthorization(
    const sdk::LocalAuthorizationResult &result) {
  if (!result) {
    denyAuthorization(QString::fromStdString(result.status.message));
    return;
  }
  snapshot_ = result.snapshot;
  stateLabel_->setText(stateText(snapshot_.state));
  detailLabel_->setText(tr("The signed license was verified successfully."));
  featureList_->clear();
  for (const auto &feature : snapshot_.features) {
    featureList_->addItem(QString::fromStdString(feature));
  }
  expiryLabel_->setText(
      snapshot_.expire_time == 0
          ? tr("Expiration: Permanent")
          : tr("Expiration: %1")
                .arg(QDateTime::fromSecsSinceEpoch(snapshot_.expire_time)
                         .toLocalTime()
                         .toString(Qt::ISODate)));
  updateProtectedActions();
  statusBar()->showMessage(tr("Authorization is valid."), 3000);
  emit authorizationFinished(true);
}

void MainWindow::denyAuthorization(const QString &message) {
  snapshot_ = {.state = sdk::ClientState::Error};
  stateLabel_->setText(stateText(snapshot_.state));
  expiryLabel_->setText(tr("Expiration: --"));
  detailLabel_->setText(message);
  featureList_->clear();
  updateProtectedActions();
  if (!options_.autoExit) {
    QMessageBox::critical(
        this, tr("Authorization Disabled"),
        tr("Protected features are disabled.\n\n%1").arg(message));
  }
  emit authorizationFinished(false);
}

void MainWindow::updateProtectedActions() {
  captureButton_->setEnabled(hasFeature("capture"));
  exportButton_->setEnabled(hasFeature("export"));
}

bool MainWindow::hasFeature(const char *feature) const {
  return snapshot_.state == sdk::ClientState::Valid &&
         std::find(snapshot_.features.begin(), snapshot_.features.end(),
                   feature) != snapshot_.features.end();
}
