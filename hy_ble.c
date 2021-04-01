/******************************************************************************
 * File Name   : hy_ble.c
 *
 * Author      : ganxiao
 *
 * Version     : 1.0
 *
 * Date        : 2020-10-22
 *
 * DESCRIPTION : -
 *
 * --------------------
 * Copyright 2009-2020 Hymost Co.,Ltd.
 *
 ******************************************************************************/
#include "hy_public.h"
/*[HYN001-18] Add ble function by weibole at 20201202 begin*/

#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */
#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */
static uint16_t   m_conn_handle          =
    BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
NRF_BLE_QWR_DEF(m_qwr);

char DEVICE_NAME[8] = {0};


#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(75, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */

NRF_BLE_GATT_DEF(m_gatt);                       /**< GATT module instance. */



BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                                   /**< BLE NUS service instance. */

#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */
#define APP_ADV_INTERVAL                320 /*64*/                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */
#define APP_ADV_DURATION                18000                                       /**< The advertising duration (180 seconds) in units of 10 milliseconds. */
uint32_t bleReportInterval = APP_ADV_INTERVAL;
uint32_t bleAdvertInterval = APP_ADV_DURATION; // default 1s

APP_TIMER_DEF(hy_ble_adv_timer);

BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

/*[HYN001-44] Add OTA functionality, including HTTP and BLE mode by zhoushaoqing at 20201230 begin*/
ADUPS_BLE_NUS_DEF(m_adups_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT); /**< BLE NUS service instance. */
/*[HYN001-44] Add OTA functionality, including HTTP and BLE mode by zhoushaoqing at 20201230 end*/


/*[HYN001-86] Added setting parameter and read parameter handling by zhoushaoqing at 20210225 begin*/
int setconfigFlag = 0;
char configdata[SET_CONFIG_MAX_LENGTH] = {0};
/*[HYN001-86] Added setting parameter and read parameter handling by zhoushaoqing at 20210225 end*/
static ble_uuid_t m_adv_uuids[]
=                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};

static void hy_power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("bt init power fail: %d", err_code);
    }
}

static void renameBle(void)
{

    char kmstr[4] = "HYN";

    s8 num = 0;
    u8 strValue[IMEI_MAX_LEN + 1] = {0};
    u8 value[IMEI_MAX_LEN] = {0};
    num = fds_read_imei(value);
    memset(DEVICE_NAME, 0, sizeof(DEVICE_NAME));
    sprintf(DEVICE_NAME, "%s", kmstr);
    if (num > 0 && strlen(value) >= 15)
    {
        conver(&value[0], strValue, num);
        sprintf(&DEVICE_NAME[3], "%s", &strValue[12]);
        NRF_LOG_INFO("DEVICE_NAME=%s", DEVICE_NAME);
    }
}

void hy_ble_write_mac_addr(void)
{
    u8 rc = FDS_ERR_CRC_CHECK_FAILED;
    u8 btMac[6] = {0};
    ble_gap_addr_t addr;
    u8 num = 0;
    rc = fds_read_bt_mac_addr(btMac);
    if (rc == FDS_SUCCESS)
    {
        if ((btMac[0] != 0) || (btMac[1] != 0) || (btMac[2] != 0) || (btMac[3] != 0) || (btMac[4] != 0) || (btMac[5] != 0))
        {
            for (num = 0; num < 6; num++)
            {
                addr.addr[5 - num] = btMac[num];
            }
            addr.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC;
            sd_ble_gap_addr_set(&addr);
        }
    }
}

static void hy_ble_event_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            hy_bleStopAdvStopReportTime();
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected");
            hy_advertising_re_start();
            // LED indication will be changed when advertising starts.
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        }
        break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}

static void hy_ble_stack_init(void)
{
    ret_code_t err_code = 0;
    uint32_t ram_start = 0;

    err_code = nrf_sdh_enable_request();
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("bt sdh enable request fail: %d", err_code);
    }

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("bt set sdh default cfg fail: %d", err_code);
    }

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("bt enable ble fail: %d", err_code);
    }

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, hy_ble_event_handler, NULL);
}

