#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "app.h"
#include "gps.h"

#define GPS_THREAD_STACK_SIZE 1024
#define GPS_THREAD_PRIORITY   5
#define UART_NODE DT_NODELABEL(usart1)
#define GPS_RING_SIZE 256
#define NMEA_LINE_SIZE 96
#define GGA_FIELD_COUNT 15

K_SEM_DEFINE(gps_sem, 0, 1);

static void gps_timer_cb(struct k_timer *timer)
{
    k_sem_give(&gps_sem);
}
K_TIMER_DEFINE(gps_timer, gps_timer_cb, NULL);
RING_BUF_DECLARE(gps_ring, GPS_RING_SIZE);
K_THREAD_STACK_DEFINE(gps_thread_stack, GPS_THREAD_STACK_SIZE);
static struct k_thread gps_thread_data;
static bool gps_started;

static void gps_thread_entry(void *p1, void *p2, void *p3);

static const struct device *uart = DEVICE_DT_GET(UART_NODE);
static char nmea_line[NMEA_LINE_SIZE];
static size_t line_len;
static bool discard_line;

static void uart_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    uart_irq_update(dev);

    while (uart_irq_rx_ready(dev))
	{
        uint8_t byte;
        int n = uart_fifo_read(dev, &byte, 1);

        if (n != 1)
		{
            break;
        }

        if (ring_buf_put(&gps_ring, &byte, 1) != 1)
		{
            discard_line = true;
        }
    }
}
static bool next_nmea_line(char *line, size_t capacity)
{
    uint8_t byte;

    if (line == NULL || capacity < 2) {
        return false;
    }

    while (ring_buf_get(&gps_ring, &byte, 1) == 1)
	{
        if (byte == '\n') {
            if (discard_line)
			{
                discard_line = false;
                line_len = 0;
                continue;
            }

            if (line_len > 0 && nmea_line[line_len - 1] == '\r')
			{
                line_len--;
            }

            if (line_len >= capacity)
			{
                line_len = 0;
                continue;
            }

            memcpy(line, nmea_line, line_len);
            line[line_len] = '\0';
            line_len = 0;
            return true;
        }
        if (discard_line)
		{
            continue;
        }

        if (line_len < sizeof(nmea_line) - 1)
		{
            nmea_line[line_len++] = (char)byte;
        } else
		{
            discard_line = true;
            line_len = 0;
        }
    }
    return false;
}

static bool parse_gga_line(const char *line,
                           int *fix,
                           int *satellites,
                           double *lat_nmea,
                           char *lat_hemisphere,
                           double *lon_nmea,
                           char *lon_hemisphere,
                           double *alt)
{
    char copy[NMEA_LINE_SIZE];
    char *fields[GGA_FIELD_COUNT];
    size_t len;
    size_t field_count = 0;

    if (line == NULL || fix == NULL || satellites == NULL ||
        lat_nmea == NULL || lat_hemisphere == NULL ||
        lon_nmea == NULL || lon_hemisphere == NULL ||
        alt == NULL) {
        return false;
    }

    len = strlen(line);
    if (len == 0 || len >= sizeof(copy)) {
        return false;
    }

    memcpy(copy, line, len + 1);

    /* Quitar checksum del último campo, si está presente. */
    char *star = strchr(copy, '*');
    if (star != NULL) {
        *star = '\0';
    }

    char *saveptr = NULL;
    char *token = strtok_r(copy, ",", &saveptr);

    while (token != NULL && field_count < GGA_FIELD_COUNT) {
        fields[field_count++] = token;
        token = strtok_r(NULL, ",", &saveptr);
    }

    /* Se accede hasta fields[10], por lo que se necesitan 11 campos. */
    if (field_count < 11 ||
        strlen(fields[0]) < 3 ||
        strcmp(fields[0] + strlen(fields[0]) - 3, "GGA") != 0) {
        return false;
    }

    if (fields[2][0] == '\0' || fields[3][0] == '\0' ||
        fields[4][0] == '\0' || fields[5][0] == '\0' ||
        fields[6][0] == '\0' || fields[7][0] == '\0' ||
        fields[9][0] == '\0') {
        return false;
    }

    char *end;

    *lat_nmea = strtod(fields[2], &end);
    if (end == fields[2] || *end != '\0') return false;
    *lat_hemisphere = fields[3][0];

    *lon_nmea = strtod(fields[4], &end);
    if (end == fields[4] || *end != '\0') return false;
    *lon_hemisphere = fields[5][0];

    long parsed_fix = strtol(fields[6], &end, 10);
    if (end == fields[6] || *end != '\0' || parsed_fix < 0 || parsed_fix > 8) {
        return false;
    }
    *fix = (int)parsed_fix;

    long parsed_satellites = strtol(fields[7], &end, 10);
    if (end == fields[7] || *end != '\0' ||
        parsed_satellites < 0 || parsed_satellites > 99) {
        return false;
    }
    *satellites = (int)parsed_satellites;

    *alt = strtod(fields[9], &end);
    if (end == fields[9] || *end != '\0') return false;

    return true;
}

static bool convert_gga_position(double lat_nmea,
                                 char lat_hemisphere,
                                 double lon_nmea,
                                 char lon_hemisphere,
                                 double *lat_degrees,
                                 double *lon_degrees)
{
    if (lat_degrees == NULL || lon_degrees == NULL) {
        return false;
    }

    int lat_deg = (int)(lat_nmea / 100.0);
    double lat_minutes = lat_nmea - (lat_deg * 100.0);

    int lon_deg = (int)(lon_nmea / 100.0);
    double lon_minutes = lon_nmea - (lon_deg * 100.0);

    if (lat_deg < 0 || lat_deg > 90 ||
        lon_deg < 0 || lon_deg > 180 ||
        lat_minutes < 0.0 || lat_minutes >= 60.0 ||
        lon_minutes < 0.0 || lon_minutes >= 60.0 ||
        (lat_deg == 90 && lat_minutes != 0.0) ||
        (lon_deg == 180 && lon_minutes != 0.0)) {
        return false;
    }

    double lat = lat_deg + lat_minutes / 60.0;
    double lon = lon_deg + lon_minutes / 60.0;

    if (lat_hemisphere == 'S') {
        lat = -lat;
    } else if (lat_hemisphere != 'N') {
        return false;
    }

    if (lon_hemisphere == 'W') {
        lon = -lon;
    } else if (lon_hemisphere != 'E') {
        return false;
    }

    *lat_degrees = lat;
    *lon_degrees = lon;
    return true;
}

static void gps_thread_entry(void *p1, void *p2, void *p3)
{
	while (1)
	{
		k_sem_take(&gps_sem, K_FOREVER);
		char line[NMEA_LINE_SIZE];
        while (next_nmea_line(line, sizeof(line)))
		{
			int fix;
			int sat;
			double lat_nmea;
			double lat_deg;
			char lat_h;
			double lon_nmea;
			double lon_deg;
			char lon_h;
			double alt;
			if (!parse_gga_line(line,&fix,&sat,&lat_nmea,
                                &lat_h,&lon_nmea,
                                &lon_h,&alt)) {
                continue;
            }
            if (fix == 0)
			{
                fsm_update_gps_data(0, (uint8_t)sat, 0.0, 0.0, 0.0f);
                continue;
            }
            if (!convert_gga_position(lat_nmea,lat_h,lon_nmea,
                                      lon_h,&lat_deg,
                                      &lon_deg))
			{
                continue;
            }

            fsm_update_gps_data(1, (uint8_t)sat,
                                lat_deg,lon_deg,
                                (float)alt);
        }
    }
}

int gps_init(void)
{
    if (!device_is_ready(uart)) {
        return -ENODEV;
    }

    if (gps_started) {
        return -EALREADY;
    }

    int ret = uart_irq_callback_user_data_set(uart, uart_isr, NULL);
    if (ret != 0) {
        return ret;
    }

    uart_irq_rx_enable(uart);

    k_timer_start(&gps_timer, K_SECONDS(1), K_SECONDS(1));

    k_tid_t tid = k_thread_create(&gps_thread_data,
                                  gps_thread_stack,
                                  K_THREAD_STACK_SIZEOF(gps_thread_stack),
                                  gps_thread_entry,
                                  NULL, NULL, NULL,
                                  GPS_THREAD_PRIORITY,
                                  0,
                                  K_NO_WAIT);
    if (tid == NULL) {
        uart_irq_rx_disable(uart);
        k_timer_stop(&gps_timer);
        return -ENOMEM;
    }

    gps_started = true;
    return 0;
}
