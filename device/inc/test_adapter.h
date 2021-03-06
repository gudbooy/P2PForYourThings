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

#include <network_adapter.h>

#include <dbug_log.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace cm;

class TestAdapter : public NetworkAdapter {
 public:
  TestAdapter() {
    snprintf(dev_name, sizeof(dev_name), "Test Adapter");
    OPEL_DBG_LOG("Test Adapter created");
    copied = false;
  }

 private:
  bool sch;
  char buff[4096];
  unsigned lenn;
  bool copied;
  bool device_on() {
    OPEL_DBG_LOG("Devince ON");
    return true;
  }
  bool device_off() {
    OPEL_DBG_LOG("Device OFF");
    return true;
  }
  bool make_connection(void) {
    OPEL_DBG_LOG("Connected");
    sch = true;
    return true;
  }
  bool close_connection(void) {
    OPEL_DBG_LOG("Closed");
    sch = false;
    return true;
  }
  bool send(const void *buf, size_t len) {
    if (sch) {
      OPEL_DBG_LOG("Sent:%x (%u)", buf, len);
      memcpy(buff, buf, len);
      lenn = len;
      copied = true;
      usleep(100000);
    } else {
      OPEL_DBG_LOG("Failed sending", buf, len);
    }

    return sch;
  }
  bool recv(void *buf, size_t len) {
    if (sch) {
      while (true) {
        if (copied) {
          memcpy(buf, buff, (lenn < len)? lenn:len);
          copied = false;
          OPEL_DBG_LOG("Recv:%s (%u)",
                       (unsigned char*)buf + 4,
                       (lenn < len)? lenn : len);
          break;
        } else {
          usleep(50000);
        }
      }
    } else {
      OPEL_DBG_LOG("Failed recving", buf, len);
    }
    return sch;
  }
};


