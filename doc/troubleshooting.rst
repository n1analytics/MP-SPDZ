.. _troubleshooting:

Troubleshooting
---------------

This section shows how to solve some common issues.


Crash without error message or ``bad_alloc``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Some protocols require several gigabytes of memory, and the virtual
machine will crash if there is not enough RAM. You can reduce the
memory usage for some malicious protocols with ``-B 5``.
Furthermore, every computation thread requires
separate resources, so consider reducing the number of threads with
:py:func:`~Compiler.library.for_range_multithreads` and similar.


List indices must be integers or slices
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You cannot access Python lists with runtime variables because the
lists only exists at compile time. Consider using
:py:class:`~Compiler.types.Array`.


``compile.py`` takes too long or runs out of memory
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you use Python loops (``for``), they are unrolled at compile-time,
resulting in potentially too much virtual machine code. Consider using
:py:func:`~Compiler.library.for_range` or similar. You can also use
``-l`` when compiling, which will replace simple loops by an optimized
version.


Order of memory instructions not preserved
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

By default, the compiler runs optimizations that in some corner case
can introduce errors with memory accesses such as accessing an
:py:class:`~Compiler.types.Array`. If you encounter such errors, you
can fix this either  with ``-M`` when compiling or placing
`break_point()` around memory accesses.


Odd timings
~~~~~~~~~~~

Many protocols use preprocessing, which means they execute expensive
computation to generates batches of information that can be used for
computation until the information is used up. An effect of this is
that computation can seem oddly slow or fast. For example, one
multiplication has a similar cost then some thousand multiplications
when using homomorphic encryption because one batch contains
information for more than than 10,000 multiplications. Only when a
second batch is necessary the cost shoots up. Other preprocessing
methods allow for a variable batch size, which can be changed using
``-b``. Smaller batch sizes generally reduce the communication cost
while potentially increasing the number of communication rounds. Try
adding ``-b 10`` to the virtual machine (or script) arguments for very
short computations.


Disparities in round figures
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The number of virtual machine rounds given by the compiler are not an
exact prediction of network rounds but the number of relevant protocol
calls (such as multiplication, input, output etc) in the program. The
actual number of network rounds is determined by the choice of
protocol, which might use several rounds per protocol
call. Furthermore, communication at the beginning and the end of a
computation such as random key distribution and MAC checks further
increase the number of network rounds.


Handshake failures
~~~~~~~~~~~~~~~~~~

If you run on different hosts, the certificates
(``Player-Data/*.pem``) must be the same on all of them. Furthermore,
party ``<i>`` requires ``Player-Data/P<i>.key`` that must match
``Player-Data/P<i>.pem``, that is, they have to be generated to
together.  The easiest way of setting this up is to run
``Scripts/setup-ssl.sh`` on one host and then copy all
``Player-Data/*.{pem,key}`` to all other hosts. This is *not* secure
but it suffices for experiments. A secure setup would generate every
key pair locally and then distributed only the public keys.  Finally,
run ``c_rehash Player-Data`` on all hosts. The certificates generated
by ``Scripts/setup-ssl.sh`` expire after a month, so you need to
regenerate them. The same holds for ``Scripts/setup-client.sh`` if you
use the client facility.


Connection failures
~~~~~~~~~~~~~~~~~~~

MP-SPDZ requires one TCP port per party to be open to other
parties. In the default setting, it's 5000 on party 0, and
5001 on party 1 etc. You change change the base port (5000) using
``--portnumbase`` and individual ports for parties using
``--my-port``. The scripts in use a random base port number, which you
can also change with ``--portnumbase``.

When running computations with many (100 or more) parties, the default
timeout of 60 seconds might be triggered. In this case, you can adjust
the timeout using the ``--timeout`` parameter.


Internally called tape has unknown offline data usage
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Certain computations are not compatible with reading preprocessing
from disk. You can compile the binaries with ``MY_CFLAGS +=
-DINSECURE`` in ``CONFIG.mine`` in order to execute the computation in
a way that reuses preprocessing.


Illegal instruction
~~~~~~~~~~~~~~~~~~~

By default, the binaries are optimized for the machine they are
compiled on. If you try to run them an another one, make sure set
``ARCH`` in ``CONFIG`` accordingly. Furthermore, if you run on an x86
processor without AVX (produced before 2011), you need to set
``AVX_OT = 0`` to run dishonest-majority protocols.


Invalid instruction
~~~~~~~~~~~~~~~~~~~

The compiler code and the virtual machine binary have to be from the
same version because most version slightly change the bytecode. This
mean you can only use the precompiled binaries with the Python code in
the same release.


Computation used more preprocessing than expected
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This indicates an error in the internal accounting of
preprocessing. Please file a bug report.


``mac_fail``
~~~~~~~~~~~~

This is a catch-all failure in protocols with malicious protocols that
can be caused by something being wrong at any level. Please file a bug
report with the specifics of your case.

