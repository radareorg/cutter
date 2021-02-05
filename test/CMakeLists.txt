find_package(Qt5 REQUIRED COMPONENTS Test)

set(TEST_HEADER_FILES
        AutoTest.h
        CutterTest.h)

set(TEST_SOURCE_FILES
        main.cpp
        CutterTest.cpp
        TestHexdumpWidget.cpp)

add_executable(CutterTest ${TEST_SOURCE_FILES} ${TEST_HEADER_FILES})
target_link_libraries(CutterTest CutterLib Qt5::Test)
