
OBJDIR = obj

LIB_TARGETS += libshimmy
DOXYGEN_TARGETS = libshimmy

ifeq ($(WORK),1)
export PROTOBUF_VERS=2.5.0
MHPLAT=ubuntu_10_04_4
PROTOC_PATH=/shared/mh_tools/bin/protoc.sh
INCS += -I/shared/mh_tools/tools/protobuf/2.5.0/$(MHPLAT)/include
LDFLAGS += -L/shared/mh_tools/tools/protobuf/2.5.0/$(MHPLAT)/lib
endif

libshimmy_TARGET = $(OBJDIR)/libshimmy.a
libshimmy_CXXSRCS = ShimmyCommon.cc ShimmyChild.cc ShimmyParent.cc
libshimmy_DOXYFILE = Doxyfile.libshimmy Doxyfile.libshimmy-source

# all test code below this line.

LIB_TARGETS += testProto

testProto_TARGET = $(OBJDIR)/libshimmytestProto.a
testProto_PROTOSRCS = testShimmy.proto

PROG_TARGETS += testChild

testChild_TARGET = $(OBJDIR)/testChild
testChild_CXXSRCS = testChild.cc
testChild_DEFS = -DSHIMMY_PROTO_HDR=\"$(testProto_testShimmy.proto_HDR)\"
testChild_LIBS = -lprotobuf -lpthread
testChild_DEPLIBS = $(libshimmy_TARGET) $(testProto_TARGET)

PROG_TARGETS += testParent

testParent_TARGET = $(OBJDIR)/testParent
testParent_CXXSRCS = testParent.cc
testParent_DEFS = -DSHIMMY_PROTO_HDR=\"$(testProto_testShimmy.proto_HDR)\"
testParent_LIBS = -lprotobuf -lpthread
testParent_DEPLIBS = $(libshimmy_TARGET) $(testProto_TARGET)

include ../pfkutils/Makefile.inc
