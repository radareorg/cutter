
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

RAnnotatedCode *Decompiler::makeWarning(QString warningMessage)
{
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

static char *jsonValueToString(const QJsonValue &str)
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
        code->code = strdup(json["code"].toString().toStdString().c_str());
        for (const auto &iter : json["annotations"].toArray()) {
            QJsonObject jsonAnnotation = iter.toObject();
            RCodeAnnotation annotation = {};
            annotation.start = jsonAnnotation["start"].toInt();
            annotation.end = jsonAnnotation["end"].toInt();
            QString type = jsonAnnotation["type"].toString();
            if (type == "offset") {
                annotation.type = R_CODE_ANNOTATION_TYPE_OFFSET;
                annotation.offset.offset = jsonAnnotation["offset"].toString().toULongLong();
            } else if (type == "function_name") {
                annotation.type = R_CODE_ANNOTATION_TYPE_FUNCTION_NAME;
                annotation.reference.name = jsonValueToString(jsonAnnotation["name"]);
                annotation.reference.offset = jsonAnnotation["offset"].toString().toULongLong();
            } else if (type == "global_variable") {
                annotation.type = R_CODE_ANNOTATION_TYPE_GLOBAL_VARIABLE;
                annotation.reference.offset = jsonAnnotation["offset"].toString().toULongLong();
            } else if (type == "constant_variable") {
                annotation.type = R_CODE_ANNOTATION_TYPE_CONSTANT_VARIABLE;
                annotation.reference.offset = jsonAnnotation["offset"].toString().toULongLong();
            } else if (type == "local_variable") {
                annotation.type = R_CODE_ANNOTATION_TYPE_LOCAL_VARIABLE;
                annotation.variable.name = jsonValueToString(jsonAnnotation["name"]);
            } else if (type == "function_parameter") {
                annotation.type = R_CODE_ANNOTATION_TYPE_FUNCTION_PARAMETER;
                annotation.variable.name = jsonValueToString(jsonAnnotation["name"]);
            } else if (type == "syntax_highlight") {
                annotation.type = R_CODE_ANNOTATION_TYPE_SYNTAX_HIGHLIGHT;
                QString highlightType = jsonAnnotation["syntax_highlight"].toString();
                if (highlightType == "keyword") {
                    annotation.syntax_highlight.type = R_SYNTAX_HIGHLIGHT_TYPE_KEYWORD;
                } else if (highlightType == "comment") {
                    annotation.syntax_highlight.type = R_SYNTAX_HIGHLIGHT_TYPE_COMMENT;
                } else if (highlightType == "datatype") {
                    annotation.syntax_highlight.type = R_SYNTAX_HIGHLIGHT_TYPE_DATATYPE;
                } else if (highlightType == "function_name") {
                    annotation.syntax_highlight.type = R_SYNTAX_HIGHLIGHT_TYPE_FUNCTION_NAME;
                } else if (highlightType == "function_parameter") {
                    annotation.syntax_highlight.type = R_SYNTAX_HIGHLIGHT_TYPE_FUNCTION_PARAMETER;
                } else if (highlightType == "local_variable") {
                    annotation.syntax_highlight.type = R_SYNTAX_HIGHLIGHT_TYPE_LOCAL_VARIABLE;
                } else if (highlightType == "constant_variable") {
                    annotation.syntax_highlight.type = R_SYNTAX_HIGHLIGHT_TYPE_CONSTANT_VARIABLE;
                } else if (highlightType == "global_variable") {
                    annotation.syntax_highlight.type = R_SYNTAX_HIGHLIGHT_TYPE_GLOBAL_VARIABLE;
                }
            }
            r_annotated_code_add_annotation(code, &annotation);
        }
        emit finished(code);
    });
    task->startTask();
}
