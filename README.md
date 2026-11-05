# Brightness Monitoring and Control System using Zephyr RTOS
## Contributors

TeikenTTK : Adrián Martínez Anchuelo 
Keiraboumerzak : Keira Boumerzak

## Project Overview

This project implements a brightness monitoring and control system on the **NUCLEO-WL55JC** board using **Zephyr RTOS**.

The system uses:

- an LDR light sensor connected to the ADC,
- an external common-anode RGB LED,
- the B2 user button,
- Zephyr threads,
- a timer,
- a semaphore,
- a message queue,
- an FSM with NORMAL, BLUE and OFF modes.

The application was developed in separate modules so that the sensor, RGB LED, brightness thread, button handling and FSM remain independent and easier to test.

---

## Final System Behaviour

### NORMAL mode

In NORMAL mode:

- the LDR is sampled every 2 seconds,
- the raw 12-bit ADC value is converted to a brightness percentage,
- the RGB LED shows the brightness range,
- serial output displays the ADC value, brightness percentage and selected colour.

Brightness mapping:

- brightness < 33% -> RED
- 33% <= brightness <= 66% -> YELLOW
- brightness > 66% -> GREEN

The system starts in NORMAL mode.

---

### BLUE mode

A short press on B2 changes:

NORMAL -> BLUE

In BLUE mode:

- the RGB LED remains BLUE,
- brightness sampling is suspended,
- no new ADC measurements should be produced.

Another short press changes:

BLUE -> NORMAL

---

### OFF mode

A long press of at least 1 second from NORMAL or BLUE changes the system to OFF.

In OFF mode:

- brightness sampling is suspended,
- the RGB LED is turned off,
- the system remains waiting for another button event.

A short press in OFF returns the system to NORMAL.

---

## Hardware

### NUCLEO-WL55JC

The project runs on:

- STM32 NUCLEO-WL55JC

### LDR

The LDR is read through the ADC.

Configured ADC characteristics:

- resolution: 12 bits
- full-scale value: 4095
- ADC channel: ADC1 channel 5
- board pin: A0 / PB1

The measured circuit orientation gives:

- more light -> lower ADC value
- less light -> higher ADC value

For this reason, the ADC percentage is inverted in software.

Brightness calculation:

```c
brightness =
    100U -
    ((raw * 100U + ADC_MAX_VALUE / 2U) / ADC_MAX_VALUE);
```

---

## RGB LED

The project uses an external common-anode RGB LED.

Connections used by the project:

- common anode -> 3.3 V
- RED -> D6
- GREEN -> D5
- BLUE -> D3

Each colour channel is controlled as a digital GPIO output.

Because the LED is common-anode, the GPIOs are defined as active-low in the devicetree overlay.

This lets the application use simple logical calls such as:

```c
rgb_set(1, 0, 0);
```

for RED without manually inverting the output inside the application logic.

---

## Button

The B2 user button is accessed using:

```c
#define USER_BUTTON DT_ALIAS(sw1)
```

The button interrupt is configured for both edges:

```c
GPIO_INT_EDGE_BOTH
```

The two edges are used to detect:

- button press,
- button release.

A Zephyr timer is used to distinguish short and long presses.

Long-press threshold:

```c
#define LONG_PRESS_TIMEOUT_MS 1000
```

A long press is generated when the 1-second threshold is reached, not when the button is released.

The `long_press_triggered` flag prevents the release of a long press from also being interpreted as a short press.

---

## Final Button Communication Design

The initial version used:

```c
int press_type;
K_SEM_DEFINE(button_sem, 0, 1);
```

The semaphore indicated that something happened, while `press_type` stored whether the event was SHORT or LONG.

The final version uses a Zephyr message queue instead:

```c
K_MSGQ_DEFINE(button_msgq,
              sizeof(button_event_t),
              4,
              4);
```

Button events are defined as:

```c
typedef enum {
    BUTTON_EVENT_SHORT_PRESS,
    BUTTON_EVENT_LONG_PRESS
} button_event_t;
```

The button module now sends the complete event directly to the FSM.

Example:

```c
button_event_t event = BUTTON_EVENT_SHORT_PRESS;

k_msgq_put(&button_msgq,
           &event,
           K_NO_WAIT);
```

This removes the old shared `press_type` global variable.

The queue can store up to four pending button events.

This design is more appropriate because button events contain data: the event is either SHORT or LONG.

---

## ISR and Timer Callback Design

The button interrupt and long-press timer callback are intentionally kept short.

They only:

1. detect the event,
2. create the correct button event,
3. send it to the message queue,
4. return.

They do not perform FSM logic or application logging.

For example:

```c
button_event_t event = BUTTON_EVENT_LONG_PRESS;
k_msgq_put(&button_msgq, &event, K_NO_WAIT);
```

`K_NO_WAIT` is used because an ISR or timer callback must not block waiting for queue space.

Logging such as:

```c
printk("[BUTTON] Short press\n");
```

and:

```c
printk("[BUTTON] Long press\n");
```

is performed by the FSM thread instead.

This keeps interrupt-side processing short and moves application work into thread context.

---

## Button Initialization Error Handling

The button initialization function checks all important GPIO setup operations.

The final implementation checks:

- whether the GPIO device is ready,
- whether the input pin configuration succeeded,
- whether the callback was successfully registered,
- whether interrupt configuration succeeded.

Example:

```c
ret = gpio_add_callback(button.port, &button_cb);

if (ret < 0) {
    printk("[ERROR] Failed to add button callback: %d\n", ret);
    return ret;
}
```

and:

```c
ret = gpio_pin_interrupt_configure_dt(&button,
                                      GPIO_INT_EDGE_BOTH);

if (ret < 0) {
    printk("[ERROR] Failed to configure button interrupt: %d\n", ret);
    return ret;
}
```

This prevents the program from reporting successful button initialization when GPIO setup has actually failed.

---

## FSM

The FSM controls the application modes.

States:

```c
typedef enum {
    STATE_OFF,
    STATE_NORMAL,
    STATE_BLUE
} state_t;
```

Main transitions:

```text
NORMAL -- short --> BLUE
BLUE   -- short --> NORMAL

NORMAL -- long --> OFF
BLUE   -- long --> OFF

OFF    -- short --> NORMAL
```

The system starts in NORMAL mode.

The FSM is initialized only after the LDR, RGB LED, brightness module and button module have been initialized.

This avoids starting brightness acquisition before the hardware is ready.

---

## FSM and Button Message Queue

The FSM thread waits for button events using:

```c
k_msgq_get(&button_msgq,
           &button_event,
           K_FOREVER);
```

The thread then handles the event:

```c
if (button_event == BUTTON_EVENT_SHORT_PRESS) {
    printk("[BUTTON] Short press\n");
    fsm_change_state(EVENT_SHORT_PRESS);
}
else if (button_event == BUTTON_EVENT_LONG_PRESS) {
    printk("[BUTTON] Long press\n");
    fsm_change_state(EVENT_LONG_PRESS);
}
```

This gives the following architecture:

```text
B2 button
   |
   v
GPIO ISR / long-press timer
   |
   v
BUTTON_EVENT_SHORT_PRESS
or
BUTTON_EVENT_LONG_PRESS
   |
   v
Zephyr message queue
   |
   v
FSM thread
   |
   v
NORMAL / BLUE / OFF
```

---

## Brightness Thread

Brightness monitoring runs in its own Zephyr thread.

The thread is created with:

```c
K_THREAD_DEFINE(...)
```

The thread does not continuously poll the sensor.

Instead, it waits for a semaphore:

```c
k_sem_take(&brightness_sem, K_FOREVER);
```

The semaphore is released by a periodic Zephyr timer every 2 seconds while NORMAL mode is active.

Architecture:

```text
2-second timer
      |
      v
brightness semaphore
      |
      v
brightness thread
      |
      v
ADC read
      |
      v
brightness calculation
      |
      v
RGB update
```

A semaphore is appropriate here because the timer event contains no additional information. It simply means:

"perform one brightness sample."

---

## Sampling Synchronization Improvement

During testing, a final brightness reading could occasionally appear around a mode change such as:

```text
[FSM] NORMAL --> BLUE
[BRIGHTNESS] ...
[BRIGHTNESS] Sampling suspended
```

This could happen because the timer had already released the semaphore before the FSM disabled sampling.

To improve this behaviour, the final design adds an atomic sampling flag:

```c
static atomic_t sampling_enabled = ATOMIC_INIT(0);
```

NORMAL mode enables it:

```c
atomic_set(&sampling_enabled, 1);
```

BLUE and OFF clear it:

```c
atomic_clear(&sampling_enabled);
```

Immediately after waking from the semaphore, the brightness thread checks:

```c
if (!atomic_get(&sampling_enabled)) {
    continue;
}
```

Therefore, if an old semaphore event is pending after sampling has been disabled, the thread goes back to waiting instead of starting another ADC measurement.

The stop function also:

