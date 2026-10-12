#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "hal/ledc_types.h"
#include "portmacro.h"
#include "sdkconfig.h"
#include "soc/clk_tree_defs.h"
#include <ctype.h>
#include <stdint.h>
#include <esp_log.h>
#include <stdio.h>
#include <string.h>


#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_FREQUENCY 5000
#define LEDC_RESOLUTION LEDC_TIMER_8_BIT

#define GPIO_RED 25
#define GPIO_BLUE 26
#define GPIO_GREEN 27

static const char *TAG  = "mood_lamp" ;

typedef enum {
    MODE_OFF,
    MODE_RED,
    MODE_GREEN,
    MODE_BLUE,
    MODE_RAINBOW,
    MODE_PULSE,
    MODE_BREATH
} led_mode_t ;

static QueueHandle_t mode_queue ;

void ledc_conftimer(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_MODE ,
        .duty_resolution = LEDC_RESOLUTION ,
        .timer_num = LEDC_TIMER ,
        .freq_hz = LEDC_FREQUENCY ,
        .clk_cfg = LEDC_AUTO_CLK ,
    };

    ledc_timer_config( &timer_conf );

};

void ledc_assignchannel( int * gpios ){
    for ( int ch = 0 ; ch < (int)sizeof(gpios)/(int)sizeof(gpios[0]) ; ch++ )
    {
        ledc_channel_config_t ch_conf={
            .speed_mode = LEDC_MODE,
            .channel = ( ledc_channel_t )ch,
            .timer_sel = LEDC_TIMER ,
            .intr_type = LEDC_INTR_DISABLE ,
            .gpio_num = gpios[ ch ],
            .duty = 0 ,
            .hpoint = 0 ,
        };
        ledc_channel_config( &ch_conf );
    }

}

void set_rgb( uint8_t r , uint8_t g , uint8_t b )
{
    ledc_set_duty( LEDC_MODE, LEDC_CHANNEL_0 , r );
    ledc_update_duty( LEDC_MODE, LEDC_CHANNEL_0 );

    ledc_set_duty( LEDC_MODE, LEDC_CHANNEL_1 , b );
    ledc_update_duty( LEDC_MODE, LEDC_CHANNEL_1 );

    ledc_set_duty( LEDC_MODE, LEDC_CHANNEL_2 , g );
    ledc_update_duty( LEDC_MODE, LEDC_CHANNEL_2 );
}

void hsv_to_rgb( uint16_t h , uint8_t s , uint8_t v,  //( Hue, Saturation, Value )
                uint8_t *r , uint8_t *b , uint8_t *g )
{
    if( s==0 )
    {
        *r = *g = *b = v ;
        return ;
    }

    uint8_t region = h /60 ;
    uint8_t remainder = ( h - ( region * 60)) * 255 / 60 ;

    uint8_t p = ( v * (255 - s )) >> 8 ;
    uint8_t q = ( v * ( 255 - (( s * remainder >> 8))) >> 8 );
    uint8_t t = ( v * ( 255 - (( s * (255 - remainder)) >> 8 ))) >> 8;

    switch ( region ){
        case 0 : *r = v ; *g = t ; *b = p ; break ;
        case 1 : *r = q ; *g = v; *b = p; break;
        case 2:  *r = p; *g = v; *b = t; break;
        case 3:  *r = p; *g = q; *b = v; break;
        case 4:  *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break ;
    }

}

