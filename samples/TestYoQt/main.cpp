#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QTimer>

#include "mainwindow.h"

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  QCoreApplication::setApplicationName("TestYoQt");
  QCommandLineParser parser;
  parser.setApplicationDescription("YoAuthorize Qt authorization sample");
  parser.addHelpOption();
  parser.addOptions({
      {{"p", "product"}, "Product identifier.", "id"},
      {{"k", "issuer-key-id"}, "Trusted license issuer key identifier.", "id"},
      {{"K", "issuer-public-key"}, "Raw 32-byte issuer public key.", "path"},
      {{"l", "license"}, "Local signed .yalc license.", "path"},
      {{"u", "activation-url"}, "HTTPS activation endpoint.", "url"},
      {{"a", "activation-code"}, "Activation code.", "code"},
      {{"c", "ca-certificate"}, "Additional PEM CA certificate.", "path"},
      {{"m", "machine-id-file"}, "Machine ID source override.", "path"},
      {"auto-exit",
       "Exit after automatic authorization for integration tests."},
  });
  parser.process(a);
  const StartupOptions options{
      .productId = parser.value("product"),
      .issuerKeyId = parser.value("issuer-key-id"),
      .issuerPublicKey = parser.value("issuer-public-key"),
      .licenseFile = parser.value("license"),
      .activationUrl = parser.value("activation-url"),
      .activationCode = parser.value("activation-code"),
      .caCertificate = parser.value("ca-certificate"),
      .machineIdFile = parser.value("machine-id-file"),
      .autoExit = parser.isSet("auto-exit"),
  };
  MainWindow w(options);
  if (options.autoExit) {
    QObject::connect(&w, &MainWindow::authorizationFinished, &a,
                     [&a](bool authorized) { a.exit(authorized ? 0 : 1); });
    QTimer::singleShot(15000, &a, [&a] { a.exit(1); });
  }
  w.show();
  return QApplication::exec();
}
