# T5 classical codec profiling — STM32F429ZI (NUCLEO-F429ZI)

Firmware-side benchmarks for **fixed int16 profile chunks** (`raw_copy`, `delta_int16`, `simple_predictor_pack`). This is **not** CCSDS-124 compliance testing. This project does **not** run AI or neural inference on the STM32; it only times lightweight encode/decode-style loops on static buffers.

## What you get

- CSV-like lines on UART (115200 baud) beginning with `T5PROFILE`, aligned with the ESP32 classical profiler column order.
- Timing via the **DWT cycle counter** when available, else **HAL_GetTick** (millisecond resolution; noted in the `timing_source=` field).
- **No dynamic allocation**; buffers are static.
- **RAM** line field: sum of the three profiling buffers only (not total MCU RAM use).
- **Flash** line field: estimate from linker symbols (`_etext`, `_sdata`, `_edata`, `FLASH_BASE`).

## STM32CubeIDE: create the project

1. **File → New → STM32 Project**.
2. In the **Board Selector**, choose **NUCLEO-F429ZI** (MCU **STM32F429ZIT6**).
3. Firmware **STM32Cube FW_F4** package; project type **STM32Cube**.
4. Finish the wizard; open **Pinout & Configuration**.

## USART (ST-Link virtual COM)

On **NUCLEO-F429ZI**, the ST-Link USB virtual COM port is typically wired to **USART3**:

- **TX**: PD8  
- **RX**: PD9  

In CubeMX:

1. Enable **USART3** in **Asynchronous** mode.
2. Set **Baud Rate** to **115200**, **8N1**, no flow control unless you need it.
3. Generate code.

Regenerate if you change clocks so `SystemCoreClock` matches the DWT-to-microseconds conversion.

## Copy generated profiling sources

From this repository folder [`firmware/stm32f429zi_t5_profile/`](.), copy into your CubeIDE project (replace `YourProject`):

| Source in repo | Destination in Cube project |
|----------------|-------------------------------|
| `Core/Inc/t5_profile_vectors.h` | `Core/Inc/t5_profile_vectors.h` |
| `Core/Inc/t5_profile_methods.h` | `Core/Inc/t5_profile_methods.h` |
| `Core/Src/t5_profile_methods.c` | `Core/Src/t5_profile_methods.c` |
| `Core/Src/t5_main_patch.c` | `Core/Src/t5_main_patch.c` |

Add the two `.c` files to the build (right-click **Application/User** → **Add/Remove Files** or drag into `Core/Src` with “copy” if needed). Include paths should already cover `Core/Inc`.

## Enable `printf` over UART

1. In **Project → Properties → C/C++ Build → Settings → Tool Settings → MCU Settings**, enable **Use float with printf** if you rely on `%f` for the compression ratio field.

2. Retarget `write()` so `printf` goes to **USART3**. Typical pattern (adjust `huart3` to match your handle name in `usart.c`):

```c
/* syscalls.c or a small t5_syscalls_uart.c compiled into the project */
#include "stm32f4xx_hal.h"
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

extern UART_HandleTypeDef huart3;

int _write(int file, char *ptr, int len)
{
  (void)file;
  if (ptr == NULL || len <= 0) {
    return 0;
  }
  HAL_UART_Transmit(&huart3, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
  return len;
}
```

3. Ensure **USART3** is initialized before any `printf` (Cube `MX_USART3_UART_Init()` runs before your code calls the profiler).

## Hook from `main.c`

After `HAL_Init()`, `SystemClock_Config()`, and all `MX_*_Init()` calls (including UART):

```c
#include "t5_profile_methods.h"

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART3_UART_Init();
  /* ... */

  T5_Profile_AfterInit(); /* 3 s delay, then runs all methods */

  while (1) {
  }
}
```

You may call `T5_Profile_RunAll()` directly instead if you handle the boot delay yourself.

## Capture logs on Windows

1. Connect the Nucleo USB **ST-Link** port; install ST drivers if needed.
2. **Device Manager → Ports (COM & LPT)** — note the COM number (e.g. `COM7`).
3. Open **PuTTY** or **Tera Term** as **Serial**, **115200**, 8 data bits, no parity, 1 stop bit.
4. Reset the board; copy lines starting with `T5PROFILE` into a `.csv` or spreadsheet if you merge results offline.

## Troubleshooting

- **No output**: Confirm `huart3` in `_write` matches Cube (`usart.h`), correct USART instance, and pins PD8/PD9.
- **Link errors for `_etext` / `_sdata` / `_edata`**: Your linker script may use different symbol names; adjust `t5_flash_estimate_bytes()` in `t5_profile_methods.c` to match the `.ld` file from Cube (or use the `.map` file to pick the end of loaded flash).
- **Coarse timings** (`timing_source=hal_tick_fallback`): DWT was not enabled or failed the sanity check; timing columns are still in microseconds but only ~1 ms resolution.
