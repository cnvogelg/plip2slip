/*
 * uartutil.c - serial utility routines
 *
 * Written by
 *  Christian Vogelgsang <chris@vogelgsang.org>
 *
 * This file is part of plip2lip.
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

#include "uart.h"
#include "util.h"

u08 uart_send_string(const char *str)
{
  while(*str) {
    if(!uart_send((u08)*str))
      return 0;
    str++;
  }
  return 1;
}

u08 uart_send_data(u08 *data,u08 len)
{
  for(u08 i=0;i<len;i++) {
    if(!uart_send(data[i]))
      return 0;
  }
  return 1;
}

u08 uart_send_crlf(void)
{
  return uart_send_string("\r\n");
}

u08 uart_send_spc(void)
{
  return uart_send((u08)' ');
}

static u08 buf[8];

u08 uart_send_hex_byte_crlf(u08 data)
{
  byte_to_hex(data,buf);
  if(uart_send_data(buf,2))
    return uart_send_crlf();
  else
    return 0;
}

u08 uart_send_hex_byte_spc(u08 data)
{
  byte_to_hex(data,buf);
  if(uart_send_data(buf,2))
    return uart_send_spc();
  else
    return 0;
}

u08 uart_send_hex_word_crlf(u16 data)
{
  word_to_hex(data,buf);
  if(uart_send_data(buf,4))
    return uart_send_crlf();
  else
    return 0;
}

u08 uart_send_hex_dword_crlf(u32 data)
{
  dword_to_hex(data,buf);
  if(uart_send_data(buf,8))
    return uart_send_crlf();
  else
    return 0;
}

