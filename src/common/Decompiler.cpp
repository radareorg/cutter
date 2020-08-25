
#include "Decompiler.h"
#include "Cutter.h"

#include <QJsonObject>
#include <QJsonArray>

Decompiler::Decompiler(const QString &id, const QString &name, QObject *parent)
    : QObject(parent),
    id(id),
    name(name)
{
}

RAnnotatedCode *Decompiler::makeWarning(QString warningMessage){
    std::string temporary = warningMessage.toStdString();
    return r_annotated_code_new(strdup(temporary.c_str()));
}

R2DecDecompiler::R2DecDecompiler(QObject *parent)
    : Decompiler("r2dec", "r2dec", parent)
{
    task = nullptr;
}

bool R2DecDecompiler::isAvailable()
{
    return Core()->cmdList("e cmd.pdc=?").contains(QStringLiteral("pdd"));
}

void R2DecDecompiler::decompileAt(RVA addr)
{
    if (task) {
        return;
    }
    task = new R2Task("pdgj @ " + QString::number(addr));
    connect(task, &R2Task::finished, this, [this]() {
        QJsonObject json = task->getResultJson().object();
        delete task;
        task = nullptr;
        if (json.isEmpty()) {
            emit finished(Decompiler::makeWarning(tr("Failed to parse JSON from r2dec")));
            return;
        }
        RAnnotatedCode *code = r_annotated_code_new(nullptr);
        QString codeString = "";
        // for (const auto &line : json["log"].toArray()) {
        //     if (!line.isString()) {
        //         continue;
        //     }
        //     codeString.append(line.toString() + "\n");
        // }
        if (json["code"].isString()) {
            codeString.append(json["code"].toString());
        }
        for (const auto &annotation : json["annotations"].toArray()) {
            // if (!line.isString()) {
            //     continue;
            // }
            // codeString.append(line.toString() + "\n");
            auto Crad = annotation.toObject()["end"];
            // if(Crad.isInt()){
                int asdf = Crad.toInt();;
            // }
            auto xo = Crad;
        }
        // auto linesArray = json["lines"].toArray();
        // for (const auto &line : linesArray) {
        //     QJsonObject lineObject = line.toObject();
        //     if (lineObject.isEmpty()) {
        //         continue;
        //     }
        //     RCodeAnnotation annotationi = { 0 };
        //     annotationi.start = codeString.length();
        //     codeString.append(lineObject["str"].toString() + "\n");
        //     annotationi.end = codeString.length();
        //     bool ok;
        //     annotationi.type = R_CODE_ANNOTATION_TYPE_OFFSET;
        //     annotationi.offset.offset = lineObject["offset"].toVariant().toULongLong(&ok);
        //     r_annotated_code_add_annotation(code, &annotationi);
        // }

        // for (const auto &line : json["errors"].toArray()) {
        //     if (!line.isString()) {
        //         continue;
        //     }
        //     codeString.append(line.toString() + "\n");
        // }
        std::string tmp = codeString.toStdString();
        code->code = strdup(tmp.c_str());
        emit finished(code);
    });
    task->startTask();
}
