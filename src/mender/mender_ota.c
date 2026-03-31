#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/net/net_if.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mender_ota, LOG_LEVEL_DBG);

#include <mender/utils.h>
#include <mender/client.h>
#include <mender/zephyr-image-update-module.h>

#include "ethernet_if.h"
#include "storage.h"
#include "cmake_config.h"
#include "system_info.h"

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
        return;
    }

#ifdef CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE
    if (MENDER_OK != (mender_zephyr_image_register_update_module())) {
        /* error already logged */
        return;
    }
#endif /* CONFIG_MENDER_ZEPHYR_IMAGE_UPDATE_MODULE */

    // activate the mender client
    if (MENDER_OK != mender_client_activate()) {
        LOG_ERR("Failed to activate Mender client");
        return;
    }
    LOG_INF("Mender OTA task started");
}
