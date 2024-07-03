#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "vvtk_io.h"
int led_setStatus(vvtk_led_status_t led)
{
    int err;
    vvtk_led_status_t tmp;
    err = vvtk_set_led_status(led);
    if (err)
    {
        printf("[led] set status error %d ------------------------------- \n", err);
        return err;
    }
    return 0;
}
int test_led_status()
{
    vvtk_led_status_t led;
    led.color = VVTK_LED_COLOR_GREEN;
    led.mode = VVTK_LED_MODE_BLINKING;
    led.interval = 500;
    printf("[led] test green blink 0.5s ++++ \n");
    led_setStatus(led);
    return 0;
}

int main(int argn, char **agrv)
{
    printf("Agent Start....\n\n");
    test_led_status();

    while (1)
    {
        sleep(1);
    }

    return -1;
}