static void hy_gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


static void hy_gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("bt init gatt fail: %d", err_code);
    }
}

static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}
/*[HYN001-86] Added setting parameter and read parameter handling by zhoushaoqing at 20210225 begin*/
static void handleCommand(uint8_t *str, uint16_t length)
{
    uint16_t maxLength = current_mtu - 3;
    uint16_t  totalLength = length;
    if (maxLength < length)
    {
        return;
    }
    ble_nus_data_send(&m_nus, str, &totalLength, m_conn_handle);
}
/*[HYN001-86] Added setting parameter and read parameter handling by zhoushaoqing at 20210225 end*/
static void nus_data_handler(ble_nus_evt_t *p_evt)
{
    if (p_evt->type == BLE_NUS_EVT_RX_DATA)
    {
        uint32_t err_code;

        NRF_LOG_DEBUG("Received data from BLE NUS.");
        NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
        /*[HYN001-86] Added setting parameter and read parameter handling by zhoushaoqing at 20210225 begin*/
		uint8_t *hwVersionStr = (uint8_t *)HW_Ver;
        uint8_t *swVersionStr = (uint8_t *)SW_Ver;
        char tempBuf[32] = {0};
    if (p_evt->type == BLE_NUS_EVT_RX_DATA)
    {
        //static uint8_t index = 0;
        //uint32_t       err_code;
        int rxValue = atoi((char *)(p_evt->params.rx_data.p_data));
        NRF_LOG_INFO("p_evt->params.rx_data.p_data=====%s\n", p_evt->params.rx_data.p_data);
        switch (rxValue)
        {
            case READ_HW_COMMAND:
                handleCommand(hwVersionStr, strlen((char *)hwVersionStr));
                break;
            case READ_SW_COMMAND:
                handleCommand(swVersionStr, strlen((char *)swVersionStr));
                break;
            case READ_UPGRADE:
                memset(tempBuf, 0, 32);
                fds_read_Upgrade(tempBuf);
                handleCommand((uint8_t *)tempBuf, strlen(tempBuf));
                break;
            case REBOOT_COMMAND:
                //reboot device
                fds_gc_resource(0);
                led_brighting(GREEN_LED);
                nrf_delay_ms(200);
                led_Dimming(RED_LED);
                nrf_delay_ms(200);
                led_brighting(GREEN_LED);
                nrf_delay_ms(200);
                led_Dimming(RED_LED);
                nrf_delay_ms(200);
                led_brighting(GREEN_LED);
                nrf_delay_ms(200);
                led_Dimming(RED_LED);
                NVIC_SystemReset();
                break;
            default:
                if (!strncmp((char *)p_evt->params.rx_data.p_data, "{", 1))
                {
                    memset(configdata, 0, SET_CONFIG_MAX_LENGTH);
                    memcpy(configdata, (char *) p_evt->params.rx_data.p_data, strlen((char *) p_evt->params.rx_data.p_data));
                    setconfigFlag = 1;
                }
                break;
        }
        /*[HYN001-86] Added setting parameter and read parameter handling by zhoushaoqing at 20210225 begin*/
    }
    }
}
/*[HYN001-44] Add OTA functionality, including HTTP and BLE mode by zhoushaoqing at 20201230 begin*/
static void adups_nus_data_handler(adups_ble_nus_evt_t *p_evt)
{
    if (p_evt->type == ADUPS_BLE_NUS_EVT_RX_DATA)
    {
        NRF_LOG_INFO("Received data from BLE NUS. Writing data on UART.");
    }
}
/*[HYN001-44] Add OTA functionality, including HTTP and BLE mode by zhoushaoqing at 20201230 end*/
static void services_init(void)
{
    uint32_t           err_code;
    ble_nus_init_t     nus_init;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("nrf_ble_qwr_init err_code=%d", err_code);
        return;
    }

    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("ble_nus_init err_code=%d", err_code);
        return;
    }
}

