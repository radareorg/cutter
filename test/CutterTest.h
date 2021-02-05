
#ifndef CUTTERTEST_H
#define CUTTERTEST_H

#include <QObject>

class CutterTest: public QObject
{
    Q_OBJECT

protected slots:
    virtual void initTestCase();
    virtual void cleanupTestCase();
};

#endif //CUTTERTEST_H
