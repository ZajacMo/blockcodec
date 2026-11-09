QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17



# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
    compresswindow.cpp \
    helpwindow.cpp \
    introducewindow.cpp \
    main.cpp \
    mainwindow.cpp \
    uncompresswindow.cpp\
    Bits.cpp\
    Block.cpp\
    RBlock.cpp\
    Compress_gray.cpp

HEADERS += \
    compresswindow.h \
    helpwindow.h \
    introducewindow.h \
    mainwindow.h \
    uncompresswindow.h\
    Bits.h\
    Block.h\
    RBlock.h\
    Compress_gray.h\
    operatorsignals.h

FORMS += \
    compresswindow.ui \
    helpwindow.ui \
    introducewindow.ui \
    mainwindow.ui \
    uncompresswindow.ui



# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32:CONFIG(release, debug|release): LIBS += -L'C:/Program Files/opencv/build/x64/vc16/lib/' -lopencv_world4100
else:win32:CONFIG(debug, debug|release): LIBS += -L'C:/Program Files/opencv/build/x64/vc16/lib/' -lopencv_world4100d

INCLUDEPATH += 'C:/Program Files/opencv/build/include'
DEPENDPATH += 'C:/Program Files/opencv/build/include'

RESOURCES += \
    resourses.qrc