static void on_conn_params_evt(ble_conn_params_evt_t *p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        if (err_code != NRF_SUCCESS)
        {
            NRF_LOG_INFO("sd_ble_gap_disconnect err: %d", err_code);
        }
    }
}

static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

static void hy_gatt_conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("bt conn set par fail: %d", err_code);
    }
}

static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            if (err_code != NRF_SUCCESS)
            {
                NRF_LOG_INFO("ble_advertising_init err: %d", err_code);
            }
            break;
        case BLE_ADV_EVT_IDLE:
            //sleep_mode_enter();
            break;
        default:
            break;
    }
}

static char ble_adv_buf[16];
static char ble_scrp_buf[16];
static void advertising_init(void)
{
    uint32_t               err_code;
    ble_advertising_init_t init;
    ble_advdata_manuf_data_t manuf_specific_data_adv;
    ble_advdata_manuf_data_t manuf_specific_data_scrp;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;//BLE_ADVDATA_NO_NAME;
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;

    //init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    //init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    memset(ble_adv_buf, 0, 16);
    memset(ble_scrp_buf, 0, 16);
    manuf_specific_data_adv.data.p_data = ble_adv_buf;
    manuf_specific_data_adv.data.size   = 15;
    manuf_specific_data_adv.company_identifier = 0xAABB;
    manuf_specific_data_scrp.data.p_data = ble_scrp_buf;
    manuf_specific_data_scrp.data.size   = 15;
    manuf_specific_data_scrp.company_identifier = 0xAABB;

    init.advdata.p_manuf_specific_data = &manuf_specific_data_adv;
    init.srdata.p_manuf_specific_data = &manuf_specific_data_scrp;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = bleReportInterval;//APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = bleAdvertInterval;//APP_ADV_DURATION;
    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("ble_advertising_init err: %d", err_code);
    }

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

static void advertising_start(void)
{
    uint32_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("advertising_start err: %d", err_code);
    }

    hy_BleReportIntervalInit();
}

void hy_ble_reinit_advert_handler(void *p_context)
{
    hy_advertising_re_start();
}


void hy_BleReportIntervalInit(void)
{
    app_timer_create(&hy_ble_adv_timer, APP_TIMER_MODE_REPEATED, hy_ble_reinit_advert_handler);
    app_timer_start(hy_ble_adv_timer, APP_TIMER_TICKS(1000), NULL);
}

void hy_BleReportIntervalReinit(void)
{
    app_timer_stop(hy_ble_adv_timer);
    app_timer_create(&hy_ble_adv_timer, APP_TIMER_MODE_REPEATED, hy_ble_reinit_advert_handler);
    app_timer_start(hy_ble_adv_timer, APP_TIMER_TICKS(1000), NULL);
}

void hy_bleStopAdvStopReportTime(void)
{
    app_timer_stop(hy_ble_adv_timer);
    hy_ble_advertising_stop();
}

void hy_advertising_re_start(void)
{
    uint32_t err_code = 0;
    while (!nrf_fstorage_is_busy(NULL))
    {
        //NRF_LOG_INFO("hy_ble_advertising_stop started.");
        err_code = hy_ble_advertising_stop();
        if (err_code == NRF_SUCCESS)
        {
            // NRF_LOG_INFO("hy_ble_advertising_stop success.");
            break;
        }
    }
    app_timer_stop(hy_ble_adv_timer);
    advertising_init();
    //nrf_delay_ms(100);
    advertising_start();
}

ret_code_t hy_ble_advertising_stop(void)
{
    ret_code_t err_code;
    err_code = sd_ble_gap_adv_stop(m_advertising.adv_handle);
    if (err_code != NRF_SUCCESS)
    {
        //NRF_LOG_INFO("stop advert fail rc=%d.", err_code);
        return err_code;
    }
    return NRF_SUCCESS;
}