```c
k_timer_stop(&brightness_timer);
k_sem_reset(&brightness_sem);
```

so future timer events stop and old pending semaphore events are removed.

---

## Why Different RTOS Communication Mechanisms Are Used

The project intentionally uses two different Zephyr mechanisms.

### Brightness thread -> semaphore

The brightness timer only needs to say:

"take one sample."

There is no payload, so a semaphore is suitable.

### Button -> message queue

The button event has a type:

- SHORT,
- LONG.

Because information must be transferred with the event, a message queue is more appropriate.

A concise explanation for the oral exam is:

> The brightness timer event carries no data, so a semaphore is sufficient. Button events carry an event type, SHORT or LONG, so a message queue is used to transfer that information directly to the FSM.

---

## Source Files

The project is divided into modules.

```text
src/
|-- main.c
|-- ldr.c
|-- ldr.h
|-- rgb.c
|-- rgb.h
|-- brightness_thread.c
|-- brightness_thread.h
|-- button_handler.c
|-- button_handler.h
|-- fsm.c
`-- fsm.h
```

Responsibilities:

- `main.c` -> initializes all modules
- `ldr.c/.h` -> ADC and LDR access
- `rgb.c/.h` -> RGB GPIO control
- `brightness_thread.c/.h` -> periodic brightness monitoring
- `button_handler.c/.h` -> B2 interrupt and short/long press detection
- `fsm.c/.h` -> application modes and transitions

---

## Startup Order

The final startup order is:

```text
ldr_init()
   |
   v
rgb_init()
   |
   v
brightness_thread_init()
   |
   v
button_system_init()
   |
   v
fsm_init()
```

`fsm_init()` is intentionally last because it starts the application in NORMAL mode and enables brightness sampling.

---

## Serial Output

Typical startup output:

```text
[INIT] Brightness monitoring system starting
[INIT] Brightness thread ready
[INIT] Button handler initialized
[BRIGHTNESS] Sampling enabled
[MODE] System NORMAL
[INIT] System initialized successfully
```

Typical NORMAL output:

```text
[BRIGHTNESS] t=... ms | ADC=.../4095 | Brightness=...% | RGB=GREEN
```

Example button transitions:

```text
[BUTTON] Short press
[FSM] NORMAL --> BLUE
[BRIGHTNESS] Sampling suspended
```

```text
[BUTTON] Short press
[FSM] BLUE --> NORMAL
[BRIGHTNESS] Sampling enabled
```

```text
[BUTTON] Long press
[FSM] NORMAL --> OFF
[BRIGHTNESS] Sampling suspended
```

---

## Final Functional Test

The final tested sequence is:

```text
startup      -> NORMAL
short press  -> BLUE
short press  -> NORMAL
long press   -> OFF
short press  -> NORMAL
```

Expected behaviour:

```text
NORMAL -> ADC every ~2 seconds and RGB follows brightness
BLUE   -> RGB remains BLUE and ADC sampling is suspended
OFF    -> RGB off and ADC sampling is suspended
```

The final integrated version was tested successfully on the board.

---

## Build

After the project is configured, build with:

```powershell
cmake --build build
```

The generated Zephyr firmware is located in:

```text
build/zephyr/
```

---

## Git Workflow

Development was performed using separate Git branches.

- `LDR` -> LDR, ADC, RGB and brightness-thread work
- `AMA` -> integrated branch containing button handling, FSM and combined system testing
- `main` -> final tested project

A suitable commit for the final synchronization and button refactor is:

```bash
git add .
git commit -m "refactor: improve button events and mode synchronization"
git push origin AMA
```

---

## Final Architecture Summary

```text
                     +-------------------+
                     |      B2 Button    |
                     +---------+---------+
                               |
                               v
                     +-------------------+
                     | GPIO ISR / Timer  |
                     +---------+---------+
                               |
                               v
                     +-------------------+
                     |     k_msgq        |
                     | SHORT / LONG      |
                     +---------+---------+
                               |
                               v
                     +-------------------+
                     |    FSM Thread     |
                     +----+---------+----+
                          |         |
               NORMAL ----+         +---- BLUE / OFF
                 |
                 v
        +-------------------+
        | 2-second timer    |
        +---------+---------+
                  |
                  v
        +-------------------+
        | brightness_sem    |
        +---------+---------+
                  |
                  v
        +-------------------+
        | Brightness Thread |
        +---------+---------+
                  |
                  v
             ADC -> RGB
```

This final design separates hardware interrupt handling, event communication, application state control and sensor processing into dedicated modules and Zephyr synchronization mechanisms.
