Virtual Machine
===============

Calling ``compile.py`` outputs the computation in a format specific to
MP-SPDZ. This includes a schedule file and one or several bytecode
files. The schedule file can be found at
``Programs/Schedules/<progname>.sch``. It contains the names of all
bytecode files found in ``Programs/Bytecode`` and the maximum number
of parallel threads. Each bytecode file represents the complete
computation of one thread, also called tape. The computation of the
main thread is always ``Programs/Bytecode/<progname>-0.bc`` when
compiled by the compiler.


Schedule File
-------------

The schedule file is as follows in ASCII text::

  <maximum number of threads>
  <number of bytecode files>
  <bytecode name>:<no of instructions>[ <bytecode name>:<no of instructions>...]
  1 0
  0
  <compilation command line>
  <domain requirements>
  opts: <potential optimizations>
  sec:<security parameter>

Domain requirements and potential optimizations are related to
:ref:`nonlinear`. Domain requirements is one of the following:

``lgp:<length>``
  minimum prime length

``p:<prime>``
  exact prime

``R:<length>``
  exact power of two

Potential optimizations is a any combination of ``trunc_pr``
(probabilistic truncation), ``edabit`` (edaBits), and ``split`` (share
splitting). Presence indicates that they would change the compiled
bytecode if used. This is used to indicate available optimizations
when running a virtual machine.

For example, ``./compile.py tutorial`` generates the following
schedule file::

  1
  1
  tutorial-0:19444
  1 0
  0
  ./compile.py tutorial
  lgp:106
  opts: edabit trunc_pr split
  sec:40

This says that program has only one thread running one bytecode file,
which is stored in ``tutorial-0.bc`` and has 19444 instructions. It
requires a prime of length 106, and all protocol optimizations could
potentially be used. The length 106 is composed as follows: assuming
64-bit integers, the difference used for comparison is a 65-bit
integer, to which 40 bits are added for statistical masking, resulting
in a 105 bits, and it takes a 106-bit prime to able to contain all
105-bit numbers.


Bytecode
--------

The bytecode is made up of 32-bit units in big-endian byte
order. Every unit represents an instruction code (possibly including
vector size), register number, or immediate value.

For example, adding the secret integers in registers 1 and 2 and then
storing the result at register 0 leads to the following bytecode (in
hexadecimal representation):

.. code-block:: none

  00 00 00 21  00 00 00 00  00 00 00 01  00 00 00 02

This is because ``0x021`` is the code of secret integer addition. The
debugging output (``compile.py -a <prefix>``) looks as follows::

  adds s0, s1, s2 # <instruction number>

There is also a vectorized addition. Adding 10 secret integers in
registers 10-19 and 20-29 and then storing the result in registers 0-9
is represented as follows in bytecode:

.. code-block:: none

  00 00 28 21  00 00 00 00  00 00 00 0a  00 00 00 14

This is because the vector size is stored in the upper 22 bits of the
first 32-bit unit (instruction codes are up to 10 bits long), and
``0x28`` equals 40 or 10 shifted by two bits. In the debugging output
the vectorized addition looks as follows::

  vadds 10, s0, s10, s20 # <instruction number>

Finally, some instructions have a variable number of arguments to
accommodate any number of parallel operations. For these, the first
argument usually indicates the number of arguments yet to come. For
example, multiplying the secret integers in registers 2 and 3 as well
as registers 4 and 5 and the storing the two results in registers 0
and 1 results in the following bytecode:

.. code-block:: none

  00 00 00 a6  00 00 00 06  00 00 00 00  00 00 00 02
  00 00 00 03  00 00 00 01  00 00 00 04  00 00 00 05

and the following debugging output::

  muls 6, s0, s2, s3, s1, s4, s5 # <instruction number>

Note that calling instructions in high-level code never is done with
the explicit number of arguments. Instead, this is derived from number
of function arguments. The example above would this simply be called
as follows::

  muls(s0, s2, s3, s1, s4, s5)


Memory size indication
~~~~~~~~~~~~~~~~~~~~~~

By default, the compiler adds memory read instructions such as
``ldms`` at the end of the main bytecode file to indicate the memory
size. This is to make sure that even when using memory with run-time
addresses, the virtual machine is aware of the memory sizes.


.. _instructions:

Instructions
------------

The following table list all instructions except the ones for
:math:`\mathrm{GF}(2^n)` computation, untested ones, and those considered
obsolete.

.. csv-table::
   :header: Name, Code
   :widths: 50, 15, 100
   :file: instructions.csv


Compiler.instructions module
----------------------------

.. automodule:: Compiler.instructions
   :members:
   :no-undoc-members:
   :exclude-members: asm_input, sqrs,
		     start_grind, stop_grind,
		     writesocketc, writesocketint,
		     matmul_base, inputmixed_base, raw_output,
		     gbitcom, gbitdec, gbitgf2ntriple, gbittriple,
		     gconvgf2n, gldmci, gldmsi, gmulbitc, gmulbitm,
		     gnotc, gstmci, gstmsi,

Compiler.GC.instructions module
-------------------------------

.. automodule:: Compiler.GC.instructions
   :members:
   :no-undoc-members:
   :exclude-members: BinaryVectorInstruction, NonVectorInstruction,
		     NonVectorInstruction1, ldmsd, stmsd, ldmsdi, stmsdi,
		     stmsdci
