#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/net/net_if.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mender_ota, LOG_LEVEL_DBG);

#include <mender/utils.h>
#include <mender/client.h>
#include <mender/zephyr-image-update-module.h>

#include <zephyr/net/socket.h>

#include "ethernet_if.h"
#include "storage.h"
#include "cmake_config.h"
#include "system_info.h"

/* Retry interval (seconds) between DNS probe attempts when the server is unreachable */
#define MENDER_DNS_PROBE_INTERVAL_S   10
/* Maximum number of DNS probe attempts before giving up and running offline */
#define MENDER_DNS_PROBE_MAX_ATTEMPTS 6

void mender_ota_task(void *p1, void *p2, void *p3);

// statically define a task for Mender OTA
K_KERNEL_THREAD_DEFINE(mender_ota_thread, 2048,
               mender_ota_task, NULL, NULL, NULL,
               CONFIG_REMOTEIO_SERVICE_PRIORITY, 0, 0);

// listen for network events
extern struct k_event ethernet_if_events;

static char mac_address[18];
static mender_identity_t mender_identity = { .name = "mac", .value = mac_address };


/*************/
/* Functions */
/*************/

// TLS
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#include <zephyr/net/tls_credentials.h>

#ifdef CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_PRIMARY
static const unsigned char ca_cert_primary[] = {
    // #include "AmazonRootCA1.cer.inc"
    #include "mender_server_ca.crt.inc"
    0x00
};
#endif // CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_PRIMARY

#ifdef CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_SECONDARY
static const unsigned char ca_cert_secondary[] = {
    #include "gts-root-r4.pem.inc"
    0x00
};
#endif // CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_SECONDARY
#endif // CONFIG_NET_SOCKETS_SOCKETS_TLS

mender_err_t mender_network_connect_cb(void) {
    LOG_DBG("network_connect_cb");
    return MENDER_OK;
}

mender_err_t mender_network_release_cb(void) {
    LOG_DBG("network_release_cb");
    // Update the system status to SYSTEM_STATUS_OK when current status is SYSTEM_STATUS_CHECKING_FOR_UPDATE or SYSTEM_STATUS_UPDATE_AVAILABLE
    system_status_t current_status = system_info_get_status();
    if (current_status == SYSTEM_STATUS_CHECKING_FOR_UPDATE || current_status == SYSTEM_STATUS_UPDATE_AVAILABLE) {
        system_info_set_status(SYSTEM_STATUS_OK);
    }

    return MENDER_OK;
}

mender_err_t mender_deployment_status_cb(mender_deployment_status_t status, const char *desc) {
    LOG_DBG("deployment_status_cb: %s", desc);
    // Update the system status based on the deployment status
    switch (status) {
        case MENDER_DEPLOYMENT_STATUS_DOWNLOADING:
            system_info_set_status(SYSTEM_STATUS_MENDER_DOWNLOADING);
            break;
        case MENDER_DEPLOYMENT_STATUS_INSTALLING:
            system_info_set_status(SYSTEM_STATUS_MENDER_INSTALLING);
            break;
        case MENDER_DEPLOYMENT_STATUS_REBOOTING:
            system_info_set_status(SYSTEM_STATUS_MENDER_REBOOTING);
            break;
        case MENDER_DEPLOYMENT_STATUS_SUCCESS:
        case MENDER_DEPLOYMENT_STATUS_FAILURE:
            system_info_set_status(SYSTEM_STATUS_OK);
            break;
        default:
            break;
    }
    return MENDER_OK;
}

mender_err_t mender_restart_cb(void) {
    LOG_DBG("restart_cb");

    system_info_set_status(SYSTEM_STATUS_MENDER_REBOOTING);

    sys_reboot(SYS_REBOOT_WARM);

    return MENDER_OK;
}

mender_err_t mender_get_identity_cb(const mender_identity_t **identity) {
    LOG_DBG("get_identity_cb");
    if (NULL != identity) {
        *identity = &mender_identity;
        return MENDER_OK;
    }
    return MENDER_FAIL;
}

/**
 * @brief Extract the hostname from a URL string (strips scheme and any trailing path/port).
 *        e.g. "https://pi-mender-stage.garmin.com" -> "pi-mender-stage.garmin.com"
 *        The result is written into @p out_buf (null-terminated). Returns 0 on success.
 */
static int extract_hostname(const char *url, char *out_buf, size_t out_buf_len)
{
    const char *p = url;

    /* Skip scheme (e.g. "https://") */
    const char *scheme_end = strstr(p, "://");
    if (scheme_end != NULL) {
        p = scheme_end + 3;
    }

    /* Copy up to the first '/', ':', or end-of-string */
    size_t i = 0;
    while (*p != '\0' && *p != '/' && *p != ':' && i < out_buf_len - 1) {
        out_buf[i++] = *p++;
    }
    out_buf[i] = '\0';

    return (i > 0) ? 0 : -EINVAL;
}

/**
 * @brief Probe whether the Mender server hostname is resolvable.
 *
 * Attempts a DNS lookup for the hostname extracted from CONFIG_MENDER_SERVER_HOST
 * up to MENDER_DNS_PROBE_MAX_ATTEMPTS times, waiting MENDER_DNS_PROBE_INTERVAL_S
 * seconds between each attempt. Stops as soon as the lookup succeeds or the
 * attempt limit is reached, so the app can continue to run fully offline.
 *
 * @return true  if the hostname resolved within the allowed attempts.
 * @return false if all attempts were exhausted (no internet / DNS unavailable).
 */
static bool wait_for_mender_server_reachable(void)
{
    char hostname[128];

    if (extract_hostname(CONFIG_MENDER_SERVER_HOST, hostname, sizeof(hostname)) != 0) {
        LOG_ERR("Failed to extract hostname from '%s'", CONFIG_MENDER_SERVER_HOST);
        return false;
    }

    LOG_INF("Probing Mender server '%s' (max %d attempts, %d s interval)...",
            hostname, MENDER_DNS_PROBE_MAX_ATTEMPTS, MENDER_DNS_PROBE_INTERVAL_S);

    struct zsock_addrinfo hints = {
        .ai_family   = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };

    for (int attempt = 1; attempt <= MENDER_DNS_PROBE_MAX_ATTEMPTS; attempt++) {
        struct zsock_addrinfo *res = NULL;
        int rc = zsock_getaddrinfo(hostname, NULL, &hints, &res);
        if (rc == 0) {
            zsock_freeaddrinfo(res);
            LOG_INF("Mender server '%s' is reachable (attempt %d/%d)",
                    hostname, attempt, MENDER_DNS_PROBE_MAX_ATTEMPTS);
            return true;
        }
        LOG_WRN("DNS probe %d/%d for '%s' failed (%s)%s",
                attempt, MENDER_DNS_PROBE_MAX_ATTEMPTS, hostname,
                zsock_gai_strerror(rc),
                (attempt < MENDER_DNS_PROBE_MAX_ATTEMPTS) ? ", retrying..." : "");
        if (attempt < MENDER_DNS_PROBE_MAX_ATTEMPTS) {
            k_sleep(K_SECONDS(MENDER_DNS_PROBE_INTERVAL_S));
        }
    }

    LOG_WRN("Mender server unreachable after %d attempts — running offline, OTA disabled",
            MENDER_DNS_PROBE_MAX_ATTEMPTS);
    return false;
}


int mender_ota_init(void) {
    LOG_DBG("Initializing Mender OTA...");

    // initialize storage
    if (MENDER_OK != mender_storage_init()) {
        LOG_ERR("Failed to initialize Mender storage");
        return MENDER_FAIL;
    }

    // wait for network.
    // when the network is ready, the network interface ip address is set.
    k_event_wait(&ethernet_if_events, ETHERNET_IF_EVENT_READY, false, K_FOREVER);

    // Probe the Mender server hostname before initialising the client.
    // If DNS does not resolve within the allowed attempts the device is
    // considered offline and OTA is skipped, keeping the app fully responsive.
    if (!wait_for_mender_server_reachable()) {
        return MENDER_FAIL;
    }


    mender_err_t ret = MENDER_OK;

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
    // add TLS credentials
#ifdef CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_PRIMARY
    if (0 != tls_credential_add(CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_PRIMARY,
                                TLS_CREDENTIAL_CA_CERTIFICATE,
                                ca_cert_primary,
                                sizeof(ca_cert_primary))) {
        LOG_ERR("Failed to add primary CA certificate");
        ret = MENDER_FAIL;
    }
#endif // CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_PRIMARY

#ifdef CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_SECONDARY
    if (0 != tls_credential_add(CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_SECONDARY,
                                       TLS_CREDENTIAL_CA_CERTIFICATE,
                                       ca_cert_secondary,
                                       sizeof(ca_cert_secondary))) {
        LOG_ERR("Failed to add secondary CA certificate");
        ret = MENDER_FAIL;
    }
#endif // CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_SECONDARY
#endif // CONFIG_NET_SOCKETS_SOCKETS_TLS

    // prepare data for Mender OTA
    // char *device_type = CONFIG_BOARD; // use board name as device type
    char *device_type = CMAKE_PROJECT_NAME; // use device type from cmake config

    // read MAC address
    struct net_if *iface = net_if_get_default();
    struct net_linkaddr *linkaddr = net_if_get_link_addr(iface);
    assert(NULL != linkaddr);
    snprintf(mac_address, sizeof(mac_address), "%02x:%02x:%02x:%02x:%02x:%02x",
             linkaddr->addr[0], linkaddr->addr[1], linkaddr->addr[2],
             linkaddr->addr[3], linkaddr->addr[4], linkaddr->addr[5]);
    LOG_DBG("MAC address of the device '%s'", mac_address);

    mender_client_config_t    mender_client_config    = { .device_type                  = device_type,
                                                          .host                         = NULL,
                                                          .tenant_token                 = NULL,
                                                          .recommissioning              = false };
    mender_client_callbacks_t mender_client_callbacks = { .network_connect        = mender_network_connect_cb,
                                                          .network_release        = mender_network_release_cb,
                                                          .deployment_status      = mender_deployment_status_cb,
                                                          .restart                = mender_restart_cb,
                                                          .get_identity           = mender_get_identity_cb,
                                                          .get_user_provided_keys = NULL, };

    // initialize mender client
    LOG_INF("Initializing Mender Client with:");
    LOG_INF("   Device type:   '%s'", mender_client_config.device_type);
    LOG_INF("   Identity:      '{\"%s\": \"%s\"}'", mender_identity.name, mender_identity.value);

    if (MENDER_OK != mender_client_init(&mender_client_config, &mender_client_callbacks)) {
        LOG_ERR("Failed to initialize the client");
        goto END;
    }
    LOG_INF("Mender client initialized");

END:
    return ret;
}

void mender_ota_task(void *p1, void *p2, void *p3) {
    if (MENDER_OK != mender_ota_init()) {
        LOG_ERR("Failed to initialize Mender OTA");
        goto exit;
    }

#ifdef CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE
    if (MENDER_OK != (mender_zephyr_image_register_update_module())) {
        /* error already logged */
        goto exit;
    }
#endif /* CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE */

    // activate the mender client
    if (MENDER_OK != mender_client_activate()) {
        LOG_ERR("Failed to activate Mender client");
        goto exit;
    }
    LOG_INF("Mender OTA task started");

exit:
    // Update the system status to SYSTEM_STATUS_OK when exiting the OTA task (e.g. if activation failed),
    // so the rest of the app can continue to run.
    system_info_set_status(SYSTEM_STATUS_OK);
}
