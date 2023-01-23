CXX = g++
CXXFLAGS = -std=c++11
MAIN = timecodexml2webvtt
SRCS = timecodexml2webvtt.cpp tfsxml.c TimeCode.cpp
CPPFLAGS =
LDFLAGS =
LDLIBS =

.PHONY: all clean

all: $(MAIN)

$(MAIN): $(SRCS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(MAIN) $(SRCS) $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) *.o *~ $(MAIN)
