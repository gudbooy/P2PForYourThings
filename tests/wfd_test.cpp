/* Copyright 2015-2016 CISS, and contributors. All rights reserved
 * 
 * Contact: Eunsoo Park <esevan.park@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0(the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <wifi_control.h>

#include <stdio.h>

int main() {
  int ret;
  printf("Wifi_direct_server_up\n");
  ret = wifi::wifi_direct_server_up();
  printf("Return: %d\n", ret);

  char buf[256];
  printf("Wifi_direct_server_reset\n");
  ret = wifi::wifi_direct_server_reset(buf, 256);
  printf("Return: %d-%s\n", ret, buf);
  
  printf("Wifi_get_p2p_device_addr\n");
  ret = wifi::wifi_get_p2p_device_addr(buf, 256);
  printf("Return: %d-%s\n", ret, buf);

  printf("Wifi_direct_server_down\n");
  //ret = wifi::wifi_direct_server_down();
  printf("Return: %d\n", ret);

  return 0;
}
