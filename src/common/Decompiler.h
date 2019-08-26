#ifndef DECOMPILER_H
#define DECOMPILER_H

#include "CutterCommon.h"

#include <QString>
#include <QObject>

struct CodeAnnotation
{
    size_t start;
    size_t end;

    enum class Type { Offset };
    Type type;

    union
    {
        struct
        {
            ut64 offset;
        } offset;
    };
};

/**
 * Describes the result of a Decompilation Process with optional metadata
 */
struct AnnotatedCode
{
    /**
     * The entire decompiled code
     */
    QString code;

    QList<CodeAnnotation> annotations;

    ut64 OffsetForPosition(size_t pos) const;
    size_t PositionForOffset(ut64 offset) const;
};

/**
 * Implements a decompiler that can be registered using CutterCore::registerDecompiler()
 */
class Decompiler: public QObject
{
    Q_OBJECT

private:
    const QString id;
    const QString name;

public:
    Decompiler(const QString &id, const QString &name, QObject *parent = nullptr);
    virtual ~Decompiler() = default;

    QString getId() const   { return id; }
    QString getName() const { return name; }

    virtual AnnotatedCode decompileAt(RVA addr) =0;
};

class R2DecDecompiler: public Decompiler
{
    Q_OBJECT

public:
    explicit R2DecDecompiler(QObject *parent = nullptr);
    AnnotatedCode decompileAt(RVA addr) override;

    static bool isAvailable();
};

#endif //DECOMPILER_H
