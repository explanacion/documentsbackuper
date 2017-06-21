TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.c \
    zip/zip.c \
    zip/crypt.c \
    zip/crc32.c \
    zip/ioapi.c \
    zip/deflate.c \
    zip/zutil.c \
    zip/adler32.c \
    zip/trees.c \
    zip/compress.c

QMAKE_CFLAGS += -std=c99

HEADERS += \
    zip/zip.h \
    zip/crypt.h \
    zip/zlib.h \
    zip/ioapi.h \
    zip/deflate.h \
    zip/zutil.h
