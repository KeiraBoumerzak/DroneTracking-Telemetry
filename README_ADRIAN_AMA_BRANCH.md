# Button Handling and Mode Control Module
# AMR BRANCH
# Author: Adrian

This README documents the **button handling and mode-control part** developed on the `AMA` branch.

The current branch focuses on detecting **short and long presses** from the B2 user button and preparing those events for the final NORMAL / BLUE / OFF control logic.

---

## 1. What this module does

This part of the project:

- configures the B2 user button
- uses GPIO interrupts on both button edges
- detects short presses
- detects long presses of at least 1 second
- prevents a long press from also becoming a short press on release
- signals button events using a Zephyr semaphore
- prepares the events for the final FSM

---

## 2. Main files

```text
src/
├── button_handler.c
├── button_handler.h
└── main.c
```

### `button_handler.c`

Contains:
- button GPIO configuration
- GPIO interrupt callback
- long-press timer
- short/long press detection
- semaphore signaling

### `button_handler.h`

Current public interface:

```c
int button_system_init(void);

extern struct k_sem button_sem;
extern int press_type;
```

Current event values:

```text
press_type = 1 -> short press
press_type = 2 -> long press
```

---

## 3. Button configuration

The B2 button is accessed through the Devicetree alias:

```c
#define USER_BUTTON DT_ALIAS(sw0)
```

The GPIO information is obtained with:

```c
static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET(USER_BUTTON, gpios);
```

Interrupts are enabled on both edges:

```c
gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
```

This allows the software to detect both:

```text
button press
button release
```

---

## 4. Short press detection

When the button is pressed, a one-shot timer starts.

If the button is released before the 1-second timeout:

```c
press_type = 1;
k_sem_give(&button_sem);
```

Flow:

```text
button pressed
      ↓
start 1-second timer
      ↓
released before timeout
      ↓
stop timer
      ↓
SHORT PRESS event
```

---

## 5. Long press detection

The long-press threshold is:

```c
#define LONG_PRESS_TIMEOUT_MS 1000
```

If the button remains pressed for 1 second, the timer callback generates the long-press event:

```c
long_press_triggered = true;
press_type = 2;
k_sem_give(&button_sem);
```

Flow:

```text
button pressed
      ↓
start timer
      ↓
1 second reached
      ↓
LONG PRESS event
```

The long press is detected at the threshold itself, not after release.

---

## 6. Preventing a second short-press event

The variable:

```c
static bool long_press_triggered = false;
```

remembers whether the long-press event already happened.

On release:

```c
if (!long_press_triggered)
{
    press_type = 1;
    k_sem_give(&button_sem);
}
```

Therefore:

```text
short press -> release generates SHORT

long press  -> timer generates LONG
            -> later release does not generate SHORT
```

---

## 7. Semaphore use

The button module uses:

```c
K_SEM_DEFINE(button_sem, 0, 1);
```

A short or long press gives the semaphore:

```c
k_sem_give(&button_sem);
```

The future control/FSM thread can wait with:

```c
k_sem_take(&button_sem, K_FOREVER);
```

and then inspect:

```c
press_type
```

to know which event occurred.

---

## 8. Intended FSM behavior

Expected transitions:

```text
NORMAL + short -> BLUE
BLUE   + short -> NORMAL
NORMAL + long  -> OFF
BLUE   + long  -> OFF
OFF    + short -> NORMAL
```

A control thread can use the event like this:

```c
while (1) {

    k_sem_take(&button_sem, K_FOREVER);

    if (press_type == 1) {
        /* Handle short press */
    }
    else if (press_type == 2) {
        /* Handle long press */
    }
}
```

---

## 9. Integration with the brightness module

The final FSM will connect the button events to the brightness subsystem.

### NORMAL

```c
brightness_sampling_start();
```

### BLUE

```c
brightness_sampling_stop();
rgb_set(0, 0, 1);
```

### OFF

```c
brightness_sampling_stop();
rgb_set(0, 0, 0);
```

Complete idea:

```text
B2 button
   ↓
GPIO interrupt
   ↓
short / long event
   ↓
button semaphore
   ↓
FSM / control thread
   ↓
NORMAL / BLUE / OFF
   ↓
brightness + RGB control
```

---

## 10. Button initialization

The module is initialized with:

```c
int button_system_init(void);
```

It performs:

```text
check GPIO device
      ↓
configure button as input
      ↓
initialize callback
      ↓
register callback
      ↓
enable edge interrupts
      ↓
initialize long-press timer
```

---

## 11. Current branch status

The button handler is implemented, but final application integration is still needed.

The current branch should still:

- call `button_system_init()` from the final `main.c`
- replace the old board-LED blink test in `main.c`
- add a control/FSM thread that waits on `button_sem`
- connect the FSM to brightness and RGB control

The current `main.c` is still the original LED blink test, so it is not yet the final application logic.

---

## 12. CMake integration

Current branch:

```cmake
target_sources(app PRIVATE
    src/main.c
    src/button_handler.c
)
```

After final integration:

```cmake
target_sources(app PRIVATE
    src/main.c
    src/button_handler.c
    src/ldr.c
    src/rgb.c
    src/brightness_thread.c
)

target_include_directories(app PRIVATE src)
```

---

## 13. Test checklist

### Short press
- [ ] press and release before 1 second
- [ ] `press_type` becomes `1`
- [ ] `"Short Press"` is printed
- [ ] semaphore is given

### Long press
- [ ] hold B2 for at least 1 second
- [ ] event happens at the 1-second threshold
- [ ] `press_type` becomes `2`
- [ ] `"Long press"` is printed
- [ ] semaphore is given

### Long release
- [ ] release after long press
- [ ] no extra short-press event appears

### Final integration
- [ ] short press toggles NORMAL / BLUE
- [ ] long press from NORMAL or BLUE enters OFF
- [ ] short press from OFF returns to NORMAL

---

## 14. Design summary

The button subsystem uses:

- GPIO interrupts
- a Zephyr one-shot timer
- a semaphore
- a long-press flag

Main idea:

```text
press
  ↓
start timer
  ↓
release before 1 s -> SHORT

or

hold for 1 s -> LONG
```

The button module detects the user input, while the final FSM decides what the rest of the system should do.
