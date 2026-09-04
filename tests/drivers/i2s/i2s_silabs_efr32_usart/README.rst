EFR32 USART I2S driver Unity tests (xG27)
=========================================

Overview
--------

On-target Unity firmware exercising the public Zephyr ``i2s_driver_api`` of the
``silabs,efr32-usart-i2s`` driver. No external codec, loopback wiring, or
expansion-header jumpers are required for the default registered tests
(**72 pass / 0 fail** on either xG27 board).

The suite resets driver state in ``setUp()`` / ``tearDown()`` via
``I2S_TRIGGER_DROP`` and ``i2s_configure(..., NULL)`` so test order is
independent.

Driver decoupling
-----------------

The I2S driver itself (``drivers/i2s/Kconfig.efr32_usart``) only ``select``\ s
``PINCTRL`` and ``DMA_SILABS_LDMA``. The clock-measurement tests verify
configured clock dividers through ``em_cmu`` and ``em_usart`` (no TIMER, no
PRS), so this test image does not add any peripheral Kconfig dependencies on
top of the driver itself - production firmware that only consumes the I2S API
stays lean.

**Registered tests:** 72 (see ``RUN_TEST`` list in ``src/test_runner.c``).

One firmware image runs **all** 72 cases at boot (``word_size`` 16 and 32,
configure / write / read / trigger / sign-extension / clock measurement). You do
**not** rebuild per API group - only pick the **board** and, optionally, the
driver **queue depth** Kconfig (see *Build matrix* below).

Requirements
------------

* West workspace rooted at ``projects/`` with Zephyr 4.4.0.
* Unity fetched by ``west update`` into ``projects/modules/lib/unity`` (see
  ``submanifests/unity.yaml``).
* Silicon Labs EFR32xG27 board: ``xg27_dk2602a`` or ``xg27_rb4194a``.
* Board overlay enables ``usart0`` as ``silabs,efr32-usart-i2s``, defines
  alias ``i2s-tx``, and (for MCLK tests) ``silabs,series-clock-clkout``.

Build matrix
------------

.. list-table::
   :header-rows: 1
   :widths: 22 18 60

   * - Case ID
     - Board
     - Notes
   * - ``unity_dk``
     - ``xg27_dk2602a``
     - Default ``prj.conf``; WS on **PB2**; MCO on **PB0** (port 1, pin 0).
   * - ``unity_rb``
     - ``xg27_rb4194a``
     - Default ``prj.conf``; WS on **PC3**; MCO on **PA4** (port 0, pin 4).
   * - ``unity_rb_ring2``
     - ``xg27_rb4194a``
     - ``CONFIG_I2S_SILABS_EFR32_USART_TX_BLOCK_COUNT=2`` and
       ``RX_BLOCK_COUNT=2`` (minimum queue depth; stresses ring-full tests).
   * - ``unity_rb_ring8``
     - ``xg27_rb4194a``
     - TX/RX queue depth **8** (larger ring than default **4**).
   * - ``unity_dk_ring2``
     - ``xg27_dk2602a``
     - Same minimum queue depth as ``unity_rb_ring2``.
   * - ``unity_dk_ring8``
     - ``xg27_dk2602a``
     - Same queue depth **8** as ``unity_rb_ring8``.

Twister (``testcase.yaml``) covers ``unity_dk`` and ``unity_rb`` as
``build_only`` on both integration platforms.

Building and running
--------------------

Run these from your **west workspace** directory (the folder that contains
``zephyr-4.4.0`` and ``.west``, for example ``devs-i2s-driver-zephyr/projects``
after ``west init -l zephyr-4.4.0``). Keep build output **outside** the Zephyr
tree with ``-d`` (matches a typical Zephyr west layout).

Workspace setup (once)
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   cd projects
   west update
   west zephyr-export
   west packages pip --install

If HAL modules are not present yet, run a full ``west update`` (or at least
``west update hal_silabs cmsis cmsis_6``) before building.

