#include <Arduino.h>
#include "controller.h"

// With LWIP core locking, a client.write() runs the whole TCP/IP and WiFi
// transmit path inline on the loop task, which is a deep call chain. Together
// with the JPEG encode in the same loop, the default 8 KB loop stack is tight,
// so give it headroom.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

void setup()
{
    gController.initialize();
}

void loop()
{
    gController.execute();
}