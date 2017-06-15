TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.c \
    zip/adler32.c \
    zip/compress.c \
    zip/crc32.c \
    zip/deflate.c \
    zip/ioapi.c \
    zip/trees.c \
    zip/zip.c \
    zip/zutil.c

QMAKE_CFLAGS += -std=c99

HEADERS += \
    zip/crc32.h \
    zip/crypt.h \
    zip/deflate.h \
    zip/ioapi.h \
    zip/trees.h \
    zip/zconf.h \
    zip/zip.h \
    zip/zlib.h \
    zip/zutil.h
