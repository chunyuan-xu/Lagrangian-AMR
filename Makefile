# AMR Solver Makefile
# 环境: MSYS2 UCRT64 + MS-MPI

CXX       := C:/msys64/ucrt64/bin/g++.exe
CXXFLAGS  := -O2 -g -Wall -std=c++14

SRCDIR    := src
BINDIR    := bin
OBJDIR    := build

# p4est 安装路径
P4EST_INC  := third_party/p4est/build/local/include
P4EST_LIB  := third_party/p4est/build/local/lib

# MS-MPI SDK (安装在C:\Program Files (x86)\Microsoft SDKs\MPI)
MSMPI_INC  := "C:/Program Files (x86)/Microsoft SDKs/MPI/Include"

CPPFLAGS  += -I$(SRCDIR) -I$(P4EST_INC) -I$(MSMPI_INC) -IC:/msys64/ucrt64/include
LDFLAGS   += -L$(P4EST_LIB) -LC:/msys64/ucrt64/lib
LIBS      := -lp4est -lsc -lz -lmsmpi -lws2_32

ifeq ($(OS),Windows_NT)
  EXEEXT := .exe
else
  EXEEXT :=
endif

OBJS := $(OBJDIR)/main.o $(OBJDIR)/alg.o
EXE  := $(BINDIR)/AMR_Solver$(EXEEXT)

.PHONY: all p4est run clean cleanall

all: p4est $(EXE)

# 首次使用需编译 p4est
p4est:
	@if [ ! -f "$(P4EST_LIB)/libp4est.a" ]; then \
		echo "=== Building p4est from third_party ==="; \
		export PATH="/ucrt64/bin:$$PATH" && \
		cd third_party/p4est && \
		cmake -B build -G Ninja -Dmpi=ON -DCMAKE_BUILD_TYPE=Release && \
		cmake --build build && \
		cmake --install build --prefix build/local; \
	fi

$(BINDIR) $(OBJDIR):
	@mkdir -p $@

$(EXE): $(OBJS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJDIR)/main.o: $(SRCDIR)/main.cpp $(SRCDIR)/alg.h $(SRCDIR)/defines.h $(SRCDIR)/variable.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJDIR)/alg.o: $(SRCDIR)/alg.cpp $(SRCDIR)/alg.h $(SRCDIR)/defines.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

run: $(EXE)
	"$(PROGRAMFILES)/Microsoft MPI/Bin/mpiexec" -n 1 $(EXE)

# 多进程运行: make run-mpi NP=4
run-mpi: $(EXE)
	"$(PROGRAMFILES)/Microsoft MPI/Bin/mpiexec" -n $(NP) $(EXE)

clean:
	$(RM) -rf $(OBJDIR) $(BINDIR)

cleanall: clean
	$(RM) -rf third_party/p4est/build
