#include <stdio.h>
#include <string.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <httpd/httpd.h>

//#define NO_CONNECTION_WATCHDOG_TIMEOUT 600000

// The GPIO pin that is connected to the relay.
const int relay_gpio = 12;
const int relay2_gpio = 13;
const int relay3_gpio = 14;

// The GPIO pin that is oconnected to the switch.
const int switch_gpio = 0;
const int switch2_gpio = 4;
const int switch3_gpio = 5;



bool is_connected_to_wifi = false;

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void switch2_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void switch3_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
//void button_callback(uint8_t gpio, button_event_t event);
void gpio_intr_handler(uint8_t gpio_num);
void gpio_intr_handler2(uint8_t gpio_num);
void gpio_intr_handler3(uint8_t gpio_num);
/*
void led_write(bool on) {
    gpio_write(led_gpio, on ? 0 : 1);
}
*/

void relay_write(uint8_t gpio,bool on) {
    gpio_write(gpio, on ? 1 : 0);
}
void reset_configuration_task() {
    printf("Resetting HomeKit Config\n");
    homekit_server_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Resetting Wifi Config\n");
    wifi_config_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Restarting\n");
    sdk_system_restart();
    vTaskDelete(NULL);
}

void reset_configuration() {
    printf("Resetting Magic-switch configuration\n");
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 3, NULL);
}

homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(
    ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback)
);

homekit_characteristic_t switch2_on = HOMEKIT_CHARACTERISTIC_(
    ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch2_on_callback)
);

homekit_characteristic_t switch3_on = HOMEKIT_CHARACTERISTIC_(
    ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch3_on_callback)
);

void gpio_init() {	
    gpio_enable(relay_gpio, GPIO_OUTPUT);
	gpio_enable(relay2_gpio, GPIO_OUTPUT);
	gpio_enable(relay3_gpio, GPIO_OUTPUT);
	gpio_set_pullup(switch_gpio, true, true);
	gpio_set_pullup(switch2_gpio, true, true);
	gpio_set_pullup(switch3_gpio, true, true);
    relay_write(relay_gpio,switch_on.value.bool_value);
	relay_write(relay2_gpio,switch2_on.value.bool_value);
	relay_write(relay3_gpio,switch3_on.value.bool_value);
}

void set_relay_value(bool value) {
    printf("Relay Value: %d\n", value);
    switch_on.value.bool_value = value;
    relay_write(relay_gpio,value);
    homekit_characteristic_notify(&switch_on, switch_on.value);
}

void set_relay2_value(bool value) {
    printf("Relay2 Value: %d\n", value);
    switch2_on.value.bool_value = value;
    relay_write(relay2_gpio,value);
    homekit_characteristic_notify(&switch2_on, switch2_on.value);
}

void set_relay3_value(bool value) {
    printf("Relay3 Value: %d\n", value);
    switch3_on.value.bool_value = value;
    relay_write(relay3_gpio,value);
    homekit_characteristic_notify(&switch3_on, switch3_on.value);
}

void toggle_relay_value() {
    set_relay_value(!switch_on.value.bool_value);
}

void toggle_relay2_value() {
    set_relay2_value(!switch2_on.value.bool_value);
}

void toggle_relay3_value() {
    set_relay3_value(!switch3_on.value.bool_value);
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write(relay_gpio,switch_on.value.bool_value);
}

void switch2_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write(relay2_gpio,switch2_on.value.bool_value);
}

void switch3_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write(relay3_gpio,switch3_on.value.bool_value);
}
/*
void res_key_callback(uint8_t gpio, button_event_t event) {
    switch (event) {
        case button_event_single_press:
            sdk_system_restart();
            break;
        case button_event_long_press:
            reset_configuration();
            break;
        default:
            printf("Unknown button event: %d\n", event);
    }
}
*/
void homekit_identify(homekit_value_t _value) {
    printf("Homekit identify\n");
}

/*
void wifi_connection_watchdog_task(void *_args) {
    vTaskDelay(NO_CONNECTION_WATCHDOG_TIMEOUT / portTICK_PERIOD_MS);
    if (is_connected_to_wifi == false) {
        printf("No Wifi Connection, Restarting\n");
        sdk_system_restart();
    }
    vTaskDelete(NULL);
}

void create_wifi_connection_watchdog() {
    printf("Wifi connection watchdog\n");
    xTaskCreate(wifi_connection_watchdog_task, "Wifi Connection Watchdog", 128, NULL, 5, NULL);
}
*/

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Magic-switch");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Fer"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "506001"),
            HOMEKIT_CHARACTERISTIC(MODEL, "001"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, homekit_identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "前灯"),
            &switch_on,
            NULL
        }),
		HOMEKIT_SERVICE(LIGHTBULB, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "中灯"),
            &switch2_on,
            NULL
        }),
		HOMEKIT_SERVICE(LIGHTBULB, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "后灯"),
            &switch3_on,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "405-11-506"
};

int32_t ssi_handler(int32_t iIndex, char *pcInsert, int32_t iInsertLen)
{
    snprintf(pcInsert, iInsertLen, (switch_on.value.bool_value) ? "on" : "off");
    /* Tell the server how many characters to insert */
    return (strlen(pcInsert));
}

char *cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    for (int i = 0; i < iNumParams; i++) {
        if (strcmp(pcValue[i], "on") == 0) {
            set_relay_value(true);
			set_relay2_value(true);
			set_relay3_value(true);
        } else if (strcmp(pcValue[i], "off") == 0) {
            set_relay_value(false);
			set_relay2_value(false);
			set_relay3_value(false);
        } else if (strcmp(pcValue[i], "toggle") == 0) {
            toggle_relay_value();
        }
    }
    return "/index.ssi";
}

