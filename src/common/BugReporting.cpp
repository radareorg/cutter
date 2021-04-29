#include "BugReporting.h"

#include "Cutter.h"
#include <QUrl>
#include <QJsonObject>
#include "CutterConfig.h"
#include <QDesktopServices>

void openIssue()
{
    QString url, osInfo, format, arch, type;
    // Pull in info needed for git issue
    osInfo = QSysInfo::productType() + " "
            + (QSysInfo::productVersion() == "unknown" ? "" : QSysInfo::productVersion());
    //RzBinInfo *info = Core()->getFileInfo();
    // TODO !
    //QJsonObject coreObj = docu.object()["core"].toObject();
    //QJsonObject binObj = docu.object()["bin"].toObject();
    //if (!binObj.QJsonObject::isEmpty()) {
    //    format = coreObj["format"].toString();
    //    arch = binObj["arch"].toString();
    //    if (!binObj["type"].isUndefined()) {
    //        type = coreObj["type"].toString();
    //    } else {
    //        type = "N/A";
    //    }
    //} else {
    //    format = coreObj["format"].toString();
    //    arch = "N/A";
    //    type = "N/A";
    //}
    url = "https://github.com/rizinorg/cutter/issues/new?&body=**Environment information**\n* "
          "Operating System: "
            + osInfo + "\n* Cutter version: " + CUTTER_VERSION_FULL + "\n* File format: " + format
            + "\n * Arch: " + arch + "\n * Type: " + type
            + "\n\n**Describe the bug**\nA clear and concise description of what the bug "
              "is.\n\n**To Reproduce**\n"
              "Steps to reproduce the behavior:\n1. Go to '...'\n2. Click on '....'\n3. Scroll "
              "down to '....'\n"
              "4. See error\n\n**Expected behavior**\n"
              "A clear and concise description of what you expected to happen.\n\n"
              "**Screenshots**\nIf applicable, add screenshots to help explain your problem.\n\n"
              "**Additional context**\nAdd any other context about the problem here.";

    QDesktopServices::openUrl(QUrl(url, QUrl::TolerantMode));
}
