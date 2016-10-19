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

#ifndef DEVICE_INC_TCP_SERVER_OVER_WFD_H_
#define DEVICE_INC_TCP_SERVER_OVER_WFD_H_

#include <network_adapter.h>

#include <dbug_log.h>

namespace cm {
class TCPServerOverWfdAdapter : public NetworkAdapter {
 public:
  TCPServerOverWfdAdapter(int port);

 private:
  int port;
  bool device_on();
  bool device_off();
  bool make_connection();
  bool close_connection();
  bool send();
  bool recv();
};
}  /* namespace cm */

#endif  // DEVICE_INC_TCP_SERVER_OVER_WFD_H_
