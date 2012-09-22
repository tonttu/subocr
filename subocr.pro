SOURCES += convert.cpp \
    main_window.cpp \
    line_splitter.cpp \
    char_scanner.cpp

HEADERS += \
    main_window.hpp \
    line_splitter.hpp \
    char_scanner.hpp

FORMS += \
    main_window.ui

QMAKE_CXXFLAGS += -std=c++0x
