
OBJDIR = obj

LIB_TARGETS += shimproto

ifeq ($(WORK),1)
export PROTOBUF_VERS=2.5.0
MHPLAT=ubuntu_10_04_4
PROTOC_PATH=/shared/mh_tools/bin/protoc.sh
INCS += -I/shared/mh_tools/tools/protobuf/2.5.0/$(MHPLAT)/include
LDFLAGS += -L/shared/mh_tools/tools/protobuf/2.5.0/$(MHPLAT)/lib
endif

shimproto_TARGET = $(OBJDIR)/libshimproto.a
shimproto_PROTOSRCS = shimmy.proto
shimproto_CXXSRCS = ShimmyChild.cc ShimmyParent.cc ShimmyCommon.cc

PROG_TARGETS += testChild

testChild_TARGET = $(OBJDIR)/testChild
testChild_CXXSRCS = testChild.cc
testChild_DEFS = -DSHIMMY_PROTO_HDR=\"$(shimproto_shimmy.proto_HDR)\"
testChild_LIBS = -lprotobuf -lpthread
testChild_DEPLIBS = $(shimproto_TARGET)

PROG_TARGETS += testParent

testParent_TARGET = $(OBJDIR)/testParent
testParent_CXXSRCS = testParent.cc
testParent_DEFS = -DSHIMMY_PROTO_HDR=\"$(shimproto_shimmy.proto_HDR)\"
testParent_LIBS = -lprotobuf -lpthread
testParent_DEPLIBS = $(shimproto_TARGET)

include ../pfkutils/Makefile.inc
