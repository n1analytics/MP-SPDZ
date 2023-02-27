
include CONFIG

MATH = $(patsubst %.cpp,%.o,$(wildcard Math/*.cpp))

TOOLS = $(patsubst %.cpp,%.o,$(wildcard Tools/*.cpp))

NETWORK = $(patsubst %.cpp,%.o,$(wildcard Networking/*.cpp))

PROCESSOR = $(patsubst %.cpp,%.o,$(wildcard Processor/*.cpp))

FHEOBJS = $(patsubst %.cpp,%.o,$(wildcard FHEOffline/*.cpp FHE/*.cpp)) Protocols/CowGearOptions.o

GC = $(patsubst %.cpp,%.o,$(wildcard GC/*.cpp)) $(PROCESSOR)
GC_SEMI = GC/SemiPrep.o GC/square64.o GC/Semi.o

OT = $(patsubst %.cpp,%.o,$(wildcard OT/*.cpp)) $(LIBSIMPLEOT)
OT_EXE = ot.x ot-offline.x

COMMONOBJS = $(MATH) $(TOOLS) $(NETWORK) GC/square64.o Processor/OnlineOptions.o Processor/BaseMachine.o Processor/DataPositions.o Processor/ThreadQueues.o Processor/ThreadQueue.o
COMPLETE = $(COMMON) $(PROCESSOR) $(FHEOFFLINE) $(TINYOTOFFLINE) $(GC) $(OT)
YAO = $(patsubst %.cpp,%.o,$(wildcard Yao/*.cpp)) $(OT) BMR/Key.o
BMR = $(patsubst %.cpp,%.o,$(wildcard BMR/*.cpp BMR/network/*.cpp))
VMOBJS = $(PROCESSOR) $(COMMONOBJS) GC/square64.o GC/Instruction.o OT/OTTripleSetup.o OT/BaseOT.o $(LIBSIMPLEOT)
VM = $(MINI_OT) $(SHAREDLIB)
COMMON = $(SHAREDLIB)
TINIER =  Machines/Tinier.o $(OT)
SPDZ = Machines/SPDZ.o $(TINIER)


LIB = libSPDZ.a
SHAREDLIB = libSPDZ.so
FHEOFFLINE = libFHE.so
LIBRELEASE = librelease.a
LIBSIMPLEOT_C = deps/SimplestOT_C/ref10/libSimplestOT.a
LIBSIMPLEOT += $(LIBSIMPLEOT_C)

ifeq ($(AVX_OT), 1)
LIBSIMPLEOT_ASM = deps/SimpleOT/libsimpleot.a
LIBSIMPLEOT += $(LIBSIMPLEOT_ASM)
endif

STATIC_OTE = local/lib/liblibOTe.a
SHARED_OTE = local/lib/liblibOTe.so

ifeq ($(USE_KOS), 0)
ifeq ($(USE_SHARED_OTE), 1)
OT += $(SHARED_OTE) local/lib/libcryptoTools.so
else
OT += $(STATIC_OTE) local/lib/libcryptoTools.a
endif
endif

# used for dependency generation
OBJS = $(BMR) $(FHEOBJS) $(TINYOTOFFLINE) $(YAO) $(COMPLETE) $(patsubst %.cpp,%.o,$(wildcard Machines/*.cpp Utils/*.cpp))
DEPS := $(wildcard */*.d */*/*.d)

# never delete
.SECONDARY: $(OBJS) $(patsubst %.cpp,%.o,$(wildcard */*.cpp))


all: arithmetic binary gen_input online offline externalIO bmr ecdsa
vm: arithmetic binary

.PHONY: doc
doc:
	cd doc; $(MAKE) html

arithmetic: rep-ring rep-field shamir semi2k-party.x semi-party.x mascot sy dealer-ring-party.x
binary: rep-bin yao semi-bin-party.x tinier-party.x tiny-party.x ccd-party.x malicious-ccd-party.x real-bmr

all: overdrive she-offline
arithmetic: semi-he gear

