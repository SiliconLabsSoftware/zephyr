# On-target Unity test — `i2s_silabs_efr32_usart` driver

On-target test app for the Silabs EFR32 USART I2S driver, driven by the
Unity Test Framework ([ThrowTheSwitch/Unity](https://github.com/ThrowTheSwitch/Unity),
tag **v2.5.2**). It is a West project declared in
`zephyr-4.4.0/submanifests/unity.yaml` and is cloned to **`modules/lib/unity`**
under the West workspace root (typically `projects/`) after **`west update`**.
The app uses the Fixture harness: `unity.c` + `unity_fixture.c` + `unity_config.h`.

**Supported hardware:** Silicon Labs **xG27-DK2602A** (Zephyr `xg27_dk2602a`).
The overlay `boards/xg27_dk2602a.overlay` reclaims USART0 for
`silabs,efr32-usart-i2s` on **PC0** (TX), **PC1** (RX), **PC2** (CLK),
**PC3** (WS) — the same pin set the DK already exposes to the on-board
MX25R8035F SPI flash (the flash node is deleted by the overlay so the
USART can own those pins).

## What is covered

| Group | File | Tests | Needs wire? |
|-------|------|------:|-------------|
| `i2s_efr32_configure`  | `src/test_configure.c`  | 18 | no |
| `i2s_efr32_config_get` | `src/test_config_get.c` | 4 | no |
| `i2s_efr32_states`     | `src/test_states.c`     | 16 | no |
| `i2s_efr32_io`         | `src/test_io.c`         | 11 | no |
| `i2s_efr32_errors`     | `src/test_errors.c`     | 3 | no |
| `i2s_efr32_clock`      | `src/test_clock.c`      | 3 | no |
| `i2s_efr32_mclk`       | `src/test_mclk.c`       | 1 | no (board has clkout) |
| `i2s_efr32_loopback`   | `src/test_loopback.c`   | 2 | **yes** (**PC0 ↔ PC1**) |
| **Total no-wire**      |                         | **56** | |
| **Total full**         |                         | **58** | |

## Test cases

The group/test names below are passed to `RUN_TEST_CASE(group, name)` in
`src/test_runners.c`. Unity prints them as `TEST(group, name)` so they
are searchable in the serial log.

| Group | Test |
|---|---|
| `i2s_efr32_configure`  | `valid_16bit_44k1__accepted` |
| `i2s_efr32_configure`  | `sub_8k_frame_clk__rejected` |
| `i2s_efr32_configure`  | `word_24bit__rejected` |
| `i2s_efr32_configure`  | `pcm_format__rejected` |
| `i2s_efr32_configure`  | `tx_rx_incompatible__rejected` |
| `i2s_efr32_configure`  | `left_justified_stereo__accepted` |
| `i2s_efr32_configure`  | `left_justified_mono__rejected` |
| `i2s_efr32_configure`  | `clock_format_nf_ib__accepted` |
| `i2s_efr32_configure`  | `clock_format_if_ib__accepted` |
| `i2s_efr32_configure`  | `options_bit_clk_gated__accepted` |
| `i2s_efr32_configure`  | `unsupported_option_bit__rejected` |
| `i2s_efr32_configure`  | `bit_clk_target_option__rejected` |
| `i2s_efr32_configure`  | `frame_clk_target_option__rejected` |
| `i2s_efr32_configure`  | `dir_both_valid_cfg__enosys` |
| `i2s_efr32_configure`  | `word_size_zero__rejected` |
| `i2s_efr32_configure`  | `channels_zero__rejected` |
| `i2s_efr32_configure`  | `block_size_zero__rejected` |
| `i2s_efr32_configure`  | `block_size_not_frame_multiple__rejected` |
| `i2s_efr32_config_get` | `get_tx_after_configure__returns_same_fields` |
| `i2s_efr32_config_get` | `get_rx_when_only_tx_configured__returns_null` |
| `i2s_efr32_config_get` | `get_tx_after_deconfigure__returns_null` |
| `i2s_efr32_config_get` | `get_rx_after_configure_both_dirs__returns_rx_cfg` |
| `i2s_efr32_states`     | `start_in_not_ready__rejected` |
| `i2s_efr32_states`     | `configure_then_start_running` |
| `i2s_efr32_states`     | `stop_in_ready__rejected` |
| `i2s_efr32_states`     | `prepare_when_not_error__rejected` |
| `i2s_efr32_states`     | `write_timeout_when_no_dma_consumer` |
| `i2s_efr32_states`     | `start_rx_in_ready__running` |
| `i2s_efr32_states`     | `stop_tx_running_empty_queue__ready_immediately` |
| `i2s_efr32_states`     | `stop_tx_running_pending_queue__stopping_then_ready` |
| `i2s_efr32_states`     | `stop_rx_running__ready` |
| `i2s_efr32_states`     | `drain_tx_running__ready_after_drain` |
| `i2s_efr32_states`     | `drain_tx_in_ready__rejected` |
| `i2s_efr32_states`     | `drain_rx__rejected` |
| `i2s_efr32_states`     | `drop_running_tx__ready` |
| `i2s_efr32_states`     | `drop_both_dirs__both_ready` |
| `i2s_efr32_states`     | `drop_in_not_ready__no_state_change` |
| `i2s_efr32_states`     | `unknown_trigger_cmd__rejected` |
| `i2s_efr32_io`         | `write_without_configure__eio` |
| `i2s_efr32_io`         | `write_size_zero__einval` |
| `i2s_efr32_io`         | `write_size_gt_block_size__einval` |
| `i2s_efr32_io`         | `write_size_not_frame_aligned__einval` |
| `i2s_efr32_io`         | `write_size_not_dma_aligned__einval` |
| `i2s_efr32_io`         | `write_in_ready_state__queues_ok` |
| `i2s_efr32_io`         | `write_when_tx_in_error__eio` |
| `i2s_efr32_io`         | `read_without_configure__eio` |
| `i2s_efr32_io`         | `read_no_data_no_wait__ebusy` |
| `i2s_efr32_io`         | `read_no_data_with_short_timeout__eagain` |
| `i2s_efr32_io`         | `read_after_stop__no_garbage` |
| `i2s_efr32_errors`     | `txuf_underrun_transitions_to_error` |
| `i2s_efr32_errors`     | `rxof_overrun_transitions_to_error` |
| `i2s_efr32_errors`     | `prepare_after_underrun__back_to_ready_then_runs` |
| `i2s_efr32_clock`      | `nonzero_clkdiv_after_configure` |
| `i2s_efr32_clock`      | `doubling_word_size_halves_divider` |
| `i2s_efr32_clock`      | `halving_frame_clk_doubles_divider` |
| `i2s_efr32_mclk`       | `clkout_in_2pct_range` |
| `i2s_efr32_loopback`   | `loopback_16bit_short_buffer_matches` |
| `i2s_efr32_loopback`   | `loopback_32bit_short_buffer_matches` |

## Requirements

- xG27-DK2602A (`xg27_dk2602a`).
- A Zephyr 4.4 toolchain that can already build
  `samples/drivers/i2s/tas2505` for the same board.
- For the loopback group: jumper **USART0 TX → USART0 RX** data pin only
  (**PC0 → PC1**).

## Build & flash

From `projects/zephyr-4.4.0/`:

> **Windows note.** If `west` is installed via pip but you don't have a
> `west.exe` shim on `PATH`, replace `west` with `python -m west` in
> every command below. You also need to set `ZEPHYR_SDK_INSTALL_DIR`
> and `ZEPHYR_TOOLCHAIN_VARIANT=zephyr` in the shell.

### No wire — 56 tests

```bash
west build -p always -b xg27_dk2602a -d build_unity \
    tests/drivers/i2s/i2s_silabs_efr32_usart
west flash -d build_unity
```

Open the on-board VCOM (115200 8N1). Expected output (default mode):

```
*** Booting Zephyr OS build ... ***

*** I2S Silabs EFR32 USART driver - Unity test runner ***
Device: usart@5005c000 @ alias i2s-node0
Frame clock: 16000 Hz, slab depth: tx=8 rx=8
Loopback group: disabled (build with CONFIG_I2S_EFR32_TEST_LOOPBACK=y to enable)
Output mode: default - one '.' per pass, 'F' per fail.
  Enable CONFIG_I2S_EFR32_TEST_VERBOSE=y for per-test names,
  or CONFIG_I2S_EFR32_TEST_SILENT=y to suppress dots.
Unity argv: i2s_efr32_unity
----------------------------------------
Unity test run 1 of 1
........................................................
-----------------------
56 Tests 0 Failures 0 Ignored
OK
----------------------------------------
*** Unity run complete: 0 failure(s) ***
```

Each `.` is a passing test case. See [Verbose / filtered output](#verbose--filtered-output)
for how to switch to per-test logging.

### Full 58 tests (loopback wire)

1. Jumper **USART0 TX → USART0 RX** for loopback: **PC0 → PC1**.
2. Build with the loopback flag:

```bash
west build -p always -b xg27_dk2602a -d build_unity_loopback \
    tests/drivers/i2s/i2s_silabs_efr32_usart -- \
    -DCONFIG_I2S_EFR32_TEST_LOOPBACK=y
west flash -d build_unity_loopback
```

The serial output adds two more `.` and the totals become `58 Tests`.

With `CONFIG_I2S_EFR32_TEST_LOOPBACK=y`, each loopback case prints
`[loopback16]` / `[loopback32]` lines: I2S API return codes, a wire hint,
the first eight stereo samples (**TX from a stack snapshot taken right
after the pattern fill, before `i2s_write`**, because the driver returns
the TX mem_slab block when TX DMA completes), RX from `i2s_read`, and a
32-byte hex prefix of each.

## Verbose / filtered output

The runner translates the following Kconfig knobs into argv passed to
`UnityMain` at startup. Override them with `-D…` on the `west build`
line, or set them in `prj.conf` for a permanent build flavor.

| Kconfig | Equivalent Unity flag | Effect |
|---|---|---|
| `CONFIG_I2S_EFR32_TEST_VERBOSE=y`            | `-v`        | Print `TEST(group, name) PASS` per test instead of one `.`. |
| `CONFIG_I2S_EFR32_TEST_SILENT=y`             | `-s`        | Suppress the dots; only failures + summary are printed. Mutually exclusive with `VERBOSE`. |
| `CONFIG_I2S_EFR32_TEST_GROUP_FILTER="<sub>"` | `-g <sub>`  | Only run `TEST_GROUP`s whose name contains `<sub>`. |
| `CONFIG_I2S_EFR32_TEST_NAME_FILTER="<sub>"`  | `-n <sub>`  | Only run `TEST` cases whose name contains `<sub>`. |
| `CONFIG_I2S_EFR32_TEST_REPEAT=N`             | `-r N`      | Repeat the whole suite `N` times. Useful for chasing intermittent races. |

### Example: verbose, errors group only, repeated 5x

```bash
west build -p always -b xg27_dk2602a -d build_unity_errors \
    tests/drivers/i2s/i2s_silabs_efr32_usart -- \
    -DCONFIG_I2S_EFR32_TEST_VERBOSE=y \
    -DCONFIG_I2S_EFR32_TEST_GROUP_FILTER='"i2s_efr32_errors"' \
    -DCONFIG_I2S_EFR32_TEST_REPEAT=5
```

Verbose output looks like this:

```
Unity test run 1 of 5
TEST(i2s_efr32_errors, txuf_underrun_transitions_to_error) PASS
TEST(i2s_efr32_errors, rxof_overrun_transitions_to_error) PASS
TEST(i2s_efr32_errors, prepare_after_underrun__back_to_ready_then_runs) PASS
-----------------------
3 Tests 0 Failures 0 Ignored
OK
```

> **Quoting on PowerShell.** String Kconfigs containing spaces / quotes
> need careful escaping: `'"…"'` works (single-quoted outer, double-quoted
> Kconfig value).

## Run via Twister (CI)

```bash
west twister -T tests/drivers/i2s/i2s_silabs_efr32_usart \
             --device-testing --device-serial COM3
```

For the loopback scenario (`drivers.i2s.silabs_efr32_usart.loopback`), pass
the fixture from `testcase.yaml` (wire **PC0 ↔ PC1**):

```bash
west twister -T tests/drivers/i2s/i2s_silabs_efr32_usart \
             --device-testing --device-serial COM3 \
             --fixture i2s_efr32_loopback_pc0_pc1
```

The Twister harness is `console` and it succeeds when it sees
`Unity run complete: 0 failure` on the serial log. Any failed Unity
assertion will print a diagnostic line containing the file:line and a
`FAIL` totals line, after which the regex never matches and Twister
flags the run as failed.

## Layout

```
tests/drivers/i2s/i2s_silabs_efr32_usart/
├── CMakeLists.txt        # Zephyr app + Unity sources + emlib include
├── Kconfig               # loopback toggle + frame clock + verbose/filter knobs
├── prj.conf              # printk + Unity output
├── README.md
├── testcase.yaml         # harness: console, regex on Unity totals
├── unity_config.h        # UNITY_OUTPUT_CHAR -> printk
├── boards/
│   └── xg27_dk2602a.overlay   # USART0 = I2S, PC0/PC1/PC2/PC3 (DK flash removed)
└── src/
    ├── common.{h,c}      # mem_slab (depth 8), default cfg, pattern fill/verify
    ├── main.c            # Zephyr main() + Kconfig->argv -> UnityMain(RunAllTests)
    ├── test_runners.c    # TEST_GROUP_RUNNER + RUN_TEST_CASE wiring
    ├── test_configure.c  # i2s_configure() accept/reject through real driver
    ├── test_config_get.c # i2s_config_get() readback after configure / deconfig
    ├── test_states.c     # state-machine transitions on real HW
    ├── test_io.c         # i2s_write()/i2s_read() validation predicates
    ├── test_errors.c     # ISR-driven ERROR + I2S_TRIGGER_PREPARE recovery
    ├── test_clock.c      # USART CLKDIV register sanity (Silabs-specific)
    ├── test_mclk.c       # External MCLK via silabs,cmu-clkout + PRS/TIMER1
    └── test_loopback.c   # end-to-end TX -> wire -> RX data integrity
```

## Devicetree overlay (DK2602A)

On the DK, USART0 is normally used for the on-board MX25R8035F SPI flash
(Thunderboard). The overlay deletes the `mx25r80` child node and the
`cs-gpios` property, then assigns `compatible = "silabs,efr32-usart-i2s"`
with DMA slots and the `i2s-node0` alias. Console stays on USART1.

The overlay also instantiates a `silabs,series-clock-clkout` node on **PB0**
(EXPCLK = HFRCODPLL / 4 = 19.2 MHz). That output is read by the
`i2s_efr32_mclk` group, which uses PRS routing + TIMER1 capture to verify
the actual frequency is within +/- 2 % of the nominal value. PB0 is not
part of the I2S pin set (PC0-PC3) so it does not collide with the
loopback wire. If a board removes the clkout node, the test self-skips
via `TEST_IGNORE_MESSAGE`.

### RB4194A overlay (`boards/xg27_rb4194a.overlay`)

The radio-board overlay uses the same **EXPCLK / prescaler 4 → 19.2 MHz**
clkout configuration, but routes the pin to **PD4** so it does not
conflict with I2S on **PA2/PA3/PA7/PA8**, console **PA5/PA6**, or **PB0–PB4**
(LED, buttons, I2C, VCOM-enable hog). The `i2s_efr32_mclk` test is
on-chip (PRS → TIMER1); you do **not** need to probe PD4 unless you want
to confirm the pad with a scope.

## Notes / assumptions

- **Output routing.** Upstream Unity ships
  `modules/lib/unity/src/unity_iostream.c` (after `west update`), which depends on
  Silabs `sl_iostream_*`. Those headers are not on the Zephyr include
  path, so we deliberately do **not** compile that file. Instead
  `unity_config.h` defines `UNITY_OUTPUT_CHAR` to call `printk`, which
  reaches the on-board VCOM through the regular Zephyr console driver.
- **Memory extension off.** `unity_fixture.h` includes `unity_memory.h`
  by default, which would override `malloc/free`. We pass
  `-DUNITY_FIXTURE_NO_EXTRAS` to disable that include and we do not
  compile `unity_memory.c`. None of the I2S tests need malloc/free
  tracking.
- **Per-test cleanup.** `i2s_efr32_test_reset_device()` is called from
  each `TEST_SETUP()` AND `TEST_TEAR_DOWN()`. Calling it from teardown
  as well guarantees that a failing test (which leaves the driver
  mid-state) does not contaminate the next group's first test.
- **One-shot suite setup.** Device-pointer / `device_is_ready` checks
  run once in `main()` before `UnityMain`, since Unity Fixture has no
  per-suite setup hook.
- **Mem-slab depth = 8.** `common.c` defines both `tx_mem_slab` and
  `rx_mem_slab` with 8 blocks, which is one more than the worst test
  needs (`write_timeout_when_no_dma_consumer` fills the driver's TX
  ring of `CONFIG_I2S_SILABS_EFR32_USART_TX_BLOCK_COUNT=4` blocks then
  retains one extra block to hit the sem-timeout path on the next
  write).
- **Test harness in `testcase.yaml`.** We use `harness: console` and a
  regex against the explicit summary line that `main()` prints, so
  Twister can flag pass/fail purely from serial output.
- **Float disabled.** `unity_config.h` sets `UNITY_EXCLUDE_FLOAT` and
  `UNITY_EXCLUDE_DOUBLE` to keep the binary small; none of the
  assertions in the I2S test plan use float types.
- **K_NO_WAIT semantics.** `read_no_data_no_wait__ebusy` expects
  `-EBUSY`, not `-EAGAIN`, because that is what `k_sem_take(K_NO_WAIT)`
  returns on an empty semaphore in Zephyr. `-EAGAIN` is only produced
  by a timeout that actually elapsed (`read_no_data_with_short_timeout`).
- **Error-injection method.** `test_errors.c` uses
  `USART_IntSet(I2S_BASE, USART_IF_TXUF | USART_IF_RXOF)` to pend the
  USART error-interrupt flags in software. We do NOT rely on the EFR32
  Series 2 USART asserting TXUF/RXOF on its own when started without a
  data path — in I2S master mode the controller simply stops shifting
  on an empty TX buffer (no underrun) and never receives without an
  external clock+data source. The IRQ is enabled by `hw_i2s_apply()`,
  so software-pending the IF flag runs the same `i2s_efr32_isr` path
  the hardware would have raised.
- **Verifying ERROR state.** The driver does not expose its internal
  state, so `test_errors.c` uses `I2S_TRIGGER_PREPARE` as a witness:
  the driver only accepts PREPARE when at least one stream is in
  ERROR. A 0 return therefore proves the ISR latched ERROR before we
  ran the assertion.
- **DMA-aligned write predicate.** `write_size_not_dma_aligned__einval`
  configures the TX direction with `channels=1` purely so we can issue
  a 2-byte write that satisfies the frame-size check (2 bytes) but
  violates the 4-byte DMA-data-size check. The driver still drives the
  USART in stereo mode internally (`init.mono = false`); the test only
  cares about the size predicate. If a future driver revision rejects
  `channels=1` at configure time, the test self-skips via
  `TEST_IGNORE_MESSAGE`.
- **Frame-clock floor.** The driver predicate is `frame_clk_freq < 8000`
  → `-EINVAL`. `sub_8k_frame_clk__rejected` uses 4000 Hz to stay
  unambiguously below that boundary; 8000 Hz itself is on the accepted
  side.
