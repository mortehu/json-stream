lib_LTLIBRARIES = libjson-stream.la
include_HEADERS = json-stream.h 
check_PROGRAMS = json-stream_test

TESTS = $(check_PROGRAMS)

AM_CFLAGS = -Wall

include $(srcdir)/third_party/gtest/Makefile.am

libjson_stream_la_SOURCES = json-stream.c 

json_stream_test_SOURCES = json-stream_test.cc
json_stream_test_LDADD = \
  libjson-stream.la \
  third_party/gtest/libgtest.a \
  -lpthread
