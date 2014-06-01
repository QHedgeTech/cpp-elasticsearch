cache()
TEMPLATE=app
CONFIG+=console
CONFIG-=app_bundle
CONFIG-=qt
CONFIG+=c++11

DEFINES+= VERBOSE=1
INCLUDEPATH += ../src
OBJECTS_DIR = build

CONFIG(debug, debug|release){
    QMAKE_CXXFLAGS += -Wextra -pedantic -ansi
    #QMAKE_CXXFLAGS += -Werror -Wfatal-errors
    message(made in mode debug)
}

CONFIG(release, debug|release){
    DEFINES += NDEBUG
    QMAKE_CXXFLAGS += -O3
    message(made in mode release)
}

linux-g++ {
    message(Compile with GCC linux)
}

linux-clang {
    message(Compile with GCC linux)
}


macx-g++ {
    QMAKE_CXX=/usr/local/bin/g++-4.9
    QMAKE_LINK=/usr/local/bin/g++-4.9

    # Missing include with gcc on MacOs
    INCLUDEPATH += /usr/include
    message(Compile with GCC MacOs $$QMAKE_CXX)
}

macx-clang {
    message(Compile with Clang MacOs $$QMAKE_CXX)
}

HEADERS += \
    ../src/http/http.h \
    ../src/elasticsearch/elasticsearch.h \
    ../src/json/json.h

SOURCES += main.cpp \
    ../src/http/http.cpp \
    ../src/elasticsearch/elasticsearch.cpp \
    ../src/json/json.cpp

