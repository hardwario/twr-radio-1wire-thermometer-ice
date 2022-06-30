#include <application.h>
#include <at.h>

#define DS18B20_PUB_NO_CHANGE_INTERVAL   (5 * 60 * 1000)
#define DS18B20_PUB_VALUE_CHANGE         0.5f

#define TEMPERATURE_UPDATE_INTERVAL     (10 * 1000)
#define BATTERY_UPDATE_INTERVAL         (5 * 60 * 1000)

#define RESET_SEND_SIGNAL_TIME (30 * 1000)

twr_lis2dh12_t lis2dh12;

int special_alarm_count = 0;

bool not_sended = true;

twr_lis2dh12_alarm_t alarm;

// LED instance
twr_led_t led;

// ds18b20 library instance
static twr_ds18b20_t ds18b20;
float last_temperature;
twr_tick_t temperature_next_pub;

twr_scheduler_task_id_t reset_send_signal_task;
twr_scheduler_task_id_t battery_measure_task_id;

void ds18b20_event_handler(twr_ds18b20_t *self, uint64_t device_address, twr_ds18b20_event_t event, void *event_param)
{
    if (event == TWR_DS18B20_EVENT_UPDATE)
    {
        float measured_temp;
        if (twr_ds18b20_get_temperature_celsius(self, device_address, &measured_temp))
        {
            if ((fabs(measured_temp - last_temperature) >= DS18B20_PUB_VALUE_CHANGE) || (temperature_next_pub < twr_scheduler_get_spin_tick()))
            {
                twr_radio_pub_temperature(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT, &measured_temp);
                last_temperature = measured_temp;
                temperature_next_pub = twr_scheduler_get_spin_tick() + DS18B20_PUB_NO_CHANGE_INTERVAL;
            }
        }
    }
}

void lis2_event_handler(twr_lis2dh12_t *self, twr_lis2dh12_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;
    if (event == TWR_LIS2DH12_EVENT_ALARM) {
        if(special_alarm_count > 6)
        {
            send_measurements();
        }
        else
        {
            special_alarm_count++;
        }
        twr_log_debug("SPECIAL ALARM");
    }
    else {
        twr_log_debug("error");
    }
}

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    float voltage;
    //int percentage;

    if(event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (twr_module_battery_get_voltage(&voltage))
        {
            twr_radio_pub_battery(&voltage);
        }
    }
}

void send_measurements()
{
    if(not_sended)
    {
        not_sended = false;
        special_alarm_count = 0;
        last_temperature = 0;
        twr_ds18b20_measure(&ds18b20);
        twr_module_battery_measure();

        twr_scheduler_plan_relative(reset_send_signal_task, RESET_SEND_SIGNAL_TIME);
    }
}

void reset_send_signal()
{
    not_sended = true;
}

void application_init(void)
{
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    alarm.x_high = true;
    alarm.y_high = true;
    alarm.threshold = 800;

    twr_lis2dh12_init(&lis2dh12, TWR_I2C_I2C0, 0x19);
    twr_lis2dh12_set_alarm(&lis2dh12, &alarm);
    twr_lis2dh12_set_event_handler(&lis2dh12, lis2_event_handler, NULL);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize Sensor Module
    twr_module_sensor_init();

    // Initialize 1-Wire temperature sensors
    twr_ds18b20_init_single(&ds18b20, TWR_DS18B20_RESOLUTION_BITS_12);
    twr_ds18b20_set_event_handler(&ds18b20, ds18b20_event_handler, NULL);
    twr_ds18b20_set_update_interval(&ds18b20, TEMPERATURE_UPDATE_INTERVAL);

    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);
    twr_radio_pairing_request("1wire-thermometer", VERSION);

    twr_led_pulse(&led, 2000);
}
