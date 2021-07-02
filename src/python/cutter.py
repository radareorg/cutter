import json
from _cutter import *

try:
    from CutterBindings import *

    def core():
        return CutterCore.instance()

    def app():
        # see https://forums.autodesk.com/t5/3ds-max-programming/qapplication-instance-returns-qcoreapplication-instance/td-p/9035295
        import shiboken2
        from PySide2 import QtCore
        coreapp = QtCore.QCoreApplication.instance()
        ptr = shiboken2.getCppPointer(coreapp)[0]
        return shiboken2.wrapInstance(ptr, CutterApplication)
except ImportError:
    pass


def cmdj(command):
    """Execute a JSON command and return the result as a dictionary"""
    return json.loads(cmd(command))