void BSP_BleInit(void)
{
    NRF_LOG_INFO("BSP_BleInit start");
    hy_power_management_init();
    hy_ble_stack_init();
    renameBle();
    hy_gap_params_init();
    hy_gatt_init();
    hy_ble_write_mac_addr();
    services_init();
    /*[HYN001-44] Add OTA functionality, including HTTP and BLE mode by zhoushaoqing at 20201230 begin*/
    adups_fota_init();
    /*[HYN001-44] Add OTA functionality, including HTTP and BLE mode by zhoushaoqing at 20201230 end*/
    hy_gatt_conn_params_init();
    advertising_init();

    NRF_LOG_INFO("BSP_BleInit end");
    advertising_start();

}

/*[HYN001-33] add bt factory test by weibole at 20201222 begin*/
#include "nrf_ble_scan.h"
NRF_BLE_SCAN_DEF(m_scan);
char * factoryrsp = NULL;
static void hy_scan_evt_handler(scan_evt_t const *p_scan_evt)
{
    ret_code_t err_code;
    switch(p_scan_evt->scan_evt_id)
    {
        case NRF_BLE_SCAN_EVT_CONNECTING_ERROR:
        {
            err_code = p_scan_evt->params.connecting_err.err_code;
            APP_ERROR_CHECK(err_code);
        }
        break;

        case NRF_BLE_SCAN_EVT_CONNECTED:
        {
            ble_gap_evt_connected_t const * p_connected =p_scan_evt->params.connected.p_connected;
            NRF_LOG_INFO("Connecting to target %02x%02x%02x%02x%02x%02x",
                p_connected->peer_addr.addr[0],
                p_connected->peer_addr.addr[1],
                p_connected->peer_addr.addr[2],
                p_connected->peer_addr.addr[3],
                p_connected->peer_addr.addr[4],
                p_connected->peer_addr.addr[5]
                );
        }
        break;

        case NRF_BLE_SCAN_EVT_SCAN_TIMEOUT:
        {
            NRF_LOG_INFO("Scan timed out.");
        }
        break;
        case NRF_BLE_SCAN_EVT_NOT_FOUND:
        {
            ble_gap_evt_adv_report_t const  * p_not_fnd = p_scan_evt->params.p_not_found;

            if( (factoryrsp[2] == ':') && (factoryrsp[5] == ':') && (factoryrsp[8] == ':') && (factoryrsp[11] == ':')  && (factoryrsp[14] == ':') )
            {
            }
            else
            {
                sprintf(factoryrsp, "%02x:%02x:%02x:%02x:%02x:%02x",
                    p_not_fnd->peer_addr.addr[0],
                    p_not_fnd->peer_addr.addr[1],
                    p_not_fnd->peer_addr.addr[2],
                    p_not_fnd->peer_addr.addr[3],
                    p_not_fnd->peer_addr.addr[4],
                    p_not_fnd->peer_addr.addr[5]
                );
            }
        }
        break;
        default:
        break;
    }
}


static void hy_scan_init(void)
{
    ret_code_t          err_code;

    err_code = nrf_ble_scan_init(&m_scan, NULL, hy_scan_evt_handler);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("bt scan init fail: %d", err_code);
    }
}

static void scan_start(void)
{
    ret_code_t ret;

    ret = nrf_ble_scan_start(&m_scan);
    APP_ERROR_CHECK(ret);

    ret = bsp_indication_set(BSP_INDICATE_SCANNING);
    APP_ERROR_CHECK(ret);
}