static void animation_task( void *pvParameters )
{
    led_mode_t current_mode = MODE_RAINBOW ;
    uint16_t hue = 0 ;
    uint8_t pulse_brightness = 0 ;
    int8_t pulse_direction = 1 ;

    ESP_LOGI( TAG , "Animation task running on core %d",
             xPortGetCoreID());
    while(1)
    {
        led_mode_t new_mode ;
        if( xQueueReceive( mode_queue , &new_mode ,0 ) == pdTRUE )
        {
                current_mode = new_mode ;
                hue = 0 ;
                pulse_brightness = 0 ;
                pulse_direction = 1;
                ESP_LOGI( TAG ,"Mode changed to %d",current_mode );
        }

        uint8_t r = 0, g = 0, b = 0 ;

        switch( current_mode )
        {
            case MODE_OFF :
                set_rgb( 0, 0 ,0 );
                vTaskDelay( pdMS_TO_TICKS(100) );
                continue ;

            case MODE_RED:
                set_rgb( 255, 0, 0 );
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;

            case MODE_GREEN :
                set_rgb( 0, 255, 0 );
                vTaskDelay( pdMS_TO_TICKS(100) );
                continue;

            case MODE_BLUE :
                set_rgb( 0, 0, 255 );
                vTaskDelay( pdMS_TO_TICKS(100) );
                continue;

            case MODE_RAINBOW :
                hsv_to_rgb(hue, 255, 200, &r, &g, &b);
                set_rgb(r, g, b);
                hue = (hue + 1) % 360;
                vTaskDelay( pdMS_TO_TICKS(20) );  /* ~18 seconds per full cycle */
                continue;

            case MODE_PULSE : {
                hsv_to_rgb(hue, 255, pulse_brightness, &r, &g, &b);
                set_rgb(r, g, b);
                int new_val = pulse_brightness + pulse_direction * 2;
                if (new_val >= 255) { new_val = 255; pulse_direction = -1; }
                if (new_val <= 0) {
                    new_val = 0;
                    pulse_direction = 1;
                    hue = (hue + 30) % 360;  /* Shift color each pulse */
                }

            case MODE_BREATH : {
                hsv_to_rgb(  hue ,  , );

                }

            pulse_brightness = ( uint8_t) new_val ;
            vTaskDelay( pdMS_TO_TICKS(10) );
            continue;
            }
        }
    }
}


static void str_tolower( char *s )
{
    for(; *s ; s++ )
    {
        *s = tolower((unsigned char) *s );
    }
}

static void parser_task( void *pvParameters )
{
    char line[64];

    ESP_LOGI( TAG , "Parser task running on core %d",xPortGetCoreID()) ;
    printf( "\nRGB Modd Lamp ready . Commands red, green , blue ,rainbow,pulse ,off\n>");
    fflush( stdout );
    // blue  red green off pulse


    while(1){
        if (fgets(line,sizeof(line),stdin) != NULL)
        {
            line[strcspn( line, "\r\n")] = '\0' ;
            str_tolower( line ) ;

            led_mode_t mode ;
            bool valid = true ;

            if (strcmp(line, "red") == 0) {
                mode = MODE_RED;
            } else if (strcmp(line, "green") == 0) {
                mode = MODE_GREEN;
            } else if (strcmp(line, "blue") == 0) {
                mode = MODE_BLUE;
            } else if (strcmp(line, "rainbow") == 0) {
                mode = MODE_RAINBOW;
            } else if (strcmp(line, "pulse") == 0) {
                mode = MODE_PULSE;
            } else if (strcmp(line, "off") == 0) {
                mode = MODE_OFF;
            }else if( strcmp( line , "breatg" ) == 0 ){
                mode = MODE_BREATH ;
            } else {
                valid = false;
                printf("Unknown command: '%s'\n", line);
                printf("Commands: red, green, blue, rainbow, pulse, off\n");
            }

            if(valid)
            {
                xQueueSend(mode_queue , &mode , pdMS_TO_TICKS(100));
                printf( "OK: %s\n" , line) ;
            }
            printf( "> " );
            fflush( stdout );

        }

        vTaskDelay( pdMS_TO_TICKS(1500)) ;
    }
}

/*
void my_task( void *pvParameters )
{
    while(1){
        vTaskDelay(pdMS_TO_TICKS(100) );
    }
}*/


void app_main( void )
{
    ESP_LOGI(TAG, "RGB Mood Lamp starting on ESP32" );

    ledc_conftimer();
    int gpios[] = {GPIO_RED , GPIO_GREEN , GPIO_BLUE } ;
    ledc_assignchannel(  gpios ) ;
    set_rgb( 0, 0 , 0 );

    mode_queue = xQueueCreate( 4 , sizeof( led_mode_t ) );

    xTaskCreatePinnedToCore(
        animation_task,
        "animation",
        4096,
        NULL,
        5,
        NULL,
        1) ;


    xTaskCreatePinnedToCore(
        parser_task,
        "parser",
        4096,
        NULL,
        5,
        NULL,
        0   //Core ID : 0 = pro_cpu , 1 = APP_CPU tskNO_AFFINITY for default handlig by scheduler
    );

}





