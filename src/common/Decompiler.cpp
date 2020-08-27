
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

static char *jsonValueToString(QJsonValue &str)
{
    return strdup(str.toString().toStdString().c_str());
}

void R2DecDecompiler::decompileAt(RVA addr)
{
    if (task) {
        return;
    }
    task = new R2Task("pddA @ " + QString::number(addr));
    connect(task, &R2Task::finished, this, [this]() {
        QJsonObject json = task->getResultJson().object();
        delete task;
        task = nullptr;
        if (json.isEmpty()) {
            emit finished(Decompiler::makeWarning(tr("Failed to parse JSON from r2dec")));
            return;
        }
        RAnnotatedCode *code = r_annotated_code_new(nullptr);
        QString codeString = json["code"].toString();
        // for (const auto &line : json["log"].toArray()) {
        //     if (!line.isString()) {
        //         continue;
        //     }
        //     codeString.append(line.toString() + "\n");
        // }
        // if (json["code"].isString()) {
        //     codeString.append(json["code"].toString());
        // }
        for (const auto &iter : json["annotations"].toArray()) {
            // if (!line.isString()) {
            //     continue;
            // }
            // codeString.append(line.toString() + "\n");
            QJsonObject jsonAnnotation = iter.toObject();
            RCodeAnnotation annotation = {};
            annotation.start = jsonAnnotation["start"].toInt();
            annotation.end = jsonAnnotation["end"].toInt();
            QString type = jsonAnnotation["type"].toString();
            bool ok;
            if (type == "offset") {
                annotation.type = R_CODE_ANNOTATION_TYPE_OFFSET;
                annotation.offset.offset = jsonAnnotation["offset"].toString().toULongLong(&ok);
            } else if (type == "function_name") {
                annotation.type = R_CODE_ANNOTATION_TYPE_FUNCTION_NAME;
                annotation.reference.name = jsonValueToString(jsonAnnotation["name"]);
                annotation.reference.offset = jsonAnnotation["offset"].toString().toULongLong(&ok);
            } else if (type == "global_variable") {
                annotation.type = R_CODE_ANNOTATION_TYPE_GLOBAL_VARIABLE;
                annotation.reference.offset = jsonAnnotation["offset"].toString().toULongLong(&ok);
            } else if (type == "constant_variable") {
                annotation.type = R_CODE_ANNOTATION_TYPE_CONSTANT_VARIABLE;
                annotation.reference.offset = jsonAnnotation["offset"].toString().toULongLong(&ok);
            } else if (type == "local_variable") {
                annotation.type = R_CODE_ANNOTATION_TYPE_LOCAL_VARIABLE;
                annotation.variable.name = jsonValueToString(jsonAnnotation["name"]);
            } else if (type == "function_parameter") {
                annotation.type = R_CODE_ANNOTATION_TYPE_FUNCTION_PARAMETER;
                annotation.variable.name = jsonValueToString(jsonAnnotation["name"]);
            }
            r_annotated_code_add_annotation(code, &annotation);
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
        // std::string tmp = codeString.toStdString();
        code->code = strdup(codeString.toStdString().c_str());
        emit finished(code);
    });
    task->startTask();
}
