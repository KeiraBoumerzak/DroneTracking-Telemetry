#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>

#include "ldr.h"

static const struct adc_dt_spec ldr_adc =
    ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

static int16_t adc_sample;

static struct adc_sequence adc_sequence = {
    .buffer = &adc_sample,
    .buffer_size = sizeof(adc_sample),
};

int ldr_init(void)
{
    int ret;

    if (!adc_is_ready_dt(&ldr_adc)) {
        printk("[ERROR] ADC device not ready\n");
        return -ENODEV;
    }

    ret = adc_channel_setup_dt(&ldr_adc);
    if (ret < 0) {
        printk("[ERROR] ADC channel setup failed: %d\n", ret);
        return ret;
    }

    ret = adc_sequence_init_dt(&ldr_adc, &adc_sequence);
    if (ret < 0) {
        printk("[ERROR] ADC sequence setup failed: %d\n", ret);
        return ret;
    }

    return 0;
}

int ldr_read_raw(uint16_t *raw_value)
{
    int ret;

    if (raw_value == NULL) {
        return -EINVAL;
    }

    ret = adc_read_dt(&ldr_adc, &adc_sequence);
    if (ret < 0) {
        return ret;
    }

    if (adc_sample < 0) {
        *raw_value = 0U;
    } else {
        *raw_value = (uint16_t)adc_sample;
    }

    return 0;
}