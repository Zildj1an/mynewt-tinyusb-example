/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <assert.h>
#include <string.h>

#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#include "nrfx_power.h"
#include "tusb.h"

static volatile int g_task1_loops;

/* tinyusb function that handles power event (detected, ready, removed)
 * We must call it within SD's SOC event handler, or set it as power event handler if SD is not enabled.
 */
extern void tusb_hal_nrf_power_event(uint32_t event);

#define USBD_STACK_SIZE   128
static struct os_task usbd_tsk;
static os_stack_t usbd_stack[OS_STACK_ALIGN(USBD_STACK_SIZE)];

void POWER_CLOCK_IRQHandler(void);
void USBD_IRQHandler(void);
void usb_device_task(void* param);

int main (int argc, char **argv)
{
  int rc;

#ifdef ARCH_sim
  mcu_sim_parse_args(argc, argv);
#endif

  sysinit();

  NVIC_SetVector(POWER_CLOCK_IRQn, (uint32_t) POWER_CLOCK_IRQHandler);
  NVIC_SetVector(USBD_IRQn, (uint32_t) USBD_IRQHandler);
  NVIC_SetPriority(USBD_IRQn, 2);

  // Power module init
  nrf_power_dcdcen_set(0);
  nrf_power_int_enable(NRF_POWER_INT_USBDETECTED_MASK | NRF_POWER_INT_USBREMOVED_MASK | NRF_POWER_INT_USBPWRRDY_MASK);

  NRFX_IRQ_PRIORITY_SET(POWER_CLOCK_IRQn, 3);
  NRFX_IRQ_ENABLE(POWER_CLOCK_IRQn);

  uint32_t usb_reg = NRF_POWER->USBREGSTATUS;

  if ( usb_reg & POWER_USBREGSTATUS_VBUSDETECT_Msk )
  {
    tusb_hal_nrf_power_event(NRFX_POWER_USB_EVT_DETECTED);
  }

  if ( usb_reg & POWER_USBREGSTATUS_OUTPUTRDY_Msk )
  {
    tusb_hal_nrf_power_event(NRFX_POWER_USB_EVT_READY);
  }

  tusb_init();

  // Create a task for tinyusb device stack
  os_task_init(&usbd_tsk, "task1", usb_device_task, NULL, OS_TASK_PRI_HIGHEST+2, OS_WAIT_FOREVER, usbd_stack, USBD_STACK_SIZE);

  hal_gpio_init_out(LED_BLINK_PIN, 1);

  while ( 1 )
  {
    ++g_task1_loops;

    /* Wait one second */
    os_time_delay(OS_TICKS_PER_SEC);

    /* Toggle the LED */
    hal_gpio_toggle(LED_BLINK_PIN);
  }
  assert(0);

  return rc;
}

// USB Device Driver task
// This top level thread process all usb events and invoke callbacks
void usb_device_task(void* param)
{
  (void) param;

  // RTOS forever loop
  while (1)
  {
    // tinyusb device task
    tud_task();
  }
}


void POWER_CLOCK_IRQHandler (void)
{
  uint32_t enabled = nrf_power_int_enable_get();

  if ( (0 != (enabled & NRF_POWER_INT_POFWARN_MASK)) &&
       nrf_power_event_get_and_clear(NRF_POWER_EVENT_POFWARN) )
  {
    /* Cannot be null if event is enabled */
//        NRFX_ASSERT(m_pofwarn_handler != NULL);
//        m_pofwarn_handler();
  }

  if ( (0 != (enabled & NRF_POWER_INT_SLEEPENTER_MASK)) &&
       nrf_power_event_get_and_clear(NRF_POWER_EVENT_SLEEPENTER) )
  {
    /* Cannot be null if event is enabled */
//        NRFX_ASSERT(m_sleepevt_handler != NULL);
//        m_sleepevt_handler(NRFX_POWER_SLEEP_EVT_ENTER);
  }
  if ( (0 != (enabled & NRF_POWER_INT_SLEEPEXIT_MASK)) &&
       nrf_power_event_get_and_clear(NRF_POWER_EVENT_SLEEPEXIT) )
  {
    /* Cannot be null if event is enabled */
//        NRFX_ASSERT(m_sleepevt_handler != NULL);
//        m_sleepevt_handler(NRFX_POWER_SLEEP_EVT_EXIT);
  }

  if ( (0 != (enabled & NRF_POWER_INT_USBDETECTED_MASK)) && (NRF_POWER_EVENT_USBDETECTED) )
  {
    tusb_hal_nrf_power_event(NRFX_POWER_USB_EVT_DETECTED);
  }
  if ( (0 != (enabled & NRF_POWER_INT_USBREMOVED_MASK)) && nrf_power_event_get_and_clear(NRF_POWER_EVENT_USBREMOVED) )
  {
    tusb_hal_nrf_power_event(NRFX_POWER_USB_EVT_REMOVED);
  }
  if ( (0 != (enabled & NRF_POWER_INT_USBPWRRDY_MASK)) && nrf_power_event_get_and_clear(NRF_POWER_EVENT_USBPWRRDY) )
  {
    tusb_hal_nrf_power_event(NRFX_POWER_USB_EVT_READY);
  }
}
