VERSION = 0.0.1
os2: {
DEFINES += 'VERSION_NUMBER=\'"0.0.1"\''
} else: {
  DEFINES += 'VERSION_NUMBER=\\\"$${VERSION}\\\"'
}


useclang{
    message ("Clang enabled")
    QMAKE_CC=clang
    QMAKE_CXX=clang
    QMAKE_CXXFLAGS += -std=c++11
}


SOURCES += wavylon.cpp \
    main.cpp \
    floatbuffer.cpp \
    envelope.cpp \
    utils.cpp \
    fman.cpp \
    shortcuts.cpp \
    logmemo.cpp \
    tio.cpp \
    fx.cpp \
    3pass_eq.cpp \
    gui_utils.cpp \
    project.cpp \
    libretta_interpolator.cpp \
    fx-panners.cpp \
    fx-filter.cpp \
    levelmeter.cpp 
#    tinyplayer.cpp

HEADERS += wavylon.h \
    utils.h \
    floatbuffer.h\
    envelope.h \
    fman.h \
    shortcuts.h \
    logmemo.h \
    tio.h \
    fx.h \
    3pass_eq.h \
    gui_utils.h \
    project.h \
    libretta_interpolator.h \
    fx-panners.h \
    fx-filter.h \
    levelmeter.h 
#    tinyplayer.h



TEMPLATE = app

CONFIG += warn_on \
    thread \
    qt \
    debug \
    link_pkgconfig

QT += core
QT += gui

greaterThan(QT_MAJOR_VERSION, 4) {
       QT += widgets
   } else {
#QT += blah blah blah
   }

QMAKE_CXXFLAGS += -fpermissive

unix: {

      PKGCONFIG += sndfile \
                   samplerate \
                   portaudio-2.0
       }


isEmpty(PREFIX):PREFIX = /usr/local #path to install
BINDIR = $$PREFIX/bin
DATADIR = $$PREFIX

TARGET = wavylon
target.path = $$BINDIR

INSTALLS += target
RESOURCES += wavylon.qrc
TRANSLATIONS += translations/wavylon_ru.ts

DISTFILES += ChangeLog \
    COPYING \
    README \
    NEWS \
    NEWS-RU \
    AUTHORS \
    TODO \
    INSTALL \
    icons/* \
    palettes/* \
    manuals/en.html \
    manuals/ru.html \
    translations/* \
    themes/Cotton/stylesheet.css \
    themes/Plum/stylesheet.css \
    themes/Smaragd/stylesheet.css \
    themes/TEA/stylesheet.css \
    themes/Turbo/stylesheet.css \
    themes/Vegan/stylesheet.css



win32: {

        CONFIG += console 

             exists ("c:\\Qt\\Qt5.3.1\\5.3\\mingw482_32\\include\portaudio.h")
                    {
                     message ("Portaudio FOUND")
                     LIBS += -llibportaudio-2
                    }


                    exists ("c:\\Qt\\Qt5.3.1\\5.3\\mingw482_32\\include\sndfile.h")
                    {
                     message ("libsndfile FOUND")
                     LIBS += -llibsndfile-1
                    }

                    exists ("c:\\Qt\\Qt5.3.1\\5.3\\mingw482_32\\include\samplerate.h")
                    {
                     message ("libsamplerate FOUND")
                     LIBS += -llibsamplerate-0
                    }


       }
