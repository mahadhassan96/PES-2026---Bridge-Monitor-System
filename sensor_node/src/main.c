#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h> 

#define DELAY_2_MS  100
#define STACK_SIZE  500 

#ifndef SENSOR_CHAN_FORCE
#define SENSOR_CHAN_FORCE SENSOR_CHAN_PRIV_START
#endif

int main() 
{
  const struct device *const dev = DEVICE_DT_GET(DT_NODELABEL(fsr_sensor));

  if(!device_is_ready(dev)) {
    printk("Sensor device not ready\n");
    return 0;
  }
  struct sensor_value force;
  for(;;){
    sensor_sample_fetch(dev);
    sensor_channel_get(dev, SENSOR_CHAN_FORCE, &force);

    printk("Force [g]: %d\n", force.val1);
    k_sleep(K_MSEC(1500));
  }

  return 0;
}