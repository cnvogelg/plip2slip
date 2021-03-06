/*
 * ser_parse.c - handle serial line live and command mode
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

#include "ser_parse.h"

#include "timer.h"
#include "board.h"
#include "uart.h"
#include "uartutil.h"

// parser states
#define STATE_WAIT_FOR_DATA    0
#define STATE_IN_FIRST_DELAY   1
#define STATE_GET_MARKERS      2
#define STATE_IN_LAST_DELAY    3
#define STATE_COMMAND_MODE     4

// options
u08 ser_marker_char = '+'; // send DELAY +++ DELAY to enter command mode
u08 ser_marker_count = 3;
u16 ser_marker_wait_delay = 100; // 1000 ms
u16 ser_marker_wait_char = 50; // 500 ms
u16 ser_command_timeout = 1000; // 10 s

static u08 state = STATE_WAIT_FOR_DATA; // wait for a serial char
static u08 num_markers = 0;
static u08 data = 0;

#define MAX_CMD_LINE_LEN  16
static u08 cmd_line[MAX_CMD_LINE_LEN];
static u08 cmd_pos;

static ser_parse_data_func_t data_func = 0;
static ser_parse_cmd_func_t cmd_func = 0;
static ser_parse_func_t cmd_enter_func = 0;
static ser_parse_func_t cmd_leave_func = 0;

static u08 read_char(u08 echo)
{
  // a char arrived
  u08 status = uart_read(&data);
  
  if(echo) {
    // pass through data
    if(data_func != 0) {
      data_func(data);
    }
  }
  
  timer_10ms = 0;
  return status;
}

static void push_char(u08 d)
{
  // pass through data
  if(data_func != 0) {
    data_func(d);
  }  
}

static void push_markers(void)
{
  for(u08 i=0;i<num_markers;i++) {
    push_char(ser_marker_char);
  }
}

static void send_ok(void)
{
  uart_send_string("OK");
  uart_send_crlf();
}

static void send_bye(void)
{
  uart_send_string("BYE");
  uart_send_crlf();
}

static void send_fail(void)
{
  uart_send_string("FAIL");
  uart_send_crlf();
}

static void send_huh(void)
{
  uart_send_string("??");
  uart_send_crlf();
}

static void send_prompt(void)
{
  uart_send_string("> ");
}

static void handle_command_char(void)
{
  switch(data) {
    case '\r':
      uart_send(data);
      break;
    case '\n':
      uart_send(data);
      cmd_line[cmd_pos] = '\0';
      u08 exit_cmd = 0;
      // command is not empty
      if(cmd_pos > 0) {
        // we have a command handler installed -> call it!
        if(cmd_func != 0) {        
          u08 status = cmd_func(cmd_pos, (const char *)cmd_line);
          // handler told us to exit
          if(status == SER_PARSE_CMD_EXIT) {
            exit_cmd = 1;
          } 
          // handler does not know this command
          else if(status == SER_PARSE_CMD_FAIL) {
            send_fail();
          }
          else if(status == SER_PARSE_CMD_UNKNOWN) {
            send_huh();
          }
          else if(status == SER_PARSE_CMD_OK) {
            send_ok();
          }
        } 
        // no command handler -> exit command mode now
        else {
          exit_cmd = 1;
        }
        // exit command mode
        if(exit_cmd) {
          send_bye();
          led_yellow_off();
          state = STATE_WAIT_FOR_DATA;
          if(cmd_leave_func != 0) {
            cmd_leave_func();
          }
        }
        cmd_pos = 0;
      } 
      // empty command gives ok
      else {
        send_ok();
      }
      // show prompt for next command if its no exit
      if(state != STATE_WAIT_FOR_DATA) {
        send_prompt();
      }
      break;
    default:
      if(cmd_pos < (MAX_CMD_LINE_LEN-1)) {
        cmd_line[cmd_pos] = data;
        cmd_pos++;
        uart_send(data); // echo character
      }
      break;
  }
}

static void ser_echo(u08 data)
{
  uart_send(data);
}

void ser_parse_set_data_func(ser_parse_data_func_t df)
{
  if(df == 0) {
    data_func = ser_echo;
  } else {
    data_func = df;
  }
}

void ser_parse_set_cmd_func(ser_parse_cmd_func_t cf)
{
  cmd_func = cf;
}

void ser_parse_set_cmd_enter_leave_func(ser_parse_func_t enter_func, ser_parse_func_t leave_func)
{
  cmd_enter_func = enter_func;
  cmd_leave_func = leave_func;
}

u08 ser_parse_worker(void)
{
  u08 old_state = state;
  
  switch(state) {
    // waiting for incoming data
    case STATE_WAIT_FOR_DATA:
      if(uart_read_data_available()) {
        // get char and transmit it
        read_char(1);
      } else {
        // nothing arrived in delay time
        if(timer_10ms >= ser_marker_wait_delay) {
          timer_10ms = 0;
          state = STATE_IN_FIRST_DELAY;
        }
      }
      break;
    // first delay before marker passed
    case STATE_IN_FIRST_DELAY:
      // get a char and check for marker
      if(uart_read_data_available()) {
        if(read_char(0)) {
          // is it the marker char?
          if(data == ser_marker_char) {
            num_markers = 1;
            state = STATE_GET_MARKERS;
          } 
          // not a marker char!
          else {
            push_char(data);
            state = STATE_WAIT_FOR_DATA;
          }
        } else {
          state = STATE_WAIT_FOR_DATA;
        }
      } 
      break;
    // waiting for marker chars after first delay
    case STATE_GET_MARKERS:
      // next marker char?
      if(uart_read_data_available()) {
        if(read_char(0)) {
          // is it the next marker char?
          if(data == ser_marker_char) {
            num_markers ++;
            if(num_markers == ser_marker_count) {
              state = STATE_IN_LAST_DELAY;
            } else {
              state = STATE_GET_MARKERS;
            }
          } 
          // no marker char!
          else {
            push_markers();
            push_char(data);
            state = STATE_WAIT_FOR_DATA;
          }
        }
      }
      // no marker char arrived in time
      else if(timer_10ms >= ser_marker_wait_char) {
        push_markers();
        state = STATE_WAIT_FOR_DATA;
      }
      break;
    // last delay after markers
    case STATE_IN_LAST_DELAY:
      // if we get a char then abort
      if(uart_read_data_available()) {
        push_markers();
        read_char(1);
        state = STATE_WAIT_FOR_DATA;
      } 
      // delay passed -> enter command mode
      else if(timer_10ms >= ser_marker_wait_delay) {
        state = STATE_COMMAND_MODE;

        // enter command mode
        if(cmd_enter_func != 0) {
          cmd_enter_func();
        }
        led_yellow_on();
        send_ok();
        send_prompt();
        cmd_pos = 0;
      }
      break;
    // command mode
    case STATE_COMMAND_MODE:
      // get command chars
      if(uart_read_data_available()) {
        if(uart_read(&data)) {
          handle_command_char();
          // reset timer after command execution
          timer_10ms = 0;
        }
      }
      // no char entered in a long time? -> leave command mode
      else if(timer_10ms >= ser_command_timeout) {
        led_yellow_off();
        send_bye();
        state = STATE_WAIT_FOR_DATA;
      }
      break;
  }

//#define DEBUG_STATE
#ifdef DEBUG_STATE
  if(state != old_state) {
    uart_send_hex_byte_spc(old_state);
    uart_send_hex_byte_crlf(state);
  }
#endif  

  // state change?
  return state != old_state;
}