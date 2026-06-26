#include <Arduino.h>
#include "controller.h"

// The MJPEG encode (fmt2jpg) runs in the Arduino loop task and is stack heavy,
// especially on busy frames. The default 8 KB loop stack can overflow into
// adjacent heap, which then shows up as a fault in an unrelated ISR. Give it
// room.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

void setup()
{
    gController.initialize();
}

void loop()
{
    gController.execute();
}