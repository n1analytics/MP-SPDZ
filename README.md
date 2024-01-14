# Multi-Protocol SPDZ [![Documentation Status](https://readthedocs.org/projects/mp-spdz/badge/?version=latest)](https://mp-spdz.readthedocs.io/en/latest/?badge=latest) [![Build Status](https://dev.azure.com/data61/MP-SPDZ/_apis/build/status/data61.MP-SPDZ?branchName=master)](https://dev.azure.com/data61/MP-SPDZ/_build/latest?definitionId=7&branchName=master) [![Gitter](https://badges.gitter.im/MP-SPDZ/community.svg)](https://gitter.im/MP-SPDZ/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

This is a software to benchmark various secure multi-party computation
(MPC) protocols in a variety of security models such as honest and
dishonest majority, semi-honest/passive and malicious/active
corruption. The underlying technologies span secret sharing,
homomorphic encryption, and garbled circuits.

#### Contact

[Filing an issue on GitHub](https://github.com/data61/MP-SPDZ/issues)
is the preferred way of contacting
us, but you can also write an email to mp-spdz@googlegroups.com
([archive](https://groups.google.com/forum/#!forum/mp-spdz)). Before
reporting a problem, please check against the list of [known
issues and possible
solutions](https://mp-spdz.readthedocs.io/en/latest/troubleshooting.html).

##### Filing Issues

Please file complete code examples because it's usually not possible
to reproduce problems from incomplete code, and please include which
protocol you have used (if applicable) because there are considerable
differences between the various protocols.

#### Frequently Asked Questions

[The documentation](https://mp-spdz.readthedocs.io/en/latest) contains
section on a number of [frequently asked
topics](https://mp-spdz.readthedocs.io/en/latest/troubleshooting.html)
as well as information on how to solve common issues.

#### TL;DR (Binary Distribution on Linux or Source Distribution on macOS)

This requires either a Linux distribution originally released 2014 or
later (glibc 2.17) or macOS High Sierra or later as well as Python 3
and basic command-line utilities.

Download and unpack the
[distribution](https://github.com/data61/MP-SPDZ/releases),
then execute the following from
the top folder:

```
Scripts/tldr.sh
echo 1 2 3 4 > Player-Data/Input-P0-0
echo 1 2 3 4 > Player-Data/Input-P1-0
Scripts/compile-run.py -E mascot tutorial
```

This runs [the tutorial](Programs/Source/tutorial.mpc) with two
parties and malicious security.

#### TL;DR (Source Distribution)

On Linux, this requires a working toolchain and [all
requirements](#requirements). On Ubuntu, the following might suffice:
```
sudo apt-get install automake build-essential clang cmake git libboost-dev libboost-thread-dev libgmp-dev libntl-dev libsodium-dev libssl-dev libtool python3
```
On MacOS, this requires [brew](https://brew.sh) to be installed,
which will be used for all dependencies.
It will execute [the
tutorial](Programs/Source/tutorial.mpc) with two parties and malicious
security.

```
make setup
echo 1 2 3 4 > Player-Data/Input-P0-0
echo 1 2 3 4 > Player-Data/Input-P1-0
Scripts/compile-run.py -E mascot tutorial
```

On strong enough hardware setups (several cores and GB of RAM), you
can speed up the last step by running `make -j8 mascot-party.x` beforehand.

#### TL;DR (Docker)
Build a docker image for `mascot-party.x`:

```
docker build --tag mpspdz:mascot-party --build-arg src=tutorial --build-arg machine=mascot-party.x .
```

Run the [the tutorial](Programs/Source/tutorial.mpc):

```
docker run --rm -it mpspdz:mascot-party ./Scripts/mascot.sh tutorial
```

See the [`Dockerfile`](./Dockerfile) for examples of how it can be used.

#### Preface

The primary aim of this software is to run the same computation in
various protocols in order to compare the performance. All protocols
in the matrix below are fully implemented. However, this does not mean
that the software has undergone a security review as should be done
with critical production code.

#### Protocols

The following table lists all protocols that are fully supported.

| Security model | Mod prime / GF(2^n) | Mod 2^k | Bin. SS | Garbling |
| --- | --- | --- | --- | --- |
| Malicious, dishonest majority | [MASCOT / LowGear / HighGear](#secret-sharing) | [SPDZ2k](#secret-sharing) | [Tiny / Tinier](#secret-sharing) | [BMR](#bmr) |
| Covert, dishonest majority | [CowGear / ChaiGear](#secret-sharing) | N/A | N/A | N/A |
| Semi-honest, dishonest majority | [Semi / Hemi / Temi / Soho](#secret-sharing) | [Semi2k](#secret-sharing) | [SemiBin](#secret-sharing) | [Yao's GC](#yaos-garbled-circuits) / [BMR](#bmr) |
| Malicious, honest majority | [Shamir / Rep3 / PS / SY](#honest-majority) | [Brain / Rep3 / PS / SY](#honest-majority) | [Rep3 / CCD / PS](#honest-majority) | [BMR](#bmr) |
| Semi-honest, honest majority | [Shamir / ATLAS / Rep3](#honest-majority) | [Rep3](#honest-majority) | [Rep3 / CCD](#honest-majority) | [BMR](#bmr) |
| Malicious, honest supermajority | [Rep4](#honest-majority) | [Rep4](#honest-majority) | [Rep4](#honest-majority) | N/A |
| Semi-honest, dealer | [Dealer](#dealer-model) | [Dealer](#dealer-model) | [Dealer](#dealer-model) | N/A |

Modulo prime and modulo 2^k are the two settings that allow
integer-like computation. For k = 64, the latter corresponds to the
computation available on the widely used 64-bit processors.  GF(2^n)
denotes Galois extension fields of order 2^n, which are different to
computation modulo 2^n. In particular, every element has an inverse,
which is not the case modulo 2^n. See [this
article](https://en.wikipedia.org/wiki/Finite_field) for an
introduction. Modulo prime and GF(2^n) are lumped together because the
protocols are very similar due to the mathematical properties.

Bin. SS stands for binary secret sharing, that is secret sharing
modulo two. In some settings, this requires specific protocols as some
protocols require the domain size to be larger than two. In other
settings, the protocol is the same mathematically speaking, but a
specific implementation allows for optimizations such as using the
inherent parallelism of bit-wise operations on machine words.

A security model specifies how many parties are "allowed" to misbehave
in what sense. Malicious means that not following the protocol will at
least be detected while semi-honest means that even corrupted parties
are assumed to follow the protocol.
See [this paper](https://eprint.iacr.org/2020/300) for an explanation
of the various security models and a high-level introduction to
multi-party computation.

##### Finding the most efficient protocol

Lower security requirements generally allow for more efficient
protocols. Within the same security model (line in the table above),
there are a few things to consider:

- Computation domain: Arithmetic protocols (modulo prime or power of
  two) are preferable for many applications because they offer integer
  addition and multiplication at low cost. However, binary circuits
  might be a better option if there is very little integer
  computation. [See below](#finding-the-most-efficient-variant) to
  find the most efficient mixed-circuit variant.  Furthermore, local
  computation modulo a power of two is cheaper, but MP-SPDZ does not
  offer this domain with homomorphic encryption.

- Secret sharing vs garbled circuits: Computation using secret sharing
  requires a number of communication rounds that grows depending on
  the computation, which is not the case for garbled
  circuits. However, the cost of integer computation as a binary
  circuit often offset this. MP-SPDZ only offers garbled circuit
  with binary computation.

- Underlying technology for dishonest majority: While secret sharing
  alone suffice honest-majority computation, dishonest majority
  requires either homomorphic encryption (HE) or oblivious transfer
  (OT). The two options offer a computation-communication trade-off:
  While OT is easier to compute, HE requires less
  communication. Furthermore, the latter requires a certain of
  batching to be efficient, which makes OT preferable for smaller
  tasks.

- Malicious, honest-majority three-party computation: A number of
  protocols are available for this setting, but SY/SPDZ-wise is the
  most efficient one for a number of reasons: It requires the lowest
  communication, and it is the only one offering constant-communication
  dot products.

- Fixed-point multiplication: Three- and four-party replicated secret
  sharing as well semi-honest full-threshold protocols allow a special
  probabilistic truncation protocol (see [Dalskov et
  al.](https://eprint.iacr.org/2019/131) and [Dalskov et
  al.](https://eprint.iacr.org/2020/1330)). You can activate it by
  adding `program.use_trunc_pr = True` at the beginning of your
  high-level program.

- Larger number of parties: ATLAS scales better than the plain Shamir
  protocol, and Temi scale better than Hemi or Semi.

- Minor variants: Some command-line options change aspects of the
  protocols such as:
    - `--bucket-size`: In some malicious binary computation and
      malicious edaBit generation, a smaller bucket size allows
      preprocessing in smaller batches at a higher asymptotic cost.
    - `--batch-size`: Preprocessing in smaller batches avoids generating
      too much but larger batches save communication rounds.
    - `--direct`: In dishonest-majority protocols, direct communication
      instead of star-shaped saves communication rounds at the expense
      of a quadratic amount. This might be beneficial with a small
      number of parties.
    - `--bits-from-squares`: In some protocols computing modulo a prime
      (Shamir, Rep3, SPDZ-wise), this switches from generating random
      bits via XOR of parties' inputs to generation using the root of a
      random square.

#### Paper and Citation

The design of MP-SPDZ is described in [this
paper](https://eprint.iacr.org/2020/521). If you use it for an
academic project, please cite:

```
@inproceedings{mp-spdz,
    author = {Marcel Keller},
    title = {{MP-SPDZ}: A Versatile Framework for Multi-Party Computation},
    booktitle = {Proceedings of the 2020 ACM SIGSAC Conference on
    Computer and Communications Security},
    year = {2020},
    doi = {10.1145/3372297.3417872},
    url = {https://doi.org/10.1145/3372297.3417872},
}
```

#### History

The software started out as an implementation of [the improved SPDZ
protocol](https://eprint.iacr.org/2012/642). The name SPDZ is derived
from the authors of the [original
protocol](https://eprint.iacr.org/2011/535).

This repository combines the functionality previously published in the
following repositories:
 - https://github.com/bristolcrypto/SPDZ-2
 - https://github.com/mkskeller/SPDZ-BMR-ORAM
 - https://github.com/mkskeller/SPDZ-Yao

#### Overview

For the actual computation, the software implements a virtual machine
that executes programs in a specific bytecode. Such code can be
generated from high-level Python code using a compiler that optimizes
the computation with a particular focus on minimizing the number of
communication rounds (for protocols based on secret sharing) or on
AES-NI pipelining (for garbled circuits).

The software uses two different bytecode sets, one for
arithmetic circuits and one for boolean circuits. The high-level code
slightly differs between the two variants, but we aim to keep these
differences a at minimum.

In the section on computation we will explain how to compile a
high-level program for the various computation domains and then how to
run it with different protocols.

The section on offline phases will explain how to benchmark the
offline phases required for the SPDZ protocol. Running the online
phase outputs the amount of offline material required, which allows to
compute the preprocessing time for a particular computation.

#### Requirements

 - GCC 5 or later (tested with up to 11) or LLVM/clang 6 or later
   (tested with up to 14). The default is to use clang because it performs
   better. Note that GCC 5/6 and clang 9 don't support libOTe, so you
   need to deactivate its use for these compilers (see the next
   section).
 - For protocols using oblivious transfer, libOTe with [the necessary
   patches](https://github.com/mkskeller/softspoken-implementation)
   but without SimplestOT. The easiest way is to run `make libote`,
   which will install it as needed in a subdirectory. libOTe requires
   CMake of version at least 3.15, which is not available by default
   on older systems such as Ubuntu 18.04. You can run `make cmake` to
   install it locally.
   libOTe also requires boost of version at least 1.75, which is not
   available by default on relatively recent systems such as Ubuntu
   22.04. You can install it locally by running `make boost`.
 - GMP library, compiled with C++ support (use flag `--enable-cxx`
   when running configure). Tested against 6.2.1 as supplied by
   Ubuntu.
 - libsodium library, tested against 1.0.18
 - OpenSSL, tested against 3.0.2
 - Boost.Asio with SSL support (`libboost-dev` on Ubuntu), tested against 1.81
 - Boost.Thread for BMR (`libboost-thread-dev` on Ubuntu), tested against 1.81
 - x86 or ARM 64-bit CPU (the latter tested with AWS Gravitron and
   Apple Silicon)
 - Python 3.5 or later
 - NTL library for homomorphic encryption (optional; tested with NTL 11.5.1)
 - If using macOS, Sierra or later
 - Windows/VirtualBox: see [this
   issue](https://github.com/data61/MP-SPDZ/issues/557) for a discussion

#### Compilation

1. Edit `CONFIG` or `CONFIG.mine` to your needs:

    - On x86, the binaries are optimized for the CPU you are compiling
      on. For all optimizations on x86, a CPU supporting AES-NI,
      PCLMUL, AVX2, BMI2, ADX is required. This includes mainstream
      processors released 2014 or later. If you intend to run on a
      different CPU than compiling, you might need to change the `ARCH`
      variable in `CONFIG` or `CONFIG.mine` to `-march=<cpu>`. See the
      [GCC
      documentation](https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html)
      for the possible options.
      To run on CPUs without AVX2 (CPUs from before 2014), you should
      also add `AVX_OT = 0` to `CONFIG.mine`.
    - For optimal results on Linux on ARM, add `ARCH = -march=armv8.2-a+crypto`
      to `CONFIG.mine`. This enables the hardware support for AES. See the [GCC
      documentation](https://gcc.gnu.org/onlinedocs/gcc/AArch64-Options.html#AArch64-Options) on available options.
    - To benchmark online-only protocols or Overdrive offline phases, add the following line at the top: `MY_CFLAGS = -DINSECURE`
    - `PREP_DIR` should point to a local, unversioned directory to store preprocessing data (the default is `Player-Data` in the current directory).
    - `SSL_DIR` should point to a local, unversioned directory to store ssl keys (the default is `Player-Data` in the current directory).
    - For homomorphic encryption with GF(2^40), set `USE_NTL = 1`.
    - To use KOS instead of SoftSpokenOT, add `USE_KOS = 1` and
      `SECURE = -DINSECURE` to `CONFIG.mine`. This is necessary with
      GCC 5 and 6 because these compilers don't support the C++
      standard used by libOTe.
    - On macOS, there have been issues with non-system compilers. Add
      `CXX = /usr/bin/g++` to fix them.

2. Run `make` to compile all the software (use the flag `-j` for faster
   compilation using multiple threads). See below on how to compile specific
   parts only. Remember to run `make clean` first after changing `CONFIG`
   or `CONFIG.mine`.

# Running Computation

See `Programs/Source/` for some example MPC programs, in particular
`tutorial.mpc`. Furthermore, [Read the
Docs](https://mp-spdz.readthedocs.io/en/latest/) hosts a more
detailed reference of all aspects of MP-SPDZ.

There are three ways of running computation:

1. Separate compilation and execution. This is the default in the
   further documentation. It allows to run the same program several
   times while only compiling once, for example:

   ```
   ./compile.py <program> <argument>
   Scripts/mascot.sh <program>-<argument> [<runtime-arg>...]
   Scripts/mascot.sh <program>-<argument> [<runtime-arg>...]
   ```

2. One-command local execution. This compiles the program and the
   virtual machine if necessary before executing it locally with the
   given protocol. The name of the protocols correspond to the script
   names below (without the `.sh`). Furthermore, some
   protocol-specific optimization options are automatically used as
   well as required options.

   ```
   Scripts/compile-run.py -E mascot <program> <argument> -- [<runtime-arg>...]
   ```

3. One-command remote execution. This compiles the program and the
   virtual machine if necessary before uploading them together with
   all necessary input and certificate files via SSH.

   ```
   Scripts/compile-run.py -H HOSTS -E mascot <program> <argument> -- [<runtime-arg>...]
   ```

   `HOSTS` has to be a text file in the following format:

   ```
   [<user>@]<host0>[/<path>]
   [<user>@]<host1>[/<path>]
   ...
   ```

   If <path> does not start with `/` (only one `/` after the
   hostname), the path with be relative to the home directory of the
   user. Otherwise (`//` after the hostname it will be relative to the
   root directory.

   It is assumed that the SSH login is possible without password.

Even with the integrated execution it is important to keep in mind
that there are two different phases, the compilation and the run-time
phase. Any secret data is only available in the second phase, when the
Python compilation has concluded. Therefore, the types like `sint` and
`sfix` are mere placeholders for data to be used later, and they don't
contain any shares. See also [the
documentation](https://mp-spdz.readthedocs.io/en/latest/compilation.html#compilation-vs-run-time)
for what this means when using Python data structures and Python
language features.


### Compiling high-level programs

There are three computation domains, and the high-level programs have
to be compiled accordingly.

#### Arithmetic modulo a prime

```./compile.py [-F <integer bit length>] [-P <prime>] <program>```

The integer bit length defaults to 64, and the prime defaults to none
given. If a prime is given, it has to be at least two bits longer than
the integer length. Note that `-P` is optional, and it involves
algorithms that are more expensive while allowing for a wider range of
integer lengths.

Note that in this context integers do not wrap around according to the
bit integer bit length but the length is used for non-linear
computations such as comparison.
Overflow in secret integers might have security implications if no
concrete prime is given.

The parameters given together with the computation mandate some
restriction on the prime modulus, either an exact value or a minimum
length. The latter is roughly the integer length plus 40 (default
security parameter). The restrictions are communicated to the virtual
machines, which will use an appropriate prime if they have been
compiled accordingly. By default, they are compiled for prime bit
lengths up to 256. For larger primes, you will have to compile with
`MOD = -DGFP_MOD_SZ=<number of limbs>` in `CONFIG.mine` where the
number of limbs is the the prime length divided by 64 rounded up.

The precision for fixed- and floating-point computation are not
affected by the integer bit length but can be set in the code
directly. For fixed-point computation this is done via
`sfix.set_precision()`.

#### Arithmetic modulo 2^k

```./compile.py -R <integer bit length> <program>```

The length is communicated to the virtual machines and automatically
used if supported. By default, they support bit lengths 64, 72, and
128 (the latter except for SPDZ2k). If another length is required, use
`MOD = -DRING_SIZE=<bit length>` in `CONFIG.mine`.

#### Binary circuits

```./compile.py -B <integer bit length> <program>```

The integer length can be any number up to a maximum depending on the
protocol. All protocols support at least 64-bit integers.

Fixed-point numbers (`sfix`) always use 16/16-bit precision by default in
binary circuits. This can be changed with `sfix.set_precision`. See
[the tutorial](Programs/Source/tutorial.mpc).

If you would like to use integers of various precisions, you can use
`sbitint.get_type(n)` to get a type for `n`-bit arithmetic.

#### Mixed circuits

MP-SPDZ allows to mix computation between arithmetic and binary
secret sharing in the same security model. In the compiler, this is
used to switch from arithmetic to binary computation for certain
non-linear functions such as
comparison, bit decomposition, truncation, and modulo power of two,
which are use for fixed- and floating-point operations. There are
several ways of achieving this as described below.

##### Classic daBits

You can activate this by adding `-X` when compiling arithmetic
circuits, that is
```./compile.py -X [-F <integer bit length>] <program>```
for computation modulo a prime and
```./compile.py -X -R <integer bit length> <program>```
for computation modulo 2^k.

Internally, this uses daBits described by [Rotaru and
Wood](https://eprint.iacr.org/2019/207), that is secret random bits
shared in different domains. Some security models allow direct
conversion of random bits from arithmetic to binary while others
require inputs from several parties followed by computing XOR and
checking for malicious security as described by Rotaru and Wood in
Section 4.1.

##### Extended daBits

Extended daBits were introduced by [Escudero et
al.](https://eprint.iacr.org/2020/338). You can activate them by using
`-Y` instead of `-X`. Note that this also activates classic daBits
when useful.

##### Local share conversion

This technique has been used by [Mohassel and
Rindal](https://eprint.iacr.org/2018/403) as well as [Araki et
al.](https://eprint.iacr.org/2018/762) for three parties and [Demmler
et al.](https://encrypto.de/papers/DSZ15.pdf) for two parties.
It involves locally
converting an arithmetic share to a set of binary shares, from which the
binary equivalent to the arithmetic share is reconstructed using a
binary adder. This requires additive secret sharing over a ring
without any MACs. You can activate it by using `-Z <n>` with the
compiler where `n` is the number of parties for the standard variant
and 2 for the special
variant by Mohassel and Rindal (available in Rep3 only).

##### Finding the most efficient variant

Where available, local share conversion is likely the most efficient
variant. Otherwise, edaBits likely offer an asymptotic benefit. When
using edaBits with malicious protocols, there is a trade-off between
cost per item and batch size. The lowest cost per item requires large
batches of edaBits (more than one million at once), which is only
worthwhile for accordingly large computation. This setting can be
selected by running the virtual machine with `-B 3`. For smaller
computation, try `-B 4` or `-B 5`, which set the batch size to ~10,000
and ~1,000, respectively, at a higher asymptotic cost. `-B 4` is the
default.

#### Bristol Fashion circuits

Bristol Fashion is the name of a description format of binary circuits
used by
[SCALE-MAMBA](https://github.com/KULeuven-COSIC/SCALE-MAMBA). You can
access such circuits from the high-level language if they are present
in `Programs/Circuits`. To run the AES-128 circuit provided with
SCALE-MAMBA, you can run the following:

```
make Programs/Circuits
./compile.py aes_circuit
Scripts/semi.sh aes_circuit
```

This downloads the circuit, compiles it to MP-SPDZ bytecode, and runs
it as semi-honest two-party computation 1000 times in parallel. It
should then output the AES test vector
`0x3ad77bb40d7a3660a89ecaf32466ef97`. You can run it with any other
protocol as well.

See the
[documentation](https://mp-spdz.readthedocs.io/en/latest/Compiler.html#module-Compiler.circuit)
for further examples.

#### Compiling programs directly in Python

You may prefer to not have an entirely static `.mpc` file to compile, and may want to compile based on dynamic inputs. For example, you may want to be able to compile with different sizes of input data without making a code change to the `.mpc` file. To handle this, the compiler an also be directly imported, and a function can be compiled with the following interface:

```python
# hello_world.mpc
from Compiler.library import print_ln
from Compiler.compilerLib import Compiler

compiler = Compiler()

@compiler.register_function('helloworld')
def hello_world():
    print_ln('hello world')

if __name__ == "__main__":
    compiler.compile_func()
```

You could then run this with the same args as used with `compile.py`:

```bash
python hello_world.mpc <compile args>
```

This is particularly useful if want to add new command line arguments specifically for your `.mpc` file. See [test_args.mpc](Programs/Source/test_args.mpc) for more details on this use case.

Note that when using this approach, all objects provided in the high level interface (e.g. sint, print_ln) need to be imported, because the `.mpc` file is interpreted directly by Python (instead of being read by `compile.py`.)

#### Compiling and running programs from external directories

Programs can also be edited, compiled and run from any directory with
the above basic structure. So for a source file in
`./Programs/Source/`, all MP-SPDZ scripts must be run from `./`. Any
setup scripts such as `setup-ssl.sh` script must also be run from `./`
to create the relevant data. For example:

```
MP-SPDZ$ cd ../
$ mkdir myprogs
$ cd myprogs
$ mkdir -p Programs/Source
$ vi Programs/Source/test.mpc
$ ../MP-SPDZ/compile.py test.mpc
$ ls Programs/
Bytecode  Public-Input  Schedules  Source
$ ../MP-SPDZ/Scripts/setup-ssl.sh
$ ls
Player-Data Programs
$ ../MP-SPDZ/Scripts/rep-field.sh test
```

### TensorFlow inference

MP-SPDZ supports inference with selected TensorFlow graphs, in
particular DenseNet, ResNet, and SqueezeNet as used in
[CrypTFlow](https://github.com/mpc-msri/EzPC). For example, you can
run SqueezeNet inference for ImageNet as follows:

```
git clone https://github.com/mkskeller/EzPC
cd EzPC/Athos/Networks/SqueezeNetImgNet
axel -a -n 5 -c --output ./PreTrainedModel https://github.com/avoroshilov/tf-squeezenet/raw/master/sqz_full.mat
pip3 install numpy scipy pillow>=9.1 tensorflow
python3 squeezenet_main.py --in ./SampleImages/n02109961_36.JPEG --saveTFMetadata True
python3 squeezenet_main.py --in ./SampleImages/n02109961_36.JPEG --scalingFac 12 --saveImgAndWtData True
cd ../../../..
cp EzPC/Athos/Networks/SqueezeNetImgNet/SqNetImgNet_img_input.inp Player-Data/Input-Binary-P0-0
./compile.py -R 64 tf EzPC/Athos/Networks/SqueezeNetImgNet/graphDef.bin 1 trunc_pr split
Scripts/ring.sh tf-EzPC_Athos_Networks_SqueezeNetImgNet_graphDef.bin-1-trunc_pr-split
```

This requires TensorFlow and the axel command-line utility to be
installed. It runs inference with
three-party semi-honest computation, similar to CrypTFlow's
Porthos. Replace 1 by the desired number of thread in the last two
lines. If you run with some other protocols, you will need to remove
`trunc_pr` and/or `split`. Also note that you will need to use a
CrypTFlow repository that includes the patches in
https://github.com/mkskeller/EzPC.

[The reference](https://mp-spdz.readthedocs.io/en/latest/Compiler.html#module-Compiler.ml)
contains further documentation on available layers.

### Emulation

For arithmetic circuits modulo a power of two and binary circuits, you
can emulate the computation as follows:

``` ./emulate.x <program> ```

This runs the compiled bytecode in cleartext computation.

## Dishonest majority

Some full implementations require oblivious transfer, which is
implemented as OT extension based on
https://github.com/mkskeller/SimpleOT or
https://github.com/mkskeller/SimplestOT_C, depending on whether AVX is
available.

### Secret sharing

The following table shows all programs for dishonest-majority computation using secret sharing:

| Program | Protocol | Domain | Security | Script |
| --- | --- | --- | --- | --- |
| `mascot-party.x` | [MASCOT](https://eprint.iacr.org/2016/505) | Mod prime | Malicious | `mascot.sh` |
| `mama-party.x` | MASCOT* | Mod prime | Malicious | `mama.sh` |
| `spdz2k-party.x` | [SPDZ2k](https://eprint.iacr.org/2018/482) | Mod 2^k | Malicious | `spdz2k.sh` |
| `semi-party.x` | OT-based | Mod prime | Semi-honest | `semi.sh` |
| `semi2k-party.x` | OT-based | Mod 2^k | Semi-honest | `semi2k.sh` |
| `lowgear-party.x` | [LowGear](https://eprint.iacr.org/2017/1230) | Mod prime | Malicious | `lowgear.sh` |
| `highgear-party.x` | [HighGear](https://eprint.iacr.org/2017/1230) | Mod prime | Malicious | `highgear.sh` |
| `cowgear-party.x` | Adapted [LowGear](https://eprint.iacr.org/2017/1230) | Mod prime | Covert | `cowgear.sh` |
| `chaigear-party.x` | Adapted [HighGear](https://eprint.iacr.org/2017/1230) | Mod prime | Covert | `chaigear.sh` |
| `hemi-party.x` | Semi-homomorphic encryption | Mod prime | Semi-honest | `hemi.sh` |
| `temi-party.x` | Adapted [CDN01](https://eprint.iacr.org/2000/055) | Mod prime | Semi-honest | `temi.sh` |
| `soho-party.x` | Somewhat homomorphic encryption | Mod prime | Semi-honest | `soho.sh` |
| `semi-bin-party.x` | OT-based | Binary | Semi-honest | `semi-bin.sh` |
| `tiny-party.x` | Adapted SPDZ2k | Binary | Malicious | `tiny.sh` |
| `tinier-party.x` | [FKOS15](https://eprint.iacr.org/2015/901) | Binary | Malicious | `tinier.sh` |

Mama denotes MASCOT with several MACs to increase the security
parameter to a multiple of the prime length.

Semi and Semi2k denote the result of stripping MASCOT/SPDZ2k of all
steps required for malicious security, namely amplifying, sacrificing,
MAC generation, and OT correlation checks. What remains is the
generation of additively shared Beaver triples using OT.

Similarly, SemiBin denotes a protocol that generates bit-wise
multiplication triples using OT without any element of malicious
security.

Tiny denotes the adaption of SPDZ2k to the binary setting. In
particular, the SPDZ2k sacrifice does not work for bits, so we replace
it by cut-and-choose according to [Furukawa et
al.](https://eprint.iacr.org/2016/944)

The virtual machines for LowGear and HighGear run a key generation
similar to the one by [Rotaru et
al.](https://eprint.iacr.org/2019/1300). The main difference is using
daBits to generate maBits. CowGear and ChaiGear denote covertly
secure versions of LowGear and HighGear. In all relevant programs,
option `-T` activates [TopGear](https://eprint.iacr.org/2019/035)
zero-knowledge proofs in both.

Hemi and Soho denote the stripped version of LowGear and
HighGear, respectively, for semi-honest
security similar to Semi, that is, generating additively shared Beaver
triples using semi-homomorphic encryption.
Temi in turn denotes the adaption of
[Cramer et al.](https://eprint.iacr.org/2000/055) to LWE-based
semi-homomorphic encryption.
Both Hemi and Temi use the diagonal packing by [Halevi and
Shoup](https://eprint.iacr.org/2014/106) for matrix multiplication.

We will use MASCOT to demonstrate the use, but the other protocols
work similarly.

First compile the virtual machine:

`make -j8 mascot-party.x`

and a high-level program, for example the tutorial (use `-R 64` for
SPDZ2k and Semi2k and `-B <precision>` for SemiBin):

`./compile.py -F 64 tutorial`

To run the tutorial with two parties on one machine, run:

`./mascot-party.x -N 2 -I -p 0 tutorial`

`./mascot-party.x -N 2 -I -p 1 tutorial` (in a separate terminal)

Using `-I` activates interactive mode, which means that inputs are
solicited from standard input, and outputs are given to any
party. Omitting `-I` leads to inputs being read from
`Player-Data/Input-P<party number>-0` in text format.

Or, you can use a script to do run two parties in non-interactive mode
automatically:

`Scripts/mascot.sh tutorial`

To run a program on two different machines, `mascot-party.x`
needs to be passed the machine where the first party is running,
e.g. if this machine is name `diffie` on the local network:

`./mascot-party.x -N 2 -h diffie 0 tutorial`

`./mascot-party.x -N 2 -h diffie 1 tutorial`

The software uses TCP ports around 5000 by default, use the `-pn`
argument to change that.


### Yao's garbled circuits

We use half-gate garbling as described by [Zahur et
al.](https://eprint.iacr.org/2014/756.pdf) and [Guo et
al.](https://eprint.iacr.org/2019/1168.pdf). Alternatively, you can
activate the implementation optimized by [Bellare et
al.](https://eprint.iacr.org/2013/426) by adding `MY_CFLAGS +=
-DFULL_GATES` to `CONFIG.mine`.

Compile the virtual machine:

`make -j 8 yao`

and the high-level program:

`./compile.py -G -B <integer bit length> <program>`

Then run as follows:

  - Garbler: ```./yao-party.x [-I] -p 0 <program>```
  - Evaluator: ```./yao-party.x [-I] -p 1 -h <garbler host> <program>```

When running locally, you can omit the host argument. As above, `-I`
activates interactive input, otherwise inputs are read from
`Player-Data/Input-P<playerno>-0`.

By default, the circuit is garbled in chunks that are evaluated
whenever received.You can activate garbling all at once by adding
`-O` to the command line on both sides.

## Honest majority

The following table shows all programs for honest-majority computation:

| Program | Sharing | Domain | Malicious | \# parties | Script |
| --- | --- | --- | --- | --- | --- |
| `replicated-ring-party.x` | Replicated | Mod 2^k | N | 3 | `ring.sh` |
| `brain-party.x` | Replicated | Mod 2^k | Y | 3 | `brain.sh` |
| `ps-rep-ring-party.x` | Replicated | Mod 2^k | Y | 3 | `ps-rep-ring.sh` |
| `malicious-rep-ring-party.x` | Replicated | Mod 2^k | Y | 3 | `mal-rep-ring.sh` |
| `sy-rep-ring-party.x` | SPDZ-wise replicated | Mod 2^k | Y | 3 | `sy-rep-ring.sh` |
| `rep4-ring-party.x` | Replicated | Mod 2^k | Y | 4 | `rep4-ring.sh` |
| `replicated-bin-party.x` | Replicated | Binary | N | 3 | `replicated.sh` |
| `malicious-rep-bin-party.x` | Replicated | Binary | Y | 3 | `mal-rep-bin.sh` |
| `ps-rep-bin-party.x` | Replicated | Binary | Y | 3 | `ps-rep-bin.sh` |
| `replicated-field-party.x` | Replicated | Mod prime | N | 3 | `rep-field.sh` |
| `ps-rep-field-party.x` | Replicated | Mod prime | Y | 3 | `ps-rep-field.sh` |
| `sy-rep-field-party.x` | SPDZ-wise replicated | Mod prime | Y | 3 | `sy-rep-field.sh` |
| `malicious-rep-field-party.x` | Replicated | Mod prime | Y | 3 | `mal-rep-field.sh` |
| `atlas-party.x` | [ATLAS](https://eprint.iacr.org/2021/833) | Mod prime | N | 3 or more | `atlas.sh` |
| `shamir-party.x` | Shamir | Mod prime | N | 3 or more | `shamir.sh` |
| `malicious-shamir-party.x` | Shamir | Mod prime | Y | 3 or more | `mal-shamir.sh` |
| `sy-shamir-party.x` | SPDZ-wise Shamir | Mod prime | Y | 3 or more | `sy-shamir.sh` |
| `ccd-party.x` | CCD/Shamir | Binary | N | 3 or more | `ccd.sh` |
| `malicious-cdd-party.x` | CCD/Shamir | Binary | Y | 3 or more | `mal-ccd.sh` |

We use the "generate random triple optimistically/sacrifice/Beaver"
methodology described by [Lindell and
Nof](https://eprint.iacr.org/2017/816) to achieve malicious
security with plain arithmetic replicated secret sharing,
except for the "PS" (post-sacrifice) protocols where the
actual multiplication is executed optimistically and checked later as
also described by Lindell and Nof.
The implementations used by `brain-party.x`,
`malicious-rep-ring-party.x -S`, `malicious-rep-ring-party.x`,
and `ps-rep-ring-party.x` correspond to the protocols called DOS18
preprocessing (single), ABF+17 preprocessing, CDE+18 preprocessing,
and postprocessing, respectively,
by [Eerikson et al.](https://eprint.iacr.org/2019/164)
We use resharing by [Cramer et
al.](https://eprint.iacr.org/2000/037) for Shamir's secret sharing and
the optimized approach by [Araki et
al.](https://eprint.iacr.org/2016/768) for replicated secret sharing.
The CCD protocols are named after the [historic
paper](https://doi.org/10.1145/62212.62214) by Chaum, Crépeau, and
Damgård, which introduced binary computation using Shamir secret
sharing over extension fields of characteristic two.
SY/SPDZ-wise refers to the line of work started by [Chida et
al.](https://eprint.iacr.org/2018/570) for computation modulo a prime
and furthered by [Abspoel et al.](https://eprint.iacr.org/2019/1298)
for computation modulo a power of two. It involves sharing both a
secret value and information-theoretic tag similar to SPDZ but not
with additive secret sharing, hence the name.
Rep4 refers to the four-party protocol by [Dalskov et
al.](https://eprint.iacr.org/2020/1330)
`malicious-rep-bin-party.x` is based on cut-and-choose triple
generation by [Furukawa et al.](https://eprint.iacr.org/2016/944) but
using Beaver multiplication instead of their post-sacrifice
approach. `ps-rep-bin-party.x` is based on the post-sacrifice approach
by [Araki et
al.](https://www.ieee-security.org/TC/SP2017/papers/96.pdf) but
without using their cache optimization.

All protocols in this section require encrypted channels because the
information received by the honest majority suffices the reconstruct
all secrets. Therefore, an eavesdropper on the network could learn all
information.

MP-SPDZ uses OpenSSL for secure channels. You can generate the
necessary certificates and keys as follows:

`Scripts/setup-ssl.sh [<number of parties> <ssl_dir>]`

The programs expect the keys and certificates to be in
`SSL_DIR/P<i>.key` and `SSL_DIR/P<i>.pem`, respectively, and
the certificates to have the common name `P<i>` for player
`<i>`. Furthermore, the relevant root certificates have to be in
`SSL_DIR` such that OpenSSL can find them (run `c_rehash
<ssl_dir>`). The script above takes care of all this by generating
self-signed certificates. Therefore, if you are running the programs
on different hosts you will need to copy the certificate files.
Note that `<ssl_dir>` must match `SSL_DIR` set in `CONFIG` or `CONFIG.mine`.
Just like `SSL_DIR`, `<ssl_dir>` defaults to `Player-Data`.

In the following, we will walk through running the tutorial modulo
2^k with three parties. The other programs work similarly.

First, compile the virtual machine:

`make -j 8 replicated-ring-party.x`

In order to compile a high-level program, use `./compile.py -R 64`:

`./compile.py -R 64 tutorial`

If using another computation domain, use `-F` or `-B` as described in
[the relevant section above](#compiling-high-level-programs).

Finally, run the three parties as follows:

`./replicated-ring-party.x -I 0 tutorial`

`./replicated-ring-party.x -I 1 tutorial` (in a separate terminal)

`./replicated-ring-party.x -I 2 tutorial` (in a separate terminal)

or

`Scripts/ring.sh tutorial`

The `-I` argument enables interactive inputs, and in the tutorial party 0 and 1
will be asked to provide three numbers. Otherwise, and when using the
script, the inputs are read from `Player-Data/Input-P<playerno>-0`.

When using programs based on Shamir's secret sharing, you can specify
the number of parties with `-N` and the maximum number of corrupted
parties with `-T`. The latter can be at most half the number of
parties.

## Dealer model

This security model defines a special party that generates correlated
randomness such as multiplication triples, which is then used by all
other parties. MP-SPDZ implements the canonical protocol where the
other parties run the online phase of the semi-honest protocol in
Semi(2k/Bin) and the dealer provides all preprocessing. The security
assumption is that dealer doesn't collude with any other party, but
all but one of the other parties are allowed to collude. In our
implementation, the dealer is the party with the highest number, so
with three parties overall, Party 0 and 1 run the online phase.

| Program | Sharing | Domain | Malicious | \# parties | Script |
| --- | --- | --- | --- | --- | --- |
| `dealer-ring-party.x` | Additive | Mod 2^k | N | 3+ | `dealer-ring.sh` |

## BMR

BMR (Beaver-Micali-Rogaway) is a method of generating a garbled circuit
using another secure computation protocol. We have implemented BMR
based on all available implementations using GF(2^128) because the nature
of this field particularly suits the Free-XOR optimization for garbled
circuits. Our implementation is based on the [SPDZ-BMR-ORAM
construction](https://eprint.iacr.org/2017/981). The following table
lists the available schemes.

| Program | Protocol | Dishonest Maj. | Malicious | \# parties | Script |
| --- | --- | --- | --- | --- | --- |
| `real-bmr-party.x` | MASCOT | Y | Y | 2 or more | `real-bmr.sh` |
| `semi-bmr-party.x` | Semi | Y | N | 2 or more | `semi-bmr.sh` |
| `shamir-bmr-party.x` | Shamir | N | N | 3 or more | `shamir-bmr.sh` |
| `mal-shamir-bmr-party.x` | Shamir | N | Y | 3 or more | `mal-shamir-bmr.sh` |
| `rep-bmr-party.x` | Replicated | N | N | 3 | `rep-bmr.sh` |
| `mal-rep-bmr-party.x` | Replicated | N | Y | 3 | `mal-rep-bmr.sh` |

In the following, we will walk through running the tutorial with BMR
based on MASCOT and two parties. The other programs work similarly.

First, compile the virtual machine. In order to run with more than
three parties, change the definition of `MAX_N_PARTIES` in
`BMR/config.h` accordingly.

`make -j 8 real-bmr-party.x`

In order to compile a high-level program, use `./compile.py -B`:

`./compile.py -G -B 32 tutorial`

Finally, run the two parties as follows:

`./real-bmr-party.x -I 0 tutorial`

`./real-bmr-party.x -I 1 tutorial` (in a separate terminal)

or

`Scripts/real-bmr.sh tutorial`

The `-I` enable interactive inputs, and in the tutorial party 0 and 1
will be asked to provide three numbers. Otherwise, and when using the
script, the inputs are read from `Player-Data/Input-P<playerno>-0`.

## Online-only benchmarking

In this section we show how to benchmark purely the data-dependent
(often called online) phase of some protocols. This requires to
generate the output of a previous phase. There are two options to do
that:
1. For select protocols, you can run [preprocessing as
   required](#preprocessing-as-required).
2. You can run insecure preprocessing. For this, you will have to
   (re)compile the software after adding `MY_CFLAGS = -DINSECURE` to
   `CONFIG.mine` in order to run this insecure generation.
   Make sure to run `make clean` before recompiling any binaries.
   Then, you need to run `make Fake-Offline.x <protocol>-party.x`.

Note that you can as well run the full protocol with option `-v` to
see the cost split by preprocessing and online phase.

### SPDZ

The SPDZ protocol uses preprocessing, that is, in a first (sometimes
called offline) phase correlated randomness is generated independent
of the actual inputs of the computation. Only the second ("online")
phase combines this randomness with the actual inputs in order to
produce the desired results. The preprocessed data can only be used
once, thus more computation requires more preprocessing. MASCOT and
Overdrive are the names for two alternative preprocessing phases to go
with the SPDZ online phase.

All programs required in this section can be compiled with the target `online`:

`make -j 8 online`

#### To setup for benchmarking the online phase

This requires the INSECURE flag to be set before compilation as explained above. For a secure offline phase, see the section on SPDZ-2 below.

Run the command below. **If you haven't added `MY_CFLAGS = -DINSECURE` to `CONFIG.mine` before compiling, it will fail.**

`Scripts/setup-online.sh`

This sets up parameters for the online phase for 2 parties with a 128-bit prime field and 128-bit binary field, and creates fake offline data (multiplication triples etc.) for these parameters.

Parameters can be customised by running

`Scripts/setup-online.sh <nparties> <nbitsp> [<nbits2>]`


#### To compile a program

To compile for example the program in `./Programs/Source/tutorial.mpc`, run:

`./compile.py tutorial`

This creates the bytecode and schedule files in Programs/Bytecode/ and Programs/Schedules/

#### To run a program

To run the above program with two parties on one machine, run:

`./mascot-party.x -F -N 2 0 tutorial`

`./mascot-party.x -F -N 2 1 tutorial` (in a separate terminal)

Or, you can use a script to do the above automatically:

`Scripts/mascot.sh -F tutorial`

MASCOT is one of the protocols that use SPDZ for the online phase, and
`-F` causes the programs to read preprocessing material from files.

To run a program on two different machines, firstly the preprocessing
data must be copied across to the second machine (or shared using
sshfs), and secondly, `mascot-party.x` needs to be passed the machine
where the first party is running. E.g., if this machine is named
`diffie` on the local network:

`./mascot-party.x -F -N 2 -h diffie 0 test_all`

`./mascot-party.x -F -N 2 -h diffie 1 test_all`

The software uses TCP ports around 5000 by default, use the `-pn`
argument to change that.

### SPDZ2k

Creating fake offline data for SPDZ2k requires to call
`Fake-Offline.x` directly instead of via `setup-online.sh`:

`./Fake-Offline.x <nparties> -Z <bit length k for SPDZ2k> -S <security parameter>`

You will need to run `spdz2k-party.x -F` in order to use the data from storage.

### Other protocols

Preprocessing data for the default parameters of most other protocols
can be produced as follows:

`./Fake-Offline.x <nparties> -e <edaBit length,...>`

The `-e` command-line parameters accepts a list of integers separated
by commas.

You can then run the protocol with argument `-F`. Note that when
running on several hosts, you will need to distribute the data in
`Player-Data`. The preprocessing files contain `-P<party number>`
indicating which party will access it.

### BMR

This part has been developed to benchmark ORAM for the [Eurocrypt 2018
paper](https://eprint.iacr.org/2017/981) by Marcel Keller and Avishay
Yanay. It only allows to benchmark the data-dependent phase. The
data-independent and function-independent phases are emulated
insecurely.

By default, the implementations is optimized for two parties. You can
change this by defining `N_PARTIES` accordingly in `BMR/config.h`. If
you entirely delete the definition, it will be able to run for any
number of parties albeit slower.

Compile the virtual machine:

`make -j 8 bmr`

After compiling the mpc file:

- Run everything locally: `Scripts/bmr-program-run.sh <program>
<number of parties>`.
- Run on different hosts: `Scripts/bmr-program-run-remote.sh <program>
<host1> <host2> [...]`

#### Oblivious RAM

You can benchmark the ORAM implementation as follows:

1) Edit `Program/Source/gc_oram.mpc` to change size and to choose
Circuit ORAM or linear scan without ORAM.
2) Run `./compile.py -G -D gc_oram`. The `-D` argument instructs the
compiler to remove dead code. This is useful for more complex programs
such as this one.
3) Run `gc_oram` in the virtual machines as explained above.

## Preprocessing as required

For select protocols, you can run all required preprocessing but not
the actual computation. First, compile the binary:

`make <protocol>-offline.x`

At the time of writing the supported protocols are `mascot`,
`cowgear`, `mal-shamir`, `semi`, `semi2k`, and `hemi`.

If you have not done so already, then compile your high-level program:

`./compile.py <program>`

Finally, run the parties as follows:

`./<protocol>-offline.x -p 0 & ./<protocol>-offline.x -p 1 & ...`

The options for the network setup are the same as for the complete
computation above.

If you run the preprocessing on different hosts, make sure to use the
same player number in the preprocessing and the online phase.

## Benchmarking offline phases

#### SPDZ-2 offline phase

This implementation is suitable to generate the preprocessed data used in the online phase.
You need to compile with `USE_NTL = 1` in `CONFIG.mine` to run this.

For quick run on one machine, you can call the following:

`./spdz2-offline.x -p 0 & ./spdz2-offline.x -p 1`

More generally, run the following on every machine:

`./spdz2-offline.x -p <number of party> -N <total number of parties> -h <hostname of party 0> -c <covert security parameter>`

The number of parties are counted from 0. As seen in the quick example, you can omit the total number of parties if it is 2 and the hostname if all parties run on the same machine. Invoke `./spdz2-offline.x` for more explanation on the options.

`./spdz2-offline.x` provides covert security according to some parameter c (at least 2). A malicious adversary will get caught with probability 1-1/c. There is a linear correlation between c and the running time, that is, running with 2c takes twice as long as running with c. The default for c is 10.

The program will generate every kind of randomness required by the online phase except input tuples until you stop it. You can shut it down gracefully pressing Ctrl-c (or sending the interrupt signal `SIGINT`), but only after an initial phase, the end of which is marked by the output `Starting to produce gf2n`. Note that the initial phase has been reported to take up to an hour. Furthermore, 3 GB of RAM are required per party.

#### Benchmarking the MASCOT or SPDZ2k offline phase

These implementations are not suitable to generate the preprocessed
data for the online phase because they can only generate either
multiplication triples or bits.

MASCOT can be run as follows:

`host1:$ ./ot-offline.x -p 0 -c`

`host2:$ ./ot-offline.x -p 1 -c`

For SPDZ2k, use `-Z <k>` to set the computation domain to Z_{2^k}, and
`-S` to set the security parameter. The latter defaults to k. At the
time of writing, the following combinations are available: 32/32,
64/64, 64/48, and 66/48.

Running `./ot-offline.x` without parameters give the full menu of
options such as how many items to generate in how many threads and
loops.

#### Benchmarking Overdrive offline phases

We have implemented several protocols to measure the maximal throughput for the [Overdrive paper](https://eprint.iacr.org/2017/1230). As for MASCOT, these implementations are not suited to generate data for the online phase because they only generate one type at a time.

Binary | Protocol
------ | --------
`simple-offline.x` | SPDZ-1 and High Gear (with command-line argument `-g`)
`pairwise-offline.x` | Low Gear
`cnc-offline.x` | SPDZ-2 with malicious security (covert security with command-line argument `-c`)

These programs can be run similarly to `spdz2-offline.x`, for example:

`host1:$ ./simple-offline.x -p 0 -h host1`

`host2:$ ./simple-offline.x -p 1 -h host1`

Running any program without arguments describes all command-line arguments.

##### Memory usage

Lattice-based ciphertexts are relatively large (in the order of megabytes), and the zero-knowledge proofs we use require storing some hundred of them. You must therefore expect to use at least some hundred megabytes of memory per thread. The memory usage is linear in `MAX_MOD_SZ` (determining the maximum integer size for computations in steps of 64 bits), so you can try to reduce it (see the compilation section for how set it). For some choices of parameters, 4 is enough while others require up to 8. The programs above indicate the minimum `MAX_MOD_SZ` required, and they fail during the parameter generation if it is too low.