Default full suite - xG27-DK2602A (``unity_dk``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   python -m west build -p always -b xg27_dk2602a zephyr-4.4.0/tests/drivers/i2s/i2s_silabs_efr32_usart -d build/i2s_unity_dk -- -DBUILD_VERSION=4.4.0
   python -m west flash -d build/i2s_unity_dk

Default full suite - xG27-RB4194A (``unity_rb``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   python -m west build -p always -b xg27_rb4194a zephyr-4.4.0/tests/drivers/i2s/i2s_silabs_efr32_usart -d build/i2s_unity_rb -- -DBUILD_VERSION=4.4.0
   python -m west flash -d build/i2s_unity_rb

Build-only (no flash, CI / compile check)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   python -m west build -p always -b xg27_dk2602a zephyr-4.4.0/tests/drivers/i2s/i2s_silabs_efr32_usart -d build/i2s_unity_dk -- -DBUILD_VERSION=4.4.0
   python -m west build -p always -b xg27_rb4194a zephyr-4.4.0/tests/drivers/i2s/i2s_silabs_efr32_usart -d build/i2s_unity_rb -- -DBUILD_VERSION=4.4.0

Queue depth variants (minimum ring, depth = 2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Exercises ``test_write__ring_full_no_wait_returns_ebusy__*`` against the
smallest allowed ``CONFIG_I2S_SILABS_EFR32_USART_*_BLOCK_COUNT``.

**RB4194A:**

First configure (from ``projects/``). On **PowerShell**, quote each ``-D`` argument
(``;`` in ``CONF_FILE`` is a command separator in PowerShell):

.. code-block:: powershell

   python -m west build -b xg27_rb4194a zephyr-4.4.0/tests/drivers/i2s/i2s_silabs_efr32_usart -d build/i2s_unity_rb_ring2 -- "-DBUILD_VERSION=4.4.0" "-DCONF_FILE=prj.conf;prj_ring2.conf"
   python -m west flash -d build/i2s_unity_rb_ring2

Incremental rebuild after editing tests (no ``-p always``):

.. code-block:: powershell

   python -m west build -d build/i2s_unity_rb_ring2
   python -m west flash -d build/i2s_unity_rb_ring2

Linux/macOS bash may use the same ``CONF_FILE`` without extra quotes:

.. code-block:: console

   west build -b xg27_rb4194a zephyr-4.4.0/tests/drivers/i2s/i2s_silabs_efr32_usart -d build/i2s_unity_rb_ring2 -- -DBUILD_VERSION=4.4.0 -DCONF_FILE="prj.conf;prj_ring2.conf"

**DK2602A:**

.. code-block:: console

   python -m west build -p always -b xg27_dk2602a zephyr-4.4.0/tests/drivers/i2s/i2s_silabs_efr32_usart -d build/i2s_unity_dk_ring2 -- -DBUILD_VERSION=4.4.0 -DCONFIG_I2S_SILABS_EFR32_USART_TX_BLOCK_COUNT=2 -DCONFIG_I2S_SILABS_EFR32_USART_RX_BLOCK_COUNT=2
   python -m west flash -d build/i2s_unity_dk_ring2

Queue depth variants (depth = 8)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Larger software queue than the default **4**; same 72 tests, different
``TX_RING_DEPTH`` / ``RX_RING_DEPTH`` at compile time.

**RB4194A:**

.. code-block:: console

   python -m west build -p always -b xg27_rb4194a zephyr-4.4.0/tests/drivers/i2s/i2s_silabs_efr32_usart -d build/i2s_unity_rb_ring8 -- -DBUILD_VERSION=4.4.0 -DCONFIG_I2S_SILABS_EFR32_USART_TX_BLOCK_COUNT=8 -DCONFIG_I2S_SILABS_EFR32_USART_RX_BLOCK_COUNT=8
   python -m west flash -d build/i2s_unity_rb_ring8

**DK2602A:**

.. code-block:: console

   python -m west build -p always -b xg27_dk2602a zephyr-4.4.0/tests/drivers/i2s/i2s_silabs_efr32_usart -d build/i2s_unity_dk_ring8 -- -DBUILD_VERSION=4.4.0 -DCONFIG_I2S_SILABS_EFR32_USART_TX_BLOCK_COUNT=8 -DCONFIG_I2S_SILABS_EFR32_USART_RX_BLOCK_COUNT=8
   python -m west flash -d build/i2s_unity_dk_ring8

Twister (both boards, build-only)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

From ``projects/``:

.. code-block:: console

   west twister -T zephyr-4.4.0/tests/drivers/i2s/i2s_silabs_efr32_usart --build-only

Equivalent to building testcase ``drivers.i2s.silabs_efr32_usart.unity`` on
``xg27_dk2602a`` and ``xg27_rb4194a``.

Console output
--------------

Open the UART console (USART1, board default, **115200 8N1** on the board USB).

Each test prints an indexed line and a one-line verdict (``PASS`` / ``FAIL`` /
``IGNORE``). Unity's own ``(i2s:line:test_name:PASS)`` line is line-buffered
(one ``printk`` per line) so UART should not drop characters. A **TEST SUMMARY**
block at the end lists failed and ignored test names explicitly.

Example:

.. code-block:: none

   === I2S silabs,efr32-usart-i2s Unity tests ===
   ...
   --- Group: write (14) ---

   [15/72] test_write__valid_in_ready_returns_zero__w16
   (i2s:167:test_write__valid_in_ready_returns_zero__w16:PASS)
        => PASS

   ...

   -----------------------
   72 Tests 0 Failures 0 Ignored
   OK

   ========== TEST SUMMARY ==========
   Registered : 72
   Executed   : 72
   Failures   : 0
   Ignored    : 0
   ==================================

If you still see ``--- N messages dropped ---``, lower the serial viewer baud
mismatch, disable ANSI color in the terminal, or flash a build with
``CONFIG_LOG=n`` (default in this test ``prj.conf``).

Test catalog
------------

**Total registered test cases:** **72** (``UNITY_TEST_COUNT`` in
``src/test_runner.c``; must stay in sync with the ``RUN_TEST(...)`` list).

All cases run in **one** flash of the default ``unity_dk`` / ``unity_rb`` image,
in the order below. On a good board with no extra wiring, expect
**72 passed** and **0 failed**.

Summary by group
~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 6 26 10 58

   * - #
     - Group
     - Count
     - Source file
   * - 1
     - Init & device
     - 2
     - ``src/test_i2s_efr32_configure.c``
   * - 2
     - Configure
     - 27
     - ``src/test_i2s_efr32_configure.c``
   * - 3
     - Write
     - 14
     - ``src/test_i2s_efr32_write.c``
   * - 4
     - Read
     - 5
     - ``src/test_i2s_efr32_read.c``
   * - 5
     - Trigger & ISR
     - 17
     - ``src/test_i2s_efr32_trigger.c``
   * - 6
     - Sign extension
     - 4
     - ``src/test_i2s_efr32_sign_extension.c``
   * - 7
     - Clock measurement
     - 3
     - ``src/test_clock_measure.c``
   * -
     - **Total**
     - **72**
     -

Group 1 - Init & device (2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

#. ``test_init__device_is_ready``
#. ``test_init__i2s_tx_alias_resolves``

Group 2 - Configure (27)
~~~~~~~~~~~~~~~~~~~~~~~~

#. ``test_configure__stop_with_null_cfg``
#. ``test_configure__config_get_null_before_configure``
#. ``test_configure__config_get_after_success__w16``
#. ``test_configure__config_get_after_success__w32``
#. ``test_configure__config_get_null_after_stop__w16``
#. ``test_configure__rejects_word_size_zero``
#. ``test_configure__rejects_channels_not_two__w16``
#. ``test_configure__rejects_block_size_zero__w16``
#. ``test_configure__rejects_block_size_not_frame_aligned__w16``
#. ``test_configure__rejects_block_size_not_frame_aligned__w32``
#. ``test_configure__rejects_dir_both__w16``
#. ``test_configure__rejects_word_size_unsupported``
#. ``test_configure__rejects_frame_clk_below_min__w16``
#. ``test_configure__rejects_unsupported_options__w16``
#. ``test_configure__rejects_bit_clk_target__w16``
#. ``test_configure__rejects_frame_clk_target__w16``
#. ``test_configure__rejects_format_right_justified__w16``
#. ``test_configure__rejects_format_pcm_short__w16``
#. ``test_configure__rejects_mismatched_tx_rx_freq__w16``
#. ``test_configure__rejects_mismatched_tx_rx_word_size``
#. ``test_configure__rejects_mismatched_tx_rx_format__w16``
#. ``test_configure__success_sets_ready__w16``
#. ``test_configure__success_sets_ready__w32``
#. ``test_configure__left_justified_format_ok__w16``
#. ``test_configure__configure_rx_then_tx_matching__w16``
#. ``test_configure__good_board_expect_zero__w16``
#. ``test_configure__good_board_expect_zero__w32``

Group 3 - Write (14)
~~~~~~~~~~~~~~~~~~~~

#. ``test_write__no_configure_returns_eio``
#. ``test_write__after_stop_returns_eio__w16``
#. ``test_write__rejects_size_zero__w16``
#. ``test_write__rejects_size_gt_block__w16``
#. ``test_write__rejects_size_not_frame_aligned__w16``
#. ``test_write__rejects_size_not_frame_aligned__w32``
#. ``test_write__valid_in_ready_returns_zero__w16``
#. ``test_write__valid_in_ready_returns_zero__w32``
#. ``test_write__valid_in_running_returns_zero__w16``
#. ``test_write__valid_in_running_returns_zero__w32``
#. ``test_write__ring_full_no_wait_returns_ebusy__w16``
#. ``test_write__ring_full_no_wait_returns_ebusy__w32``
#. ``test_write__stopping_state_returns_eio__w16``
#. ``test_write__stopping_state_returns_eio__w32``

Group 4 - Read (5)
~~~~~~~~~~~~~~~~~~

#. ``test_read__no_configure_returns_eio``
#. ``test_read__timeout_no_data_returns_ebusy__w16``
#. ``test_read__timeout_no_data_returns_ebusy__w32``
#. ``test_read__short_timeout_returns_eagain__w16``
#. ``test_read__live_data_after_start`` *(needs PC0 TX tied to PC1 RX)*

Group 5 - Trigger & ISR (17)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#. ``test_trigger__start_from_ready__w16``
#. ``test_trigger__start_from_ready__w32``
#. ``test_trigger__start_without_configure_is_noop``
#. ``test_trigger__stop_tx_empty_queue_immediate_ready__w16``
#. ``test_trigger__stop_tx_empty_queue_immediate_ready__w32``
#. ``test_trigger__stop_tx_nonempty_queue_drains__w16``
#. ``test_trigger__stop_tx_nonempty_queue_drains__w32``
#. ``test_trigger__stop_rx_immediate_ready__w16``
#. ``test_trigger__stop_rx_immediate_ready__w32``
#. ``test_trigger__drain_from_running__w16``
#. ``test_trigger__drain_not_running_rejects__w16``
#. ``test_trigger__drain_rx_rejects__w16``
#. ``test_trigger__drop_with_cfg_returns_ready__w16``
#. ``test_trigger__drop_without_cfg_returns_not_ready``
#. ``test_trigger__prepare_not_error_rejects__w16``
#. ``test_trigger__unknown_cmd_rejects__w16``
#. ``test_trigger__stop_not_running_rejects__w16``

Group 6 - Sign extension (4)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#. ``test_sign_extension__sample_expand_zeros_positive__w32``
#. ``test_sign_extension__sample_expand_differs_from_signed_widen``
#. ``test_sign_extension__correct_sign_extend_reference``
#. ``test_sign_extension__low16_mask_safe_compare``

Group 7 - Clock measurement (3)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#. ``test_clock__mclk_routed_at_expected_ratio``
#. ``test_clock__bclk_matches_configured_rate__w16``
#. ``test_clock__bclk_matches_configured_rate__w32``

The authoritative registration order is the ``RUN_TEST`` block in
``src/test_runner.c`` (lines 131-212). When adding or removing a case, update
that list, ``UNITY_TEST_COUNT``, this catalog, and the boot-banner string.

Clock measurement
-----------------

Diagnostic clock checks live in ``src/test_clock_measure.c``. **No expansion-
header jumpers are required** on either board. All three checks read the same
hardware registers the I2S driver programs, so they never depend on TIMER
input-capture, PRS routing, or GPIO bit-banging (earlier revisions tried both
PRS-fed TIMER capture and GPIO-DIN edge counting on the USART pins; both
caused Hard Faults / 0 Hz read-back on xG27 because the USART output pins
cannot be sampled reliably while held in peripheral push-pull).

How each clock is verified
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 18 14 68

   * - Signal
     - Jumper?
     - How the test verifies it
   * - **MCLK (MCO)**
     - **No**
     - Compares ``CMU_ClockFreqGet(cmuClock_EXPCLK) / prescaler`` against
       ``HFRCODPLL / prescaler`` (within +/-5%). The ``silabs,series-clock-clkout``
       driver has already routed EXPCLK to the MCO pin (PA4 on RB4194A,
       PB0 on DK2602A); the test re-derives the divider from CMU rather than
       sampling the pin.
   * - **BCLK**
     - **No**
     - Configures TX (``frame_clk = 16 kHz``, ``word_size = 16`` then ``32``),
       then reads back ``USART_BaudrateGet(USART0)`` which decodes the
       ``USART0->CLKDIV`` register against ``cmuClock_PCLK``. Asserts
       ``BCLK ≈ frame_clk * 2 * word_size`` (within +/-5%).
   * - **LRCLK**
     - **No**
     - Derived: ``LRCLK = BCLK / (2 * word_size)``. The USART I2S engine
       generates word-select directly from the bit clock - there is no
       independent LRCLK divider to verify on xG27.

Why register read-back, not GPIO sampling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* ``USART0`` drives PC2 / PC3 in peripheral push-pull mode. GPIO ``DIN`` is
  unreliable in that mode on xG27 (read-back returns a static value), so a
  busy-wait edge counter sees zero edges even while I2S is streaming.
* xG27 also lacks a PRS producer for the USART bit clock output, so we cannot
  capture BCLK through a PRS-fed TIMER without a jumper to a regular GPIO -
  and the TIMER / PRS re-init path conflicts with Zephyr's clock manager
  (Bus fault on TIMER2 base).
* Reading the divider register the driver just wrote covers what the test
  actually needs to prove: the driver translated ``frame_clk_freq`` and
  ``word_size`` into the right ``CLKDIV`` for ``cmuClock_PCLK``.

Enabling / disabling the clock group
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The three clock cases run when the board overlay declares
``silabs,i2s-clock-measure`` (RB4194A overlay does, DK2602A overlay omits it
so the two BCLK cases ``IGNORE`` at runtime). The MCLK case additionally
needs ``silabs,series-clock-clkout`` (present in both overlays).

External / ppm-level verification
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The +/-5% window in the test flags routing or divider bugs, not absolute
crystal accuracy. For ppm-level confidence put a scope on PC2 (BCLK), PC3
(LRCLK) and PA4 / PB0 (MCO) - the suite does not assert on the wire.

Loopback wiring (``test_read__live_data_after_start``)
------------------------------------------------------

This test **runs** (not ignored). It needs **PC0 (TX) -> PC1 (RX)** plus GND.
Sequence: configure RX+TX, **prefill TX in READY**, ``START`` RX then TX,
``i2s_read()`` — same pattern as Zephyr ``i2s_api`` loopback tests.

Troubleshooting
---------------

* **Build error: unity.c not found** - run ``west update`` from ``projects/``
  so ``modules/lib/unity`` is populated.
* **Link error: undefined reference to i2s** - confirm ``CONFIG_I2S_SILABS_EFR32_USART=y``
  and the board overlay is applied (``dt_alias_exists("i2s-tx")``).
* **Device not ready** - check ``&dma0 { status = "okay"; }`` and that the
  on-board SPI flash node is disabled in the overlay so USART0 pins are free.
* **Hanging tests** - deferred TX ``STOP`` / ``DRAIN`` tests wait up to 3 s for
  DMA drain. The clock cases are register-only and complete in microseconds.
* **``stop_tx_nonempty_queue_drains`` / ``test_wait_tx_ready`` timeout** -
  deferred TX ``STOP``/``DRAIN`` tests queue blocks in **READY** before
  ``START`` so an open TX line cannot drain the ring before ``STOP`` returns.
  Rebuild and reflash; if it still fails, confirm DMA/clock enable in the
  overlay.
* **Ring-full test fails** - rebuild with ``unity_*_ring2`` or confirm
  ``CONFIG_I2S_SILABS_EFR32_USART_TX_BLOCK_COUNT`` matches
  ``test_i2s_efr32_common.h``.
* **BCLK clock test ignored** - rebuild with the RB4194A overlay that enables
  ``silabs,i2s-clock-measure``. On DK2602A the overlay omits that node so
  both BCLK cases ``IGNORE`` at runtime; the MCLK case still runs because
  ``silabs,series-clock-clkout`` is present on both boards.
* **BCLK measured = 0 Hz** - regression: GPIO-DIN read-back on a USART-driven
  pin returns a static value. Verify ``test_clock_measure.c`` uses
  ``USART_BaudrateGet()`` (register read-back) and not GPIO polling on
  PC2/PC3.
* **MCLK test fails** - confirm ``silabs,series-clock-clkout`` is ``okay`` in the
  overlay; on RB4194A the MCO pin is PA4, on DK2602A it is PB0. A wrong
  ``clock-div`` will also miss the +/-5% window.