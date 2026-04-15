#include <sys/param.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/inet.h"

#include "esp_http_server.h"
#include "dns_server.h"

#include "events.h"
#include "init.h"


extern const char root_start[] asm("_binary_captive_html_start");
extern const char root_end[] asm("_binary_captive_html_end");

static const char *TAG = "wifi_handler";

static httpd_handle_t server = NULL;
static dns_server_handle_t dns_handle = NULL;

typedef struct {
    char ssid[33];
    char password[65];
} wifi_credentials_t;



static esp_err_t wifi_config_stored(void)
{
    /* Get Wi-Fi Station configuration */
    wifi_config_t wifi_cfg;
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) != ESP_OK) {
        return ESP_FAIL;
    }

    if (strlen((const char *) wifi_cfg.sta.ssid)) {
        ESP_LOGD(TAG, "Wi-Fi SSID     : %.*s",
                strnlen((const char *) wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid)), 
                (const char *) wifi_cfg.sta.ssid);
        return ESP_OK;
    }
    return ESP_FAIL;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from AP, retrying...");
        xEventGroupClearBits(system_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        xEventGroupSetBits(system_event_group, WIFI_CONNECTED_BIT);
    }
}

// Task to handle WiFi mode switching asynchronously
static void wifi_switch_task(void *pvParameters)
{
    wifi_credentials_t *creds = (wifi_credentials_t *)pvParameters;
    
    ESP_LOGI(TAG, "WiFi switch task started");
    
    // Give time for HTTP response to be sent
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "Stopping web services...");
    
    // Stop the webserver and DNS server
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
    if (dns_handle) {
        stop_dns_server(dns_handle);
        dns_handle = NULL;
    }
    
    ESP_LOGI(TAG, "Configuring WiFi for SSID: %s", creds->ssid);
    
    // Configure WiFi
    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, creds->ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, creds->password, sizeof(wifi_config.sta.password));
    
    // Set auth mode based on whether password is provided
    if (strlen(creds->password) == 0) {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }
    
    // Stop WiFi and reconfigure
    esp_wifi_stop();
    
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    
    ESP_LOGI(TAG, "WiFi reconfigured, waiting for connection events...");
    
    // Free credentials memory
    free(creds);
    
    // Delete this task
    vTaskDelete(NULL);
}

static void wifi_init_softap(void)
{
    /* Start Wi-Fi in access point mode */
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_WIFI_SSID,
            .ssid_len = strlen(CONFIG_WIFI_SSID),
            .password = CONFIG_WIFI_PASSWORD,
            .max_connection = CONFIG_WIFI_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(CONFIG_WIFI_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' password:'%s'",
             CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
}

static void dhcp_set_captiveportal_url(void) {
    // get the IP of the access point to redirect to
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    // turn the IP into a URI
    char* captiveportal_uri = (char*) malloc(32 * sizeof(char));
    assert(captiveportal_uri && "Failed to allocate captiveportal_uri");
    strcpy(captiveportal_uri, "http://");
    strcat(captiveportal_uri, ip_addr);

    // get a handle to configure DHCP with
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    // set the DHCP option 114
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(netif));
    ESP_ERROR_CHECK(esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, captiveportal_uri, strlen(captiveportal_uri)));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(netif));
    free(captiveportal_uri);
}

// HTTP GET Handler
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const uint32_t root_len = root_end - root_start;

    ESP_LOGI(TAG, "Serve root");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, root_start, root_len);

    return ESP_OK;
}

static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler
};

// Helper function to URL decode
static void url_decode(char *dst, const char *src, size_t max_len) {
    char a, b;
    size_t written = 0;
    while (*src && written < max_len - 1) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a')
                a -= 'a'-'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a'-'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = 16*a+b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
        written++;
    }
    *dst = '\0';
}

static esp_err_t connect_post_handler(httpd_req_t *req)
{
    char buf[300];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) httpd_resp_send_408(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    /* Parse form fields */
    char ssid[33]       = {0};
    char password[65]   = {0};
    char broker[128]    = {0};

    char *p;
    if ((p = strstr(buf, "ssid="))) {
        p += 5;
        char *end = strchr(p, '&'); if (end) *end = '\0';
        url_decode(ssid, p, sizeof(ssid));
        if (end) *end = '&';
    }
    if ((p = strstr(buf, "password="))) {
        p += 9;
        char *end = strchr(p, '&'); if (end) *end = '\0';
        url_decode(password, p, sizeof(password));
        if (end) *end = '&';
    }
    if ((p = strstr(buf, "mqtt_broker="))) {
        p += 12;
        char *end = strchr(p, '&'); if (end) *end = '\0';
        url_decode(broker, p, sizeof(broker));
        if (end) *end = '&';
    }

    ESP_LOGI(TAG, "SSID: %s  Broker: %s", ssid, broker);

    /* Persist broker URL before switching WiFi */
    if (strlen(broker) > 0) {
        nvs_save_broker(broker);
    }

    wifi_credentials_t *creds = malloc(sizeof(wifi_credentials_t));
    if (!creds) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }
    strlcpy(creds->ssid,     ssid,     sizeof(creds->ssid));
    strlcpy(creds->password, password, sizeof(creds->password));

    const char *resp = "<html><body><h1>A conectar...</h1><p>O dispositivo vai ligar à sua rede WiFi.</p></body></html>";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    xTaskCreate(wifi_switch_task, "wifi_switch", 4096, creds, tskIDLE_PRIORITY + 4, NULL);
    return ESP_OK;
}

static const httpd_uri_t connect_post = {
    .uri = "/connect",
    .method = HTTP_POST,
    .handler = connect_post_handler
};

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &connect_post);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}

/**
 * @brief Start captive portal services (SoftAP, web server, DNS server)
 */
static void start_captive_portal(void)
{
    ESP_LOGI(TAG, "Starting captive portal...");
    
    // Start SoftAP mode
    wifi_init_softap();
    
    // Configure DNS-based captive portal, if configured
    dhcp_set_captiveportal_url();
    
    // Start the web server
    server = start_webserver();
    
    // Start the DNS server that will redirect all queries to the softAP IP
    dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    dns_handle = start_dns_server(&config);
    
    ESP_LOGI(TAG, "Captive portal started successfully");
}

/**
 * @brief Reset WiFi configuration and restart captive portal
 * 
 * This function:
 * 1. Clears stored WiFi credentials from NVS
 * 2. Stops current WiFi connection
 * 3. Starts SoftAP mode
 * 4. Starts web server and DNS server for captive portal
 * 
 * Call this when you want to reconfigure WiFi settings
 */
void reset_wifi_config_and_start_portal(void)
{
    ESP_LOGI(TAG, "Resetting WiFi configuration...");
    
    // Stop any existing servers
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
    if (dns_handle) {
        stop_dns_server(dns_handle);
        dns_handle = NULL;
    }
    
    // Stop WiFi
    esp_wifi_stop();
    
    // Clear stored WiFi configuration
    esp_wifi_restore();
    
    // Start captive portal
    start_captive_portal();
}

void initialize_wifi(void)
{
    /*
        Turn off warnings from HTTP server as redirecting traffic will yield
        lots of invalid requests
    */
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize Wi-Fi including netif with default config
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers for WiFi station events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    if (wifi_config_stored() != ESP_OK) {
        ESP_LOGI(TAG, "No Wi-Fi credentials stored, starting captive portal");
        start_captive_portal();
    } 
    else {
        ESP_LOGI(TAG, "Wi-Fi credentials stored, not starting captive portal");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
}
