
#ifndef AUTOTEST_H
#define AUTOTEST_H

// see http://qtcreator.blogspot.de/2009/10/running-multiple-unit-tests.html

#include <QTest>
#include <QList>
#include <QString>
#include <QSharedPointer>

namespace AutoTest
{
typedef QList<QObject *> TestList;

inline TestList &testList()
{
    static TestList list;
    return list;
}

inline bool findObject(QObject *object)
{
    TestList &list = testList();
    if (list.contains(object)) {
        return true;
    }

    for (QObject *test : list) {
        if (test->objectName() == object->objectName()) {
            return true;
        }
    }
    return false;
}

inline void addTest(QObject *object)
{
    TestList &list = testList();
    if (!findObject(object)) {
        list.append(object);
    }
}

inline int run(int argc, char *argv[])
{
    int ret = 0;

    for (QObject *test : testList()) {
        ret += QTest::qExec(test, argc, argv);
    }

    return ret;
}
}

template<class T>
class Test
{
public:
    QSharedPointer<T> child;

    explicit Test(const QString &name)
        : child(new T)
    {
        child->setObjectName(name);
        AutoTest::addTest(child.data());
    }
};

#define DECLARE_TEST(className) static Test<className> t(#className);

#endif //AUTOTEST_H
