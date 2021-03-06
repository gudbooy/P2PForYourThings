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

#include <segment_manager.h>
#include <dbug_log.h>
#include <protocol_manager.h>

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include <list>

namespace cm {
static SegmentManager *sm = NULL;

SegmentManager::SegmentManager(void) {
  queue_size[kSegSend] = 0;
  queue_size[kSegRecv] = 0;
  next_seq_no[kSegSend] = 0;
  next_seq_no[kSegRecv] = 0;
  free_list_size = 0;
  seq_no = 0;
}

SegmentManager *SegmentManager::get_instance(void) {
  if (sm == NULL)
    sm = new SegmentManager();
  return sm;
}

uint16_t SegmentManager::get_seq_no(uint16_t len) {
  uint16_t res;
  seq_no_lock.lock();
  res = seq_no;
  seq_no += len;
  seq_no_lock.unlock();
  return res;
}

void SegmentManager::serialize_segment_header(Segment *seg) {
  uint16_t net_seq_no = htons(seg -> seq_no);
  uint16_t net_flag_len = htons(seg -> flag_len);

  memcpy(seg -> data, &net_seq_no, sizeof(uint16_t));
  memcpy(seg -> data+2, &net_flag_len, sizeof(uint16_t));
}
int SegmentManager::send_to_segment_manager(uint8_t *data, size_t len) {
  assert(data != NULL && len > 0);


  uint32_t offset = 0;
  uint16_t num_of_segments =(uint16_t)((len + kSegSize - 1) / kSegSize);
  assert((len + kSegSize - 1) / kSegSize < UINT16_MAX);
  /* Reserve sequence numbers to this thread */
  uint16_t allocated_seq_no = get_seq_no(num_of_segments);
  int seg_idx;
  for (seg_idx = 0; seg_idx < num_of_segments; seg_idx ++) {
    uint16_t seg_len =(len - offset < kSegSize)? len - offset : kSegSize;
    Segment *seg = get_free_segment();

    /* Set segment length */
    mSetSegLenBits(seg_len, seg -> flag_len);

    /* Set segment sequence number */
    seg -> seq_no = allocated_seq_no++;

    /* Set segment data */
    memcpy(&(seg -> data[kSegHeaderSize]), data + offset, seg_len);
    offset += seg_len;

    /* Set segment MF flag */
    if (offset < len) mSetSegFlagBits(kSegFlagMF, seg -> flag_len);

    /* Set segment header to data */
    serialize_segment_header(seg);
    
    enqueue(kSegSend, seg);
  }

  return 0;
}

uint8_t *SegmentManager::recv_from_segment_manager(void *proc_data_handle) {
  assert(proc_data_handle != NULL);

  ProtocolData *pd = reinterpret_cast<ProtocolData *>(proc_data_handle);
  uint8_t *serialized = NULL;
  uint16_t offset = 0;
  size_t data_size = 0;
  bool cont = false;

  Segment *seg = dequeue(kSegRecv);
  ProtocolManager::parse_header(&(seg -> data[kSegHeaderSize]), pd);

  if (unlikely(pd -> len == 0))
    return NULL;

  serialized = reinterpret_cast<uint8_t *>(calloc(pd -> len, sizeof(uint8_t)));

  data_size = mGetSegLenBits(seg->flag_len) - kProtHeaderSize;
  memcpy(serialized + offset,
         &(seg->data[kSegHeaderSize]) + kProtHeaderSize,
         data_size);
  offset += data_size;

  cont = (mGetSegFlagBits(seg -> flag_len) == kSegFlagMF);

  free_segment(seg);

  while (cont) {
    seg = dequeue(kSegRecv);
    data_size = mGetSegLenBits(seg->flag_len);
    memcpy(serialized + offset,
           &(seg -> data[kSegHeaderSize]),
           data_size);
    cont =(mGetSegFlagBits(seg -> flag_len) == kSegFlagMF);
    offset += data_size;
    free_segment(seg);
  }
  return serialized;
}

void SegmentManager::enqueue(SegQueueType type, Segment *seg) {
  std::unique_lock<std::mutex> lck(lock[type]);
  bool segment_enqueued = false;

  if (seg -> seq_no == next_seq_no[type]) {
    next_seq_no[type]++;
    queue[type].push_back(seg);
    queue_size[type]++;
    segment_enqueued = true;
  } else {
    assert(seg -> seq_no > next_seq_no[type]);
    std::list<Segment *>::iterator curr_it = pending_queue[type].begin();

    /* First, we put a requested segment into pending queue */
    while (curr_it != pending_queue[type].end()) {
      Segment *walker = *curr_it;
      assert(walker -> seq_no != seg -> seq_no);

      if (walker -> seq_no > seg -> seq_no)
        break;

      curr_it++;
    }
    pending_queue[type].insert(curr_it, seg);

    /* Finally, we put all consequent segments into type queue */
    curr_it = pending_queue[type].begin();
    while (curr_it != pending_queue[type].end() &&
           (*curr_it) -> seq_no == next_seq_no[type]) {
      next_seq_no[type]++;
      queue[type].push_back(*curr_it);
      queue_size[type]++;
      segment_enqueued = true;

      std::list<Segment *>::iterator to_erase = curr_it++;
      pending_queue[type].erase(to_erase);
    }

  }

  if (queue_size[type] % 10 == 0)
    OPEL_DBG_WARN("[%d]Queue size:%d", type, queue_size[type]);

  if (segment_enqueued) not_empty[type].notify_all();
}

Segment *SegmentManager::dequeue(SegQueueType type) {
  std::unique_lock<std::mutex> lck(lock[type]);
  while (queue_size[type] == 0) {
    not_empty[type].wait(lck);
  }

  Segment *ret = queue[type].front();
  queue[type].pop_front();
  queue_size[type]--;
  return ret;
}

Segment *SegmentManager::get_free_segment(void) {
  std::unique_lock<std::mutex> lck(free_list_lock);
  Segment *ret = NULL;
  if (free_list_size == 0) {
    ret = reinterpret_cast<Segment *>(calloc(1, sizeof(Segment)));
  } else {
    ret = free_list.front();
    free_list.pop_front();
    free_list_size--;
  }

  ret->seq_no = 0;
  ret->flag_len = 0;
  assert(ret != NULL);
  return ret;
}

void SegmentManager::release_segment_from_free_list(uint32_t threshold) {
  while (free_list_size > threshold) {
    Segment *to_free = free_list.front();
    free_list.pop_front();
    free(to_free);
    free_list_size--;
  }
}
void SegmentManager::free_segment(Segment *seg) {
  std::unique_lock<std::mutex> lck(free_list_lock);
  free_list.push_front(seg);
  free_list_size++;

  if (unlikely(free_list_size > kSegFreeThreshold)) {
    release_segment_from_free_list(kSegFreeThreshold / 2);
  }
}

void SegmentManager::free_segment_all(void) {
  std::unique_lock<std::mutex> lck(free_list_lock);
  release_segment_from_free_list(0);
}

void SegmentManager::failed_sending(Segment *seg) {
  std::unique_lock<std::mutex> lck(failed_lock);
  failed.push_back(seg);
}

Segment *SegmentManager::get_failed_sending(void) {
  std::unique_lock<std::mutex> lck(failed_lock);
  if (failed.size() == 0) return NULL;

  Segment *res = failed.front();
  failed.pop_front();

  return res;
}

}; /* namespace cm */
