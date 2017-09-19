#ifndef _RCIO_H
#define _RCIO_H

#include <linux/mutex.h>

typedef enum
{
	NAVIO2 = 0x0,
	EDGE = 0x01,
	NEW_BOARD1  = 0x02,
	NEW_BOARD2 = 0x03,
	UNKNOWN_BOARD = 0x04

} board_type_t;

#define NAVIO2_ADC_CHANNELS_COUNT 6
#define NAVIO2_PWM_CHANNELS_COUNT 14

#define EDGE_ADC_CHANNELS_COUNT 8
#define EDGE_PWM_CHANNELS_COUNT 16

struct rcio_state
{
    struct kobject *object;
    struct rcio_adapter *adapter;
    int (*register_set)(struct rcio_state *state, u8 page, u8 offset, const u16 *values, u8 num_values);
    int (*register_get)(struct rcio_state *state, u8 page, u8 offset, u16 *values, u8 num_values);
    int (*register_set_byte)(struct rcio_state *state, u8 page, u8 offset, u16 value);
    u16 (*register_get_byte)(struct rcio_state *state, u8 page, u8 offset);
    int (*register_modify)(struct rcio_state *state, u8 page, u8 offset, u16 clearbits, u16 setbits);
    
    board_type_t board_type;
    int adc_channels_count;
    int pwm_channels_count;
};

struct rcio_adapter {
    void *client;
    struct device *dev;
    struct mutex lock;

    int (*read)(struct rcio_adapter *state, u16 address, char *buffer, size_t length); 
    int (*write)(struct rcio_adapter *state, u16 address, const char *buffer, size_t length); 
};

int rcio_probe(struct rcio_adapter *state);
int rcio_remove(struct rcio_adapter *state);

#endif /* _RCIO_H */
