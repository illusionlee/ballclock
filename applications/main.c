/*
 * Copyright (c) Illusion
 *
 * License: MIT
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-05-26     Illusion     first
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "drv_wifi.h"
#include "string.h"

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define NET_READY_TIME_OUT       (rt_tick_from_millisecond(100 * 1000))
static char WLAN_SSID[64] = "SSID";
static char WLAN_PASSWORD[64] = "password";

extern void init_pwm(void);
extern void hour(void);
extern void mins_high(void);
extern void mins_low(void);

static struct rt_semaphore net_ready;

time_t now_time_t;
struct tm *now_time_tm;
#define THREAD_PRIORITY         20
#define THREAD_TIMESLICE        10

ALIGN(RT_ALIGN_SIZE)
static char time_stack[1024];
static struct rt_thread time_thread;

static void time_entry(void *param) {
    do {
        rt_thread_mdelay(1000);
        now_time_t = time(NULL);
        now_time_tm = localtime(&now_time_t);
        LOG_D("wait ntp...\n");
    } while (now_time_tm->tm_year < 120);

    int Hour_old = now_time_tm->tm_hour;
    int Mins_H_old = now_time_tm->tm_min / 10;
    int Mins_L_old = now_time_tm->tm_min % 10;
    int Hour_new = Hour_old;
    int Mins_H_new = Mins_H_old;
    int Mins_L_new = Mins_L_old;
    LOG_D("%d:%d%d", Hour_new, Mins_H_new, Mins_L_new);
    while (1) {
        now_time_t = time(NULL);
        now_time_tm = localtime(&now_time_t);
        Hour_new = now_time_tm->tm_hour;
        Mins_H_new = now_time_tm->tm_min / 10;
        Mins_L_new = now_time_tm->tm_min % 10;
        if (Hour_new != Hour_old) {
            hour();
        } else if (Mins_H_new != Mins_H_old) {
            mins_high();
        } else if (Mins_L_new != Mins_L_old) {
            mins_low();
        }
        Hour_old = Hour_new;
        Mins_H_old = Mins_H_new;
        Mins_L_old = Mins_L_new;
        rt_thread_mdelay(1000);
    }
}

void time_thread_func(void) {
    rt_thread_init(&time_thread,
                   "time_check",
                   time_entry,
                   RT_NULL,
                   &time_stack[0],
                   sizeof(time_stack),
                   THREAD_PRIORITY, THREAD_TIMESLICE);
    rt_thread_startup(&time_thread);
    return 0;
}

void wlan_ready_handler(int event, struct rt_wlan_buff *buff, void *parameter) {
    rt_sem_release(&net_ready);
}

/* 断开连接回调函数 */
void wlan_station_disconnect_handler(int event, struct rt_wlan_buff *buff, void *parameter) {
    LOG_I("callback from the offline network!");
}

int main(void) {
    int result = RT_EOK;
    struct rt_wlan_info info;

    /* 配置 wifi 工作模式 */
    rt_wlan_set_mode(RT_WLAN_DEVICE_STA_NAME, RT_WLAN_STATION);

    LOG_D("start to connect ap ...");
    rt_sem_init(&net_ready, "net_ready", 0, RT_IPC_FLAG_FIFO);

    /* 注册 wlan ready 回调函数 */
    rt_wlan_register_event_handler(RT_WLAN_EVT_READY, wlan_ready_handler, RT_NULL);
    /* 注册 wlan 断开回调函数 */
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_DISCONNECTED, wlan_station_disconnect_handler, RT_NULL);
    result = rt_wlan_connect(WLAN_SSID, WLAN_PASSWORD);
    if (result == RT_EOK) {
        rt_memset(&info, 0, sizeof(struct rt_wlan_info));
        /* 等待成功获取 IP */
        result = rt_sem_take(&net_ready, NET_READY_TIME_OUT);
        if (result == RT_EOK) {
            LOG_D("networking ready!");
            init_pwm();
            time_thread_func();
        } else {
            LOG_D("wait ip got timeout!");
        }
        /* 回收资源 */
        rt_wlan_unregister_event_handler(RT_WLAN_EVT_READY);
        rt_sem_detach(&net_ready);
    } else {
        LOG_E("The AP(%s) is connect failed!", WLAN_SSID);
    }

    return 0;
}

#define PWM_DEV_NAME        "pwm"   /* PWM设备名称 */
#define PWM_DEV_CHANNEL     5       /* PWM通道 */
static rt_uint32_t PWM_PERIOD = 5000000;   /* 周期为5ms，单位为纳秒ns */

struct rt_device_pwm *pwm_dev;      /* PWM设备句柄 */

void init_pwm(void) {
    /* 查找设备 */
    pwm_dev = (struct rt_device_pwm *)rt_device_find(PWM_DEV_NAME);
    if (pwm_dev == RT_NULL) {
        rt_kprintf("pwm run failed! can't find %s device!\n", PWM_DEV_NAME);
    }
}

void mins_low(void) {
    rt_kprintf("mins_low!\n");
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*16);
    rt_pwm_enable(pwm_dev, PWM_DEV_CHANNEL);
    rt_thread_mdelay(1000);
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*32);
    rt_thread_mdelay(1000);
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*27);
    rt_thread_mdelay(1000);
    rt_pwm_disable(pwm_dev, PWM_DEV_CHANNEL);
}

void mins_high(void) {
    rt_kprintf("mins_high!\n");
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*14);
    rt_pwm_enable(pwm_dev, PWM_DEV_CHANNEL);
    rt_thread_mdelay(1000);
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*16);
    rt_thread_mdelay(1000);
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*27);
    rt_thread_mdelay(1000);
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*32);
    rt_thread_mdelay(1000);
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*27);
    rt_thread_mdelay(1000);
    rt_pwm_disable(pwm_dev, PWM_DEV_CHANNEL);
}

void hour(void) {
    rt_kprintf("hour!\n");
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*11);
    rt_pwm_enable(pwm_dev, PWM_DEV_CHANNEL);
    rt_thread_mdelay(1000);
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*14);
    rt_thread_mdelay(1000);
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*16);
    rt_thread_mdelay(1000);
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*27);
    rt_thread_mdelay(1000);
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*29);
    rt_thread_mdelay(1000);
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*32);
    rt_thread_mdelay(1000);
    rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD, PWM_PERIOD/100*27);
    rt_thread_mdelay(1000);
    rt_pwm_disable(pwm_dev, PWM_DEV_CHANNEL);
}

MSH_CMD_EXPORT(hour, Change hour ball.);
MSH_CMD_EXPORT(mins_high, Change mins_high ball.);
MSH_CMD_EXPORT(mins_low, Change mins_low ball.);