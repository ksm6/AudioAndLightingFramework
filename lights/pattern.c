#ifdef __cplusplus
extern "C" {
#endif

#include "pattern.h"

static ws2811_t*
get_ledstring_single(uint32_t led_count)
{
    ws2811_t *ledstring = (ws2811_t*)malloc(sizeof(ws2811_t));
    ledstring->freq = TARGET_FREQ;
    ledstring->dmanum = DMA;
    ledstring->channel[0].gpionum = GPIO_PIN_ONE;
    ledstring->channel[0].count = led_count;
    ledstring->channel[0].invert = 0;
    ledstring->channel[0].brightness = 100;
    ledstring->channel[0].strip_type = STRIP_TYPE;
    ledstring->channel[1].gpionum = 0;
    ledstring->channel[1].count = 0;
    ledstring->channel[1].invert = 0;
    ledstring->channel[1].brightness = 100;
    ledstring->channel[1].strip_type = STRIP_TYPE;
    return ledstring;
}

static ws2811_t*
get_ledstring_double(uint32_t ch1_led_count, uint32_t ch2_led_count)
{
    ws2811_t *ledstring_double = (ws2811_t*) malloc(sizeof(ws2811_t));

    ledstring_double->freq = TARGET_FREQ;
    ledstring_double->dmanum = DMA;
    ledstring_double->channel[0].gpionum = GPIO_PIN_ONE;
    ledstring_double->channel[0].count = ch1_led_count;
    ledstring_double->channel[0].invert = 0;
    ledstring_double->channel[0].brightness = 100;
    ledstring_double->channel[0].strip_type = STRIP_TYPE;

    ledstring_double->channel[1].gpionum = GPIO_PIN_TWO;
    ledstring_double->channel[1].count = 0; //ch2_led_count;
    ledstring_double->channel[1].invert = 0;
    ledstring_double->channel[1].brightness = 100;
    ledstring_double->channel[1].strip_type = STRIP_TYPE;
    return ledstring_double;
}

#ifdef __cplusplus
}
#endif