int hy_Ble_factoryTest(char *rsp)
{
    ret_code_t          err_code;
    uint64_t rtc_time;
    if(m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        nrf_delay_ms(200);
    }

    hy_bleStopAdvStopReportTime();
    nrf_sdh_disable_request();
    nrf_delay_ms(100);

    hy_ble_stack_init();
    renameBle();
    hy_scan_init();
    hy_gap_params_init();
    factoryrsp = rsp;
    rtc_time = rtc_timeTamp;
    scan_start();
    while(1)
    {
        nrf_delay_ms(10);
        if( (factoryrsp[2] == ':') && (factoryrsp[5] == ':') && (factoryrsp[8] == ':') && (factoryrsp[11] == ':')  && (factoryrsp[14] == ':') )
        {
            nrf_ble_scan_stop();
            return 1;
		}
        if( (rtc_timeTamp - rtc_time ) >= 10)
        {
            nrf_ble_scan_stop();
            return 0;
        }
    }
}

int hy_Ble_writebtmac(char * req, char *rsp)
{
    u8 mac[6];
    u8 * eqstr= "=";
    u8 * colonstr= "?";

    memset(mac, 0, 6);
    eqstr = strstr(req, eqstr);
    if(eqstr == NULL)
    {

        eqstr = strstr(req, colonstr);
        if(eqstr == NULL)
        {
            return 0;
        }
        else
            goto READBTMAC;
    }

    eqstr++;
    for(;*eqstr == ' ';eqstr++);

    for(int i =0 ;i < 12; i ++)
    {
        if( *eqstr == ':')
        {
            eqstr++;
        }

        if( (*eqstr >= '0') && (*eqstr <= '9'))
        {
            mac[i/2] = (mac[i/2] << 4) + *eqstr - '0';
        }
        else if((*eqstr >= 'a') && (*eqstr <= 'f') )
        {
            mac[i/2] = (mac[i/2] << 4) + *eqstr - 'a' + 10;
        }
        else if((*eqstr >= 'A') && (*eqstr <= 'F') )
        {
            mac[i/2] = (mac[i/2] << 4) + *eqstr - 'A' + 10;
        }
        else
            return 0;
        eqstr++;
    }

    if(fds_write_bt_mac_addr(mac) == FDS_SUCCESS)
        return 1;
    else
        return 0;

READBTMAC:
    if(fds_read_bt_mac_addr(mac) != FDS_SUCCESS)
        return 0;
    sprintf(rsp, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return 1;
}
/*[HYN001-33] add bt factory test by weibole at 20201222 end*/
/*[HYN001-86] Added setting parameter and read parameter handling by zhoushaoqing at 20210225 begin*/
static void notifyAppSetConfigAck(void)
{
    uint8_t *REPORT_DATA_END = (uint8_t *) "cfgack";
    handleCommand(REPORT_DATA_END, 6);
}
void parseConfigData()
{
    char *pcBegin = NULL;
    char *pcEnd = NULL;
    char finalData[SET_CONFIG_MAX_LENGTH] = {0};
    char delims[] = ":,";
    char *result = NULL;
    char result_str[33] = {0};
    pcBegin = strstr(configdata, "{");
    pcEnd = strstr(configdata, "}");

    if (pcBegin == NULL || pcEnd == NULL || pcBegin > pcEnd)
    {
         NRF_LOG_INFO("config data is error");
    }
    else
    {
        pcBegin += strlen("{");
        memcpy(finalData, pcBegin, pcEnd - pcBegin);
    }

    result = strtok(finalData, delims);
    int index = 0;
    while (result != NULL)
    {
        if (strstr(result, UPGRADE_NAME))
        {
            index = 1;
        }
        else
        {
            memcpy(result_str, result + 1, strlen(result) - 1);
            switch (index)
            {
                case 1:
                    if (strstr(result_str, "Y"))
                    {
                    fds_write_Upgrade(READY_DOWNLOAD);
                    }
                    else
                    {
                    fds_write_Upgrade(NORMAL_MODE);
                    }
                    break;
				default:
                    break;
            }
        }
        result = strtok(NULL, delims);
    }
    notifyAppSetConfigAck();
}
/*[HYN001-86] Added setting parameter and read parameter handling by zhoushaoqing at 20210225 end*/
void BLE_Pro(void)
{
}

/*[HYN001-18] Add ble function by weibole at 20201202 end*/