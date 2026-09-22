# ADD TO EACH PATH $$PWD VARIABLE!!!!!!
# This need for corect working file translations.pro

SOURCES += \
    $$PWD/measurements.cpp \
    $$PWD/measurements_old_format_converter.cpp \
    $$PWD/svg_generator.cpp \
    $$PWD/vlabeltemplate.cpp

*msvc*:SOURCES += $$PWD/stable.cpp

HEADERS += \
    $$PWD/measurements.h \
    $$PWD/measurements_old_format_converter.h \
    $$PWD/stable.h \
    $$PWD/svg_generator.h \
    $$PWD/vlabeltemplate.h