-include $(DEPS)
include $(wildcard *.d static/*.d)

%.o: %.cpp
	$(CXX) -o $@ $< $(CFLAGS) -MMD -MP -c

online: Fake-Offline.x Server.x Player-Online.x Check-Offline.x emulate.x

offline: $(OT_EXE) Check-Offline.x mascot-offline.x cowgear-offline.x mal-shamir-offline.x

gen_input: gen_input_f2n.x gen_input_fp.x

externalIO: bankers-bonus-client.x

bmr: bmr-program-party.x bmr-program-tparty.x

real-bmr: $(patsubst Machines/%.cpp,%.x,$(wildcard Machines/*-bmr-party.cpp))

yao: yao-party.x

she-offline: Check-Offline.x spdz2-offline.x

overdrive: simple-offline.x pairwise-offline.x cnc-offline.x gear
gear: cowgear-party.x chaigear-party.x lowgear-party.x highgear-party.x
semi-he: hemi-party.x soho-party.x temi-party.x

rep-field: malicious-rep-field-party.x replicated-field-party.x ps-rep-field-party.x

rep-ring: replicated-ring-party.x brain-party.x malicious-rep-ring-party.x ps-rep-ring-party.x rep4-ring-party.x

rep-bin: replicated-bin-party.x malicious-rep-bin-party.x ps-rep-bin-party.x Fake-Offline.x

replicated: rep-field rep-ring rep-bin

spdz2k: spdz2k-party.x ot-offline.x Check-Offline-Z2k.x galois-degree.x Fake-Offline.x
mascot: mascot-party.x spdz2k mama-party.x

ifeq ($(OS), Darwin)
tldr: mac-setup
else
tldr: mpir linux-machine-setup
endif

tldr: libote
	$(MAKE) mascot-party.x
	mkdir Player-Data 2> /dev/null; true

ifeq ($(ARM), 1)
Tools/intrinsics.h: deps/simde/simde
endif

shamir: shamir-party.x malicious-shamir-party.x atlas-party.x galois-degree.x

sy: sy-rep-field-party.x sy-rep-ring-party.x sy-shamir-party.x

ecdsa: $(patsubst ECDSA/%.cpp,%.x,$(wildcard ECDSA/*-ecdsa-party.cpp)) Fake-ECDSA.x
ecdsa-static: static-dir $(patsubst ECDSA/%.cpp,static/%.x,$(wildcard ECDSA/*-ecdsa-party.cpp))

$(LIBRELEASE): Protocols/MalRepRingOptions.o $(PROCESSOR) $(COMMONOBJS) $(TINIER) $(GC)
	$(AR) -csr $@ $^

CFLAGS += -fPIC
LDLIBS += -Wl,-rpath -Wl,$(CURDIR)

$(SHAREDLIB): $(PROCESSOR) $(COMMONOBJS) GC/square64.o GC/Instruction.o
	$(CXX) $(CFLAGS) -shared -o $@ $^ $(LDLIBS)

$(FHEOFFLINE): $(FHEOBJS) $(SHAREDLIB)
	$(CXX) $(CFLAGS) -shared -o $@ $^ $(LDLIBS)

static/%.x: Machines/%.o $(LIBRELEASE) $(LIBSIMPLEOT) local/lib/libcryptoTools.a local/lib/liblibOTe.a
	$(CXX) -o $@ $(CFLAGS) $^ -Wl,-Map=$<.map -Wl,-Bstatic -static-libgcc -static-libstdc++ $(LIBRELEASE) -llibOTe -lcryptoTools $(LIBSIMPLEOT) $(BOOST) $(LDLIBS) -Wl,-Bdynamic -ldl

static/%.x: ECDSA/%.o ECDSA/P256Element.o $(VMOBJS) $(OT) $(LIBSIMPLEOT)
	$(CXX) $(CFLAGS) -o $@ $^ -Wl,-Map=$<.map -Wl,-Bstatic -static-libgcc -static-libstdc++ $(BOOST) $(LDLIBS) -Wl,-Bdynamic -ldl

static-dir:
	@ mkdir static 2> /dev/null; true

static-release: static-dir $(patsubst Machines/%.cpp, static/%.x, $(wildcard Machines/*-party.cpp)) static/emulate.x

Fake-ECDSA.x: ECDSA/Fake-ECDSA.cpp ECDSA/P256Element.o $(COMMON) Processor/PrepBase.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LDLIBS)

ot.x: $(OT) $(COMMON) Machines/OText_main.o Machines/OTMachine.o $(LIBSIMPLEOT)
	$(CXX) $(CFLAGS) -o $@ $^ $(LDLIBS)

ot-offline.x: $(OT) $(LIBSIMPLEOT) Machines/TripleMachine.o

gc-emulate.x: $(VM) GC/FakeSecret.o GC/square64.o

bmr-%.x: $(BMR) $(VM) Machines/bmr-%.cpp $(LIBSIMPLEOT)
	$(CXX) -o $@ $(CFLAGS) $^ $(BOOST) $(LDLIBS)

%-bmr-party.x: Machines/%-bmr-party.o $(BMR) $(SHAREDLIB) $(MINI_OT)
	$(CXX) -o $@ $(CFLAGS) $^ $(BOOST) $(LDLIBS)

bmr-clean:
	-rm BMR/*.o BMR/*/*.o GC/*.o

bankers-bonus-client.x: ExternalIO/bankers-bonus-client.o $(COMMON)
	$(CXX) $(CFLAGS) -o $@ $^ $(LDLIBS)

simple-offline.x: $(FHEOFFLINE)
pairwise-offline.x: $(FHEOFFLINE)
cnc-offline.x: $(FHEOFFLINE)
spdz2-offline.x: $(FHEOFFLINE)

yao-party.x: $(YAO)
static/yao-party.x: $(YAO)

yao-clean:
	-rm Yao/*.o

galois-degree.x: Utils/galois-degree.o
	$(CXX) $(CFLAGS) -o $@ $^ $(LDLIBS)

default-prime-length.x: Utils/default-prime-length.o
	$(CXX) $(CFLAGS) -o $@ $^ $(LDLIBS)

secure.x: Utils/secure.o
	$(CXX) -o $@ $(CFLAGS) $^

Fake-Offline.x: Utils/Fake-Offline.o $(VM)
	$(CXX) -o $@ $(CFLAGS) $^ $(LDLIBS)

%.x: Utils/%.o $(COMMON)
	$(CXX) -o $@ $(CFLAGS) $^ $(LDLIBS)

%.x: Machines/%.o $(MINI_OT) $(SHAREDLIB)
	$(CXX) -o $@ $(CFLAGS) $^ $(LDLIBS) $(SHAREDLIB)

%-ecdsa-party.x: ECDSA/%-ecdsa-party.o ECDSA/P256Element.o $(VM)
	$(CXX) -o $@ $(CFLAGS) $^ $(LDLIBS)

replicated-bin-party.x: GC/square64.o
replicated-ring-party.x: GC/square64.o
replicated-field-party.x: GC/square64.o
brain-party.x: GC/square64.o
malicious-rep-bin-party.x: GC/square64.o
ps-rep-bin-party.x: GC/PostSacriBin.o
semi-bin-party.x: $(OT) $(GC_SEMI)
tiny-party.x: $(OT)
tinier-party.x: $(OT)
spdz2k-party.x: $(TINIER) $(patsubst %.cpp,%.o,$(wildcard Machines/SPDZ2*.cpp))
static/spdz2k-party.x: $(patsubst %.cpp,%.o,$(wildcard Machines/SPDZ2*.cpp))
semi-party.x: $(OT)  $(GC_SEMI)
semi2k-party.x: $(OT) $(GC_SEMI)
hemi-party.x: $(FHEOFFLINE) $(GC_SEMI) $(OT)
temi-party.x: $(FHEOFFLINE) $(GC_SEMI) $(OT)
soho-party.x: $(FHEOFFLINE) $(GC_SEMI) $(OT)
cowgear-party.x: $(FHEOFFLINE) Protocols/CowGearOptions.o $(TINIER)
chaigear-party.x: $(FHEOFFLINE) Protocols/CowGearOptions.o $(TINIER)
lowgear-party.x: $(FHEOFFLINE) $(TINIER) Protocols/CowGearOptions.o Protocols/LowGearKeyGen.o
highgear-party.x: $(FHEOFFLINE) $(TINIER) Protocols/CowGearOptions.o Protocols/HighGearKeyGen.o
atlas-party.x: GC/AtlasSecret.o
static/hemi-party.x: $(FHEOBJS)
static/temi-party.x: $(FHEOBJS)
static/soho-party.x: $(FHEOBJS)
static/cowgear-party.x: $(FHEOBJS)
static/chaigear-party.x: $(FHEOBJS)
static/lowgear-party.x: $(FHEOBJS) Protocols/CowGearOptions.o Protocols/LowGearKeyGen.o
static/highgear-party.x: $(FHEOBJS) Protocols/CowGearOptions.o Protocols/HighGearKeyGen.o
mascot-party.x: $(SPDZ)
static/mascot-party.x: $(SPDZ)
Player-Online.x: $(SPDZ)
mama-party.x: $(TINIER)
ps-rep-ring-party.x: Protocols/MalRepRingOptions.o
malicious-rep-ring-party.x: Protocols/MalRepRingOptions.o
sy-rep-ring-party.x: Protocols/MalRepRingOptions.o
rep4-ring-party.x: GC/Rep4Secret.o
no-party.x: Protocols/ShareInterface.o
semi-ecdsa-party.x: $(OT) $(LIBSIMPLEOT) $(GC_SEMI)
mascot-ecdsa-party.x: $(OT) $(LIBSIMPLEOT)
fake-spdz-ecdsa-party.x: $(OT) $(LIBSIMPLEOT)
emulate.x: GC/FakeSecret.o
semi-bmr-party.x: $(GC_SEMI) $(OT)
real-bmr-party.x: $(OT)
paper-example.x: $(VM) $(OT) $(FHEOFFLINE)
binary-example.x: $(VM) $(OT) GC/PostSacriBin.o $(GC_SEMI) GC/AtlasSecret.o
mixed-example.x: $(VM) $(OT) GC/PostSacriBin.o $(GC_SEMI) GC/AtlasSecret.o Machines/Tinier.o
l2h-example.x: $(VM) $(OT) Machines/Tinier.o
he-example.x: $(FHEOFFLINE)
mascot-offline.x: $(VM) $(TINIER)
cowgear-offline.x: $(TINIER) $(FHEOFFLINE)
static/rep-bmr-party.x: $(BMR)
static/mal-rep-bmr-party.x: $(BMR)
static/shamir-bmr-party.x: $(BMR)
static/mal-shamir-bmr-party.x: $(BMR)
static/semi-bmr-party.x: $(BMR)
static/real-bmr-party.x: $(BMR)
static/bmr-program-party.x: $(BMR)
static/no-party.x: Protocols/ShareInterface.o
Test/failure.x: Protocols/MalRepRingOptions.o

ifeq ($(AVX_OT), 1)
$(LIBSIMPLEOT_ASM): deps/SimpleOT/Makefile
	$(MAKE) -C deps/SimpleOT

OT/BaseOT.o: deps/SimpleOT/Makefile

deps/SimpleOT/Makefile:
	git submodule update --init deps/SimpleOT || git clone https://github.com/mkskeller/SimpleOT deps/SimpleOT
endif

$(LIBSIMPLEOT_C): deps/SimplestOT_C/ref10/Makefile
	$(MAKE) -C deps/SimplestOT_C/ref10

OT/BaseOT.o: deps/SimplestOT_C/ref10/Makefile

deps/SimplestOT_C/ref10/Makefile:
	git submodule update --init deps/SimplestOT_C || git clone https://github.com/mkskeller/SimplestOT_C deps/SimplestOT_C
	cd deps/SimplestOT_C/ref10; PATH=$(CURDIR)/local/bin:$(PATH) cmake .

.PHONY: Programs/Circuits
Programs/Circuits:
	git submodule update --init Programs/Circuits

.PHONY: mpir-setup mpir-global
mpir-setup: deps/mpir/Makefile
deps/mpir/Makefile:
	git submodule update --init deps/mpir || git clone https://github.com/wbhart/mpir deps/mpir
	cd deps/mpir; \
	autoreconf -i; \
	autoreconf -i
	- $(MAKE) -C deps/mpir clean

mpir-global: mpir-setup
	cd deps/mpir; \
	./configure --enable-cxx;
	$(MAKE) -C deps/mpir
	sudo $(MAKE) -C deps/mpir install

mpir: local/lib/libmpirxx.so
local/lib/libmpirxx.so: deps/mpir/Makefile
	cd deps/mpir; \
	./configure --enable-cxx --prefix=$(CURDIR)/local
	$(MAKE) -C deps/mpir install

deps/libOTe/libOTe:
	git submodule update --init --recursive deps/libOTe || git clone --recurse-submodules https://github.com/mkskeller/softspoken-implementation deps/libOTe
boost: deps/libOTe/libOTe
	cd deps/libOTe; \
	python3 build.py --setup --boost --install=$(CURDIR)/local

OTE_OPTS = -DENABLE_SOFTSPOKEN_OT=ON -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_INSTALL_LIBDIR=lib

ifeq ($(USE_SHARED_OTE), 1)
OTE = $(SHARED_OTE)
else
OTE = $(STATIC_OTE)
endif

libote:
	rm $(STATIC_OTE) $(SHARED_OTE)* 2>/dev/null; true
	$(MAKE) $(OTE)

local/lib/libcryptoTools.a: $(STATIC_OTE)
local/lib/libcryptoTools.so: $(SHARED_OTE)
OT/OTExtensionWithMatrix.o: $(OTE)

ifeq ($(ARM), 1)
local/lib/liblibOTe.a: deps/libOTe/libOTe
	cd deps/libOTe; \
	PATH="$(CURDIR)/local/bin:$(PATH)" python3 build.py --install=$(CURDIR)/local -- -DBUILD_SHARED_LIBS=0 -DENABLE_AVX=OFF -DENABLE_SSE=OFF $(OTE_OPTS)
else
local/lib/liblibOTe.a: deps/libOTe/libOTe
	cd deps/libOTe; \
	PATH="$(CURDIR)/local/bin:$(PATH)" python3 build.py --install=$(CURDIR)/local -- -DBUILD_SHARED_LIBS=0 $(OTE_OPTS)
endif

$(SHARED_OTE): deps/libOTe/libOTe
	cd deps/libOTe; \
	python3 build.py --install=$(CURDIR)/local -- -DBUILD_SHARED_LIBS=1 $(OTE_OPTS)

local/lib/libdots.so:
	$(MAKE) -C deps/libdots libdots.so
	cp deps/libdots/libdots.so $(CURDIR)/local/lib
	cp -R deps/libdots/include/. $(CURDIR)/local/include

cmake:
	wget https://github.com/Kitware/CMake/releases/download/v3.24.1/cmake-3.24.1.tar.gz
	tar xzvf cmake-3.24.1.tar.gz
	cd cmake-3.24.1; \
	./bootstrap --parallel=8 --prefix=../local && make && make install

mac-setup: mac-machine-setup
	brew install openssl boost libsodium mpir yasm ntl cmake
	-echo MY_CFLAGS += -I/usr/local/opt/openssl/include -I`brew --prefix`/opt/openssl/include -I`brew --prefix`/include >> CONFIG.mine
	-echo MY_LDLIBS += -L/usr/local/opt/openssl/lib -L`brew --prefix`/lib -L`brew --prefix`/opt/openssl/lib >> CONFIG.mine
#	-echo USE_NTL = 1 >> CONFIG.mine

ifeq ($(ARM), 1)
mac-machine-setup:
	-echo ARCH = >> CONFIG.mine
linux-machine-setup:
	-echo ARCH = -march=armv8.2-a+crypto >> CONFIG.mine
else
mac-machine-setup:
linux-machine-setup:
endif

deps/simde/simde:
	git submodule update --init deps/simde || git clone https://github.com/simd-everywhere/simde deps/simde

clean:
	-rm -f */*.o *.o */*.d *.d *.x core.* *.a gmon.out */*/*.o static/*.x *.so
