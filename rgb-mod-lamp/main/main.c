#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

void my_task( void *pvParameters )
{
    while(1){
        vTaskDelay(pdMS_TO_TICKS(100) );
    }
}


void app_main( void )
{
    xTaskCreatePinnedToCore(
        my_task,
        "my_task",
        4096,
        NULL,
        5,
        NULL,
        1   //Core ID : 0 = pro_cpu , 1 = APP_CPU tskNO_AFFINITY for default handlig by scheduler
    );

}
