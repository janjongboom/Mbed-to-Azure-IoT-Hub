#ifndef _SIMPLE_AZURE_IOT_HUB_H_
#define _SIMPLE_AZURE_IOT_HUB_H_

#include "MQTT_server_setting_jan.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "NTPClient.h"
#include "mbedtls/error.h"

#define MQTT_MAX_CONNECTIONS         5
#define MQTT_MAX_PACKET_SIZE         1024
#define MQTT_MESSAGE_BUFFER_SIZE     1024

// Topics from user's setting in MQTT_server_setting.h
static const char *az_mqtt_topic_pub = "devices/" DEVICE_ID "/messages/events/";
static const char *az_mqtt_topic_sub = "devices/" DEVICE_ID "/messages/devicebound/#";

class AzureIoT {
public:
    /**
     * Instantiate an Azure IoT Hub instance.
     * Credentials are loaded from MQTT_server_setting.
     *
     * @param queue An instance of an event queue
     * @param network A connected network instance
     * @param mh Function callback to be invoked when a message was received
     *           todo: replace by Mbed OS Callback
     */
    AzureIoT(EventQueue *queue, NetworkInterface *network, MQTT::Client<MQTTNetwork, Countdown, MQTT_MAX_PACKET_SIZE, MQTT_MAX_CONNECTIONS>::messageHandler mh)
        : _queue(queue), _network(network), _mh(mh), _mqttNetwork(network), _mqttClient(_mqttNetwork)
    {

    }

    ~AzureIoT() {
    }

    /**
     * Connect to Azure IoT Hub
     * Note: This will also perform an NTP time sync
     *
     * @returns NSAPI_ERROR_OK if succeeded, or a negative error code when failed
     */
    nsapi_error_t connect() {
        // Do an NTP time sync
        {
            NTPClient ntp(_network);
            ntp.set_server("time.google.com", 123);
            time_t now = ntp.get_timestamp();
            if (now < 0) return now;

            set_time(now);
            printf("[AZUR] Time is now %s\n", ctime(&now));
        }

        printf("[AZUR] Connecting to server...\n");

#if IOTHUB_AUTH_METHOD == IOTHUB_AUTH_SYMMETRIC_KEY
        int rc = _mqttNetwork.connect(MQTT_SERVER_HOST_NAME, MQTT_SERVER_PORT, SSL_CA_PEM);
#elif IOTHUB_AUTH_METHOD == IOTHUB_AUTH_CLIENT_SIDE_CERT
         int rc = _mqttNetwork.connect(MQTT_SERVER_HOST_NAME, MQTT_SERVER_PORT, SSL_CA_PEM,
                    SSL_CLIENT_CERT_PEM, SSL_CLIENT_PRIVATE_KEY_PEM);
#endif
        if (rc != MQTT::SUCCESS) {
            const int MAX_TLS_ERROR_CODE = -0x1000;
            // Network error
            // TODO: implement converting an error code into message.
            printf("[AZUR] ERROR from MQTTNetwork connect is %d\n", rc);
#if defined(MBEDTLS_ERROR_C) || defined(MBEDTLS_ERROR_STRERROR_DUMMY)
            // TLS error - mbedTLS error codes starts from -0x1000 to -0x8000.
            if(rc <= MAX_TLS_ERROR_CODE) {
                const int buf_size = 256;
                char *buf = new char[buf_size];
                mbedtls_strerror(rc, buf, buf_size);
                printf("[AZUR] TLS ERROR (%d) : %s\n", rc, buf);
            }
#endif
            return rc;
        }

        printf("[AZUR] Connected to server\n");

        // Generate username from host name and client id.
        const char *username = MQTT_SERVER_HOST_NAME "/" DEVICE_ID "/api-version=2016-11-14";

        /* Establish a MQTT connection. */
        printf("[AZUR] Authenticating...\n");
        {
            MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
            data.MQTTVersion = 4; // 3 = 3.1 4 = 3.1.1
            data.clientID.cstring = (char*)DEVICE_ID;
            data.username.cstring = (char*)username;
            data.password.cstring = (char*)MQTT_SERVER_PASSWORD;

            int rc = _mqttClient.connect(data);
            if (rc != MQTT::SUCCESS) {
                printf("[AZUR] ERROR: rc from MQTT connect is %d\n", rc);
                return rc;
            }
        }
        printf("[AZUR] Authentication completed\n");

        printf("[AZUR] Subscribing to topic \"%s\"\n", az_mqtt_topic_sub);
        {
            int rc = _mqttClient.subscribe(az_mqtt_topic_sub, MQTT::QOS0, _mh);
            if (rc != MQTT::SUCCESS) {
                printf("[AZUR] ERROR: rc from MQTT subscribe is %d\n", rc);
                return rc;
            }
        }
        printf("[AZUR] Subscribed to topic\n");

        _queue->call_every(100, callback(&_mqttClient, &MQTT::Client<MQTTNetwork, Countdown, MQTT_MAX_PACKET_SIZE, MQTT_MAX_CONNECTIONS>::yield), 1);

        return NSAPI_ERROR_OK;
    }

    nsapi_error_t publish(MQTT::Message *message) {
        if (!_mqttClient.isConnected()) {
            printf("[AZUR] MQTTClient is not connected\n");
            return -1;
        }

        // Publish a message.
        printf("[AZUR] Publishing message to the topic '%s'\n", az_mqtt_topic_pub);
        int rc = _mqttClient.publish(az_mqtt_topic_pub, *message);
        if(rc != MQTT::SUCCESS) {
            printf("[AZUR] ERROR: rc from MQTT publish is %d\n", rc);
            return rc;
        }
        printf("[AZUR] Message published\n");

        return NSAPI_ERROR_OK;
    }

private:

    EventQueue *_queue;
    NetworkInterface *_network;
    MQTT::Client<MQTTNetwork, Countdown, MQTT_MAX_PACKET_SIZE, MQTT_MAX_CONNECTIONS>::messageHandler _mh;

    MQTTNetwork _mqttNetwork;
    MQTT::Client<MQTTNetwork, Countdown, MQTT_MAX_PACKET_SIZE, MQTT_MAX_CONNECTIONS> _mqttClient;

    char _messageBuffer[MQTT_MESSAGE_BUFFER_SIZE];
};

#endif // _SIMPLE_AZURE_IOT_HUB_H_
