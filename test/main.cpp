
#include "AutoTest.h"

#include "CutterApplication.h"

int main(int argc, char *argv[])
{
    int argcApp = 1;
    char *argvApp[] = { strdup("./Cutter") };
    CutterApplication app(argcApp, argvApp, true);

    return AutoTest::run(argc, argv);
}

