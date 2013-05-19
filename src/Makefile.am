bin_PROGRAMS = camtickler

camtickler_SOURCES = main.cpp
camtickler_SOURCES += maygion-mips.cpp
camtickler_SOURCES += network.cpp

EXTRA_camtickler_SOURCES = main.hpp
EXTRA_camtickler_SOURCES += device-interface.hpp
EXTRA_camtickler_SOURCES += maygion-mips.hpp
EXTRA_camtickler_SOURCES += network.hpp

WARNINGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS  = $(BOOST_CPPFLAGS)
AM_CPPFLAGS += $(WARNINGS)

AM_LDFLAGS  = $(BOOST_SYSTEM_LIBS)
AM_LDFLAGS += $(BOOST_PROGRAM_OPTIONS_LIBS)
AM_LDFLAGS += $(BOOST_ASIO_LIBS)
AM_LDFLAGS += $(BOOST_REGEX_LIBS)
AM_LDFLAGS += $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)
