/* gps.c - GPS on USART1, no thread.
 * TODO: the interrupt collects a line, a queue + work item read it,
 * puting position and fix in the shared data. */
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include "app.h"
#include "gps.h"

#define GPS_THREAD_STACK_SIZE 1024
#define GPS_THREAD_PRIORITY   5
#define UART_NODE DT_NODELABEL(usart1)
#define GPS_RING_SIZE 256
#define NMEA_LINE_SIZE 96
#define GGA_FIELD_COUNT 15

static void gps_timer_cb(struct k_timer *timer)
{
    k_sem_give(&gps_sem);
}

K_SEM_DEFINE(gps_sem, 0, 1);
K_TIMER_DEFINE(gps_timer, gps_timer_cb, NULL);
RING_BUF_DECLARE(gps_ring, GPS_RING_SIZE);
K_THREAD_DEFINE(gps_thread_id,
                GPS_THREAD_STACK_SIZE,
                gps_thread_entry,
                NULL,
                NULL,
                NULL,
                GPS_THREAD_PRIORITY,
                0,
                0);

static const struct device *uart = DEVICE_DT_GET(UART_NODE);
static uint8_t ring_storage[GPS_RING_SIZE];
static char nmea_line[NMEA_LINE_SIZE];
static size_t line_len;

static void uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);
	uart_irq_update(dev);

	while (uart_irq_rx_ready(dev))
	{
		uint8_t byte;
		int n = uart_fifo_read(dev, &byte, 1);

		if ( n == 1)
		{
			ring_buf_put(&gps_ring, &byte, 1);
		}
	}
}

bool next_nmea_line(char *line, size_t capacity)
{
	uint8_t byte;
	while(ring_buf_get(&gps_ring, &byte, 1) == 1)
	{
		if (byte == '\r')
		{
            continue;
        }
		if (byte == '\n')
		{
            line[line_len] = '\0';
            line_len = 0;
            return true;
        }
		if (line_len < capacity - 1)
		{
            line[line_len++] = (char)byte;
        } else
		{
            line_len = 0;
        }
	}
}

bool parse_gga_line(const char *line,
					int *fix,
					int *sat,
					double *lat_nmea,
					char *lat_h,
					double *lon_nmea,
					char *lon_h,
					double *alt)
{
	char aux[NMEA_LINE_SIZE];
	char *fields[GGA_FIELD_COUNT];
	int field_count = 0;

	strcpy(aux, line);

	char *saveptr = NULL;
    char *token = strtok_r(aux, ",", &saveptr);

	while(token != NULL && field_count < GGA_FIELD_COUNT)
	{
		fields[field_count++] = token;
		token = strtok_r(NULL, ",", &saveptr);
	}

	if (field_count < GGA_FIELD_COUNT || strlen(fields[0]) < 3 ||
        strcmp(fields[0] + strlen(fields[0]) - 3, "GGA") != 0)
	{
        return false;
    }

	if (fields[2][0] == '\0' || fields[3][0] == '\0' ||
        fields[4][0] == '\0' || fields[5][0] == '\0' ||
        fields[6][0] == '\0' || fields[7][0] == '\0' ||
        fields[9][0] == '\0')
	{
        return false;
    }

    char *end;

    *lat_nmea = strtod(fields[2], &end);
    if (*end != '\0') return false;
    *lat_h = fields[3][0];

    *lon_nmea = strtod(fields[4], &end);
    if (*end != '\0') return false;
    *lon_h = fields[5][0];

    *fix = (int)strtol(fields[6], &end, 10);
    if (*end != '\0') return false;

    *sat = (int)strtol(fields[7], &end, 10);
    if (*end != '\0') return false;

    *alt = strtod(fields[9], &end);
    if (*end != '\0' && *end != '*') return false;

    return true;

}

static bool convert_gga_position(double lat_nmea,
                                 char lat_h,
                                 double lon_nmea,
                                 char lon_h,
                                 char alt_unit,
                                 double *lat_deg,
                                 double *lon_deg)
{
	int lat_degrees = (int)(lat_nmea / 100.0);
    double lat_minutes = lat_nmea - (lat_degrees * 100.0);

    int lon_degrees = (int)(long_nmea / 100.0);
    double lon_minutes = long_nmea - (lon_degrees * 100.0);

	double lat = lat_degrees + lat_minutes / 60.0;
    double lon = lon_degrees + lon_minutes / 60.0;


    if (lat_h == 'S') {
        lat = -lat;
    } else if (lat_h != 'N') {
        return false;
    }

    if (lon_h == 'W') {
        lon = -lon;
    } else if (lon_h != 'E') {
        return false;
    }

    if (alt_unit != 'M') {
        return false;
    }
	*lat_deg = lat;
    *lon_deg = lon;
}

void gps_thread_entry(void *p1, void *p2, void *p3)
{
	uart_irq_callback_user_data_set(uart, uart_isr, NULL);
	uart_irq_rx_enable(uart);

	while (1)
	{
		k_sem_take(&gps_sem, K_FOREVER);
		char line[NMEA_LINE_SIZE];
        while (next_nmea_line(line, sizeof(line))) {
			int *fix;
			int *sat;
			double *lat_nmea;
			double *lat_deg;
			char *lat_h;
			double *lon_nmea;
			double *lon_deg;
			char *lon_h;
			double *alt;
			if(parse_gga_line(fix,sat,lat,lat_h,lon,lon_h,alt))
			{
				convert_gga_position(lat_nmea, lat_h, lon_nmea, lon_h, *lat_deg, *lon_deg, *alt_m)
			}
        }
	}
}

void gps_init()
{
	k_timer_start(&gps_timer, K_MSEC(1000), K_MSEC(1000));
}