char *on_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    set_relay_value(true);
	set_relay2_value(true);
	set_relay3_value(true);
    return "/ok.html";
}

char *off_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    set_relay_value(false);
	set_relay2_value(false);
	set_relay3_value(false);
    return "/ok.html";
}

char *toggle_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    toggle_relay_value();
	//toggle_relay2_value();
	//toggle_relay3_value();
    return "/ok.html";
}

char *state_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    return "/state.ssi";
}

void httpd_task(void *pvParameters)
{
    tCGI pCGIs[] = {
        {"/", (tCGIHandler) cgi_handler},
        {"/on", (tCGIHandler) on_cgi_handler},
        {"/off", (tCGIHandler) off_cgi_handler},
        {"/toggle", (tCGIHandler) toggle_cgi_handler},
        {"/state", (tCGIHandler) state_cgi_handler}
    };

    const char *pcConfigSSITags[] = {
        "state"     // SONOFF_STATE
    };

    /* register handlers and start the server */
    http_set_cgi_handlers(pCGIs, sizeof (pCGIs) / sizeof (pCGIs[0]));
    http_set_ssi_handler((tSSIHandler) ssi_handler, pcConfigSSITags, sizeof (pcConfigSSITags) / sizeof (pcConfigSSITags[0]));
    //websocket_register_callbacks((tWsOpenHandler) websocket_open_cb, (tWsHandler) websocket_cb);
    httpd_init();
    for (;;);
}

void on_wifi_ready() {
    is_connected_to_wifi = true;
    xTaskCreate(&httpd_task, "HTTP Test", 512, NULL, 1, NULL);
    homekit_server_init(&config);
}

void create_accessory_name() {
/**
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    int name_len = snprintf(NULL, 0, "Magic Switch-%02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Magic Switch-%02X%02X%02X",
             macaddr[3], macaddr[4], macaddr[5]);
*/
    char *name_value = malloc(17);
    snprintf(name_value, 17, "Magic Switch-506");
    name.value = HOMEKIT_STRING(name_value);
}


void watch_switch(void *pvParameters)
{
    QueueHandle_t *tsqueue = (QueueHandle_t *)pvParameters;
    gpio_set_interrupt(switch_gpio, GPIO_INTTYPE_EDGE_NEG, gpio_intr_handler);

    uint32_t last = 0;
    while(1) {
        uint32_t button_ts;
        xQueueReceive(*tsqueue, &button_ts, portMAX_DELAY);
        button_ts *= portTICK_PERIOD_MS;
        if(last < button_ts-100) {
			toggle_relay_value();
            last = button_ts;
        }
    }
}

void watch_switch2(void *pvParameters)
{
    QueueHandle_t *tsqueue2 = (QueueHandle_t *)pvParameters;
    gpio_set_interrupt(switch2_gpio, GPIO_INTTYPE_EDGE_NEG, gpio_intr_handler2);

    uint32_t last = 0;
    while(1) {
        uint32_t button_ts;
        xQueueReceive(*tsqueue2, &button_ts, portMAX_DELAY);
        button_ts *= portTICK_PERIOD_MS;
        if(last < button_ts-100) {
			toggle_relay2_value();
            last = button_ts;
        }
    }
}

void watch_switch3(void *pvParameters)
{
    QueueHandle_t *tsqueue3 = (QueueHandle_t *)pvParameters;
    gpio_set_interrupt(switch3_gpio, GPIO_INTTYPE_EDGE_NEG, gpio_intr_handler3);

    uint32_t last = 0;
    while(1) {
        uint32_t button_ts;
        xQueueReceive(*tsqueue3, &button_ts, portMAX_DELAY);
        button_ts *= portTICK_PERIOD_MS;
        if(last < button_ts-100) {
			toggle_relay3_value();
            last = button_ts;
        }
    }
}

static QueueHandle_t tsqueue;
static QueueHandle_t tsqueue2;
static QueueHandle_t tsqueue3;

void gpio_intr_handler(uint8_t gpio_num)
{
    uint32_t now = xTaskGetTickCountFromISR();
    xQueueSendToBackFromISR(tsqueue, &now, NULL);
}
void gpio_intr_handler2(uint8_t gpio_num)
{
    uint32_t now = xTaskGetTickCountFromISR();
    xQueueSendToBackFromISR(tsqueue2, &now, NULL);
}
void gpio_intr_handler3(uint8_t gpio_num)
{
    uint32_t now = xTaskGetTickCountFromISR();
    xQueueSendToBackFromISR(tsqueue3, &now, NULL);
}


void create_switch_watch() {
    tsqueue = xQueueCreate(2, sizeof(uint32_t));
	tsqueue2 = xQueueCreate(2, sizeof(uint32_t));
	tsqueue3 = xQueueCreate(2, sizeof(uint32_t));
	xTaskCreate(watch_switch, "watch switch", 256, &tsqueue, 2, NULL);
	xTaskCreate(watch_switch2, "watch switch2", 256, &tsqueue2, 2, NULL);
	xTaskCreate(watch_switch3, "watch switch3", 256, &tsqueue3, 2, NULL);
}

void user_init(void) {
    uart_set_baud(0, 115200);

    create_accessory_name();
    wifi_config_init("Magic-switch", NULL, on_wifi_ready);
    gpio_init();

    //if (button_create(button_gpio, 0, 4000, button_callback)) {		//gpio口 ， 按下值 ， 长按时间，回调函数
   //     printf("Failed to initialize button\n");
   // }
	
    //create_wifi_connection_watchdog();
	
    set_relay_value(false);
	set_relay2_value(false);
	set_relay3_value(false);
	
	create_switch_watch();
}
