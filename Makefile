
OBJDIR = obj

LIB_TARGETS += shimproto

shimproto_TARGET = $(OBJDIR)/libshimproto.a
shimproto_PROTOSRCS = shimmy.proto
shimproto_CXXSRCS = ShimmyChild.cc ShimmyParent.cc

include ../pfkutils/Makefile.inc
