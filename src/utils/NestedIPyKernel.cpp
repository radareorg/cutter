
#include <Python.h>

#include <QFile>

#include "cutter.h"
#include "NestedIPyKernel.h"

NestedIPyKernel *NestedIPyKernel::start(const QStringList &argv)
{
    PyThreadState *parentThreadState = PyThreadState_Get();

    PyThreadState *threadState = Py_NewInterpreter();
    if (!threadState)
    {
        qWarning() << "Could not create subinterpreter.";
        return nullptr;
    }

    QFile moduleFile(":/python/cutter_ipykernel.py");
    moduleFile.open(QIODevice::ReadOnly);
    QByteArray moduleCode = moduleFile.readAll();
    moduleFile.close();

    auto moduleCodeObject = Py_CompileString(moduleCode.constData(), "cutter_ipykernel.py", Py_file_input);
    if (!moduleCodeObject)
    {
        qWarning() << "Could not compile cutter_ipykernel.";
        return nullptr;
    }
    auto cutterIPykernelModule = PyImport_ExecCodeModule("cutter_ipykernel", moduleCodeObject);
    Py_DECREF(moduleCodeObject);
    if (!cutterIPykernelModule)
    {
        qWarning() << "Could not import cutter_ipykernel.";
        return nullptr;
    }

    auto kernel = new NestedIPyKernel(cutterIPykernelModule, argv);

    PyThreadState_Swap(parentThreadState);

    return kernel;
}

NestedIPyKernel::NestedIPyKernel(PyObject *cutterIPykernelModule, const QStringList &argv)
{
    auto launchFunc = PyObject_GetAttrString(cutterIPykernelModule, "launch_ipykernel");

    PyObject *argvListObject = PyList_New(argv.size());
    for (int i = 0; i < argv.size(); i++)
    {
        QString s = argv[i];
        PyList_SetItem(argvListObject, i, PyUnicode_DecodeUTF8(s.toUtf8().constData(), s.length(), nullptr));
    }

    kernel = PyObject_CallFunction(launchFunc, "O", argvListObject);
}

NestedIPyKernel::~NestedIPyKernel()
{

}

void NestedIPyKernel::kill()
{
    auto parentThreadState = PyThreadState_Swap(threadState);
    PyObject_CallMethod(kernel, "kill", nullptr);
    PyThreadState_Swap(parentThreadState);
}
