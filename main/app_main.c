#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"


#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt.h"

#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/DHT22.h"
#include "driver/adc.h"

const char *MQTT_TAG = "MQTT_SAMPLE";
unsigned long int avgValue;
float b;
uint32_t buf[10], temp;

static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;

#define LED_GPIO 2
#define TOKEN "2;nct_laboratory;collect;0"
#define ERROR_TOKEN      "2;nct_laboratory;collect;nct"
#define KEEP_ALIVE_TOKEN "2;nct_laboratory;nct"
#define ADC1_CHANNEL (ADC1_CHANNEL_0) 

int authentication_error = 0;


// #define CONFIG_MQTT_SECURITY_ON 1

// keep alive thread
void *keep_alive(void *vargp){
    mqtt_client *client = (mqtt_client *)vargp;
    while(1){
        sleep(60);
        // keep alive
        mqtt_publish(client,"nct_keep_alive",KEEP_ALIVE_TOKEN,sizeof(KEEP_ALIVE_TOKEN),0,0);
        printf("Keep alive message publish !!!!\n");
    }
}

float getPH(){
    for (int i = 0; i < 10; i++)
    {
        
        buf[i] = adc1_get_raw(ADC1_CHANNEL)/6;
        printf("%d\n", buf[i]);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    for (int i = 0; i < 9; i++)
    {
        for (int j = i + 1; j < 10; j++)
        {
            if (buf[i] > buf[j])
            {
                temp = buf[i];
                buf[i] = buf[j];
                buf[j] = temp;
            }
        }
    }
    avgValue = 0;
    for (int i = 2; i < 8; i++)
    {
        avgValue += buf[i];
    }
    float phValue = (float)avgValue*5.0/1024/6;
    phValue = -5.70 * phValue + 21.34;;
    return phValue;
}


void connected_cb(void *self, void *params)
{
    mqtt_client *client = (mqtt_client *)self;
    setDHTgpio(4);
    printf( "Starting DHT Task\n\n");
    mqtt_publish(client,"nct_authentication",TOKEN,sizeof(TOKEN),0,0);
     mqtt_subscribe(client, "nct_authentication_result_2", 0);

    // while(1){
    //     int ret = readDHT();
    //     char buf[26];
    //     sprintf(buf,"{\"temp\":%.1f,\"humid\":%.1f}\n",getTemperature(),getHumidity());
    //     // sprintf(buf,"%.1f\n",getTemperature());
    //     mqtt_publish(client, "nct_collect",buf,26, 0, 0);
    //     printf("publish : %s", buf);
    //     vTaskDelay( 3000 / portTICK_RATE_MS );
    
    // }
}

void disconnected_cb(void *self, void *params)
{

}
void reconnect_cb(void *self, void *params)
{

}
void subscribe_cb(void *self, void *params)
{
    ESP_LOGI(MQTT_TAG, "[APP] Subscribe ok, test publish msg");
    mqtt_client *client = (mqtt_client *)self;
}

void publish_cb(void *self, void *params)
{

}


void data_cb(void *self, void *params)
{
    mqtt_client *client = (mqtt_client *)self;
    mqtt_event_data_t *event_data = (mqtt_event_data_t *)params;

    // if(event_data->data_offset == 0) {

    //     char *topic = malloc(event_data->topic_length + 1);
    //     memcpy(topic, event_data->topic, event_data->topic_length);
    //     topic[event_data->topic_length] = 0;
    //     ESP_LOGI(MQTT_TAG, "[APP] Publish topic: %s", topic);
    //     free(topic);
    // }

    // char *data = malloc(event_data->data_length + 1);
    // memcpy(data, event_data->data, event_data->data_length);
    // data[event_data->data_length] = 0;

    // data);

    // free(data);



    // while(1){
    //     int ret = readDHT();
    //     char buf[26];
    //     sprintf(buf,"{\"temp\":%.1f,\"humid\":%.1f}\n",getTemperature(),getHumidity());
    //     // sprintf(buf,"%.1f\n",getTemperature());
    //     mqtt_publish(client, "nct_collect_2",buf,26, 0, 0);
    //     printf("publish : %s", buf);
    //     vTaskDelay( 3000 / portTICK_RATE_MS );
    
    // }

       char *topic = malloc(event_data->topic_length + 1);
        memcpy(topic, event_data->topic, event_data->topic_length);
        topic[event_data->topic_length] = 0;

    

    if((strcmp(topic,"nct_authentication_result_2") == 0)){

        char* recv = (char*)event_data->data;
        
        if((strcmp(recv,"PASS")) == 0){
            printf("CHECK PASSWORD DONE !!!!\n");

            pthread_t tid;
            pthread_create(&tid, NULL,keep_alive, (void*)client);

            setDHTgpio(4);
            while(1){
                int ret = readDHT();
                char buf[72];
                // sprintf(buf,"nct;%.1f;%.1f;900;7.0;true;\"00/00/00 00:00:00\";nct"
                //     ,getTemperature(),getHumidity());
                sprintf(buf,"2;%.1f;%.1f;900;%.1f;100;nct"
                    ,getTemperature(),getHumidity(),getPH());
                mqtt_publish(client, "nct_collect",buf,28, 0, 0);
                printf("publish : %s", buf);
                vTaskDelay( 30*1000 / portTICK_RATE_MS );  
            }

        }else{
            printf("CHECK PASSWORD FAILED !!!!\n");
            if(authentication_error >2){
                mqtt_publish(client,"nct_error",ERROR_TOKEN,sizeof(ERROR_TOKEN),0,0);
                printf("PUBLISH TO NCT_ERROR !!!!\n");
            }else{
                authentication_error+=1;
                vTaskDelay(60000 / portTICK_PERIOD_MS);
                printf("RE AUTHENTICATION !!!!\n");
                mqtt_publish(client,"nct_authentication",TOKEN,sizeof(TOKEN),0,0);
            }
        }

    }

}

mqtt_settings settings = {
    .host = "iot.eclipse.org",
    .port = 1883,
// #if defined(CONFIG_MQTT_SECURITY_ON)
//     .port = 8883, // encrypted
// #else
//     .port = 1883, // unencrypted
// #endif
    .client_id = "mqtt_client_id",
    .username = "tienvn3012",
    .password = "tienchocorion",
    .clean_session = 0,
    .keepalive = 120,
    .lwt_topic = "/lwt",
    .lwt_msg = "offline",
    .lwt_qos = 0,
    .lwt_retain = 0,
    .connected_cb = connected_cb,
    .disconnected_cb = disconnected_cb,
    // .reconnect_cb = reconnect_cb,
    .subscribe_cb = subscribe_cb,
    .publish_cb = publish_cb,
    .data_cb = data_cb
};



static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            mqtt_start(&settings);
            // mqtt_start(&setting2);
            //init app here
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            /* This is a workaround as ESP32 WiFi libs don't currently
               auto-reassociate. */
            esp_wifi_connect();
            mqtt_stop();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void wifi_conn_init(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "NCT",
            .password = "dccndccn",
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(MQTT_TAG, "start the WIFI SSID:[%s] password:[%s]", CONFIG_WIFI_SSID, "******");
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main()
{


    gpio_pad_select_gpio(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_INPUT_OUTPUT);
    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC1_CHANNEL,ADC_ATTEN_11db);


    nvs_flash_init();
    wifi_conn_init();
}
