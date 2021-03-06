/*
 * ping_plip.c - ping plip side test mode
 *
 * Written by
 *  Christian Vogelgsang <chris@vogelgsang.org>
 *
 * This file is part of plip2slip.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "ping_plip.h"
#include "pkt_buf.h"
#include "board.h"
#include "ser_parse.h"
#include "pkt_buf.h"
#include "plip.h"
#include "uart.h"
#include "uartutil.h"
#include "ip.h"
#include "bench.h"
#include "param.h"
#include "stats.h"
#include "error.h"
#include "log.h"

static u16 pos;

static u08 begin_rx(plip_packet_t *pkt)
{
  pos = 0;
  return PLIP_STATUS_OK;
}

static u08 fill_rx(u08 *data)
{
  if(pos < PKT_BUF_SIZE) {
    pkt_buf[pos++] = *data;
  }
  return PLIP_STATUS_OK;
}

static u08 fill_tx(u08 *data)
{
  if(pos < PKT_BUF_SIZE) {
    *data = pkt_buf[pos++];
  }
  return PLIP_STATUS_OK;
}

static u08 end_rx(plip_packet_t *pkt)
{
  return PLIP_STATUS_OK;
}

void ping_plip_loop(void)
{
  plip_recv_init(begin_rx, fill_rx, end_rx);
  plip_send_init(fill_tx);
  ser_parse_set_data_func(0); // use serial echo

  bench_begin();
  
  led_green_on(); 
  while(param.mode == PARAM_MODE_PING_PLIP) {
    ser_parse_worker();
    error_worker();
    
    // is a PLIP packet ready to be received?
    u08 status = plip_can_recv();
    if(status != PLIP_STATUS_IDLE) {
      led_green_off();

      // get PLIP packet
      status = plip_recv(&pkt);
      if(status == PLIP_STATUS_OK) {
        
        // account receive
        stats.rx_cnt ++;
        stats.rx_bytes += pkt.size;
         
        // is a ping packet?
        if(ip_icmp_is_ping_request(pkt_buf)) {
          u16 pkt_size = ip_hdr_get_size(pkt_buf);
          
          // transform into reply packet
          ip_icmp_ping_request_to_reply(pkt_buf);

          // send reply packet via PLIP
          pos = 0;
          status = plip_send(&pkt);
          if(status == PLIP_STATUS_CANT_SEND) {
            uart_send('C');
            error_add();
            stats.tx_drop ++;
          } else if(status != PLIP_STATUS_OK) {
            uart_send('T');
            error_add();
            log_add(status);
            stats.tx_err ++;
          } else {
            // send ok!
            
            // do benchmarking
            u16 count = bench_submit(pkt_size);
            if(count == 256) {
              bench_end();

              // send bench result via serial
              uart_send_crlf();
              bench_dump();

              bench_begin();
            }
            
            // update send stats
            stats.tx_cnt ++;
            stats.tx_bytes += pkt.size;
          }          
        } else {
          // no ICMP request
          uart_send('?');
          uart_send_hex_byte_spc(pkt_buf[0]);
          uart_send_hex_byte_spc(pkt_buf[9]);
          uart_send_hex_byte_spc(pkt_buf[20]);
        }
      } else {
        // receive error
        stats.rx_err ++;
        log_add(status);
        error_add();
        uart_send('R');
      }

      led_green_on();
    }
  }
  led_green_off();
  
  bench_end();
}

