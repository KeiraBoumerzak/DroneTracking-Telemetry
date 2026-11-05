#ifndef BRIGHTNESS_THREAD_H
#define BRIGHTNESS_THREAD_H


int brightness_thread_init(void);
/*
NORMAL -> brightness_sampling_start();
BLUE   -> brightness_sampling_stop();
OFF    -> brightness_sampling_stop();
*/
void brightness_sampling_start(void);
void brightness_sampling_stop(void);

#endif
