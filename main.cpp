// ----------------------------------------------------------------------------
// Copyright 2016-2019 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------

#define MQTTCLIENT_QOS1 0
#define MQTTCLIENT_QOS2 0

#include "mbed.h"
#include "mbed-trace/mbed_trace.h"
#include "mbed_events.h"
#include "SimpleAzureIoTHub.h"

// LED on/off - This could be different among boards
#define LED_ON  0
#define LED_OFF 1

AzureIoT *azure;

EventQueue eventQueue;

// Peripherals
DigitalOut led_red(LED1, LED_OFF);
DigitalOut led_green(LED2, LED_OFF);
DigitalOut led_blue(LED3, LED_OFF);

// Function prototypes
void handleMqttMessage(MQTT::MessageData& md);
void handleButtonRise();

int main(int argc, char* argv[])
{
    mbed_trace_init();

    const float version = 0.1;

    printf("Mbed to Azure IoT Hub: version is %.2f\r\n", version);
    printf("\r\n");

    // Turns on green LED to indicate processing initialization process
    led_green = LED_ON;

    printf("Opening network interface...\r\n");

    NetworkInterface *network = NetworkInterface::get_default_instance();
    if (!network) {
        printf("Unable to open network interface.\r\n");
        return -1;
    }

    nsapi_error_t net_status = NSAPI_ERROR_NO_CONNECTION;
    while ((net_status = network->connect()) != NSAPI_ERROR_OK) {
        printf("Unable to connect to network (%d). Retrying...\r\n", net_status);
    }

    printf("Connected to the network successfully. IP address: %s\r\n", network->get_ip_address());
    printf("\r\n");

    azure = new AzureIoT(&eventQueue, network, &handleMqttMessage);
    nsapi_error_t cr = azure->connect();
    printf("Azure connect returned %d\n", cr);

    // Enable button 1 for publishing a message.
    InterruptIn btn1(BUTTON1);
    btn1.rise(eventQueue.event(handleButtonRise));

    printf("To send a packet, push the button 1 on your board.\r\n");

    eventQueue.dispatch_forever();
}

/*
 * Callback function called when a message arrived from server.
 */
void handleMqttMessage(MQTT::MessageData& md)
{
    // Copy payload to the buffer.
    MQTT::Message &message = md.message;
    if (message.payloadlen > 127) message.payloadlen = 127;
    char buff[128] = { 0 };
    memcpy(buff, message.payload, message.payloadlen);

    printf("\r\nMessage arrived:\r\n%s\r\n", buff);
}

/*
 * Callback function called when button is pushed.
 */
void handleButtonRise() {
    if (!azure) {
        printf("\r\nMQTTClient is not connected\r\n");
        return;
    }

    static unsigned int id = 0;
    static unsigned int count = 0;

    // When sending a message, blue LED lights.
    led_blue = LED_ON;

    MQTT::Message message;
    message.retained = false;
    message.dup = false;

    const size_t len = 128;
    char buf[len];
    snprintf(buf, len, "Message #%d from %s.", count, DEVICE_ID);
    printf("Sending %s\n", buf);
    message.payload = (void*)buf;

    message.qos = MQTT::QOS0;
    message.id = id++;
    message.payloadlen = strlen(buf);

    nsapi_error_t rc = azure->publish(&message);
    if(rc != MQTT::SUCCESS) {
        printf("ERROR: rc from MQTT publish is %d\r\n", rc);
    }
    printf("Message published.\r\n");

    count++;

    led_blue = LED_OFF;
}
