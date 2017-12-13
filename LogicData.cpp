#include "Arduino.h"
#include "LogicData.h"

// Expect 1 bit per millisecond
#define SAMPLE_RATE 1000

// Push to head; pop from tail

index_t mque::next(index_t x)
{
  return ( x + 1 ) % Q_MAX;
}

bool mque::empty() {
  return tail == head;
}

bool mque::full() {
  return next(tail) == head;
}

index_t mque::size()
{
  return (Q_MAX + head - tail) % Q_MAX;
}

// destructive push; pushes even if full
bool mque::push(micros_t t)
{
  // NOTE: Caller should have disabled interrupts
  trace[head] = t;
  head = next(head);
  if (tail == head) {
    tail = next(tail);
  }
}

// destructive pop; no-op if empty
bool mque::pop(micros_t * t)
{
  noInterrupts();
  if (empty()) return false;
  *t = trace[tail];
  tail = next(tail);
  interrupts();
  return true;
}

// drop elements from the tail of the queue; no range-checking!
void mque::drop(index_t n)
{
  noInterrupts();
  tail += n;
  tail %= Q_MAX;  // TODO: Check if this is expensive
  interrupts();
}

// non-destructive indexed peek
bool mque::peek(index_t index, micros_t * t)
{
  if (index >= size()) return false;
  noInterrupts();
  index += tail;
  index %= Q_MAX;
  *t = trace[index];
  interrupts();
  return true;
}

//------------------------------------------------------

void LogicData::Begin() {
  pinMode(tx_pin, OUTPUT);
  SendBit(MARK); // IDLE-CLOSED
}

void LogicData::PinChange(bool level) {
  if (level != prev_level) {
    prev_level = level;
    micros_t now = micros();
    if (pin_idle) {
      q.push(BIG_IDLE);
      pin_idle = false;
    } else {
      q.push(now-prev_bit);
    }
    prev_bit = now;
  }
}

void LogicData::Service() {
  micros_t idle_time = micros() - prev_bit;
  if (!pin_idle && idle_time >= IDLE_TIME) {
    noInterrupts();
    if (micros() - prev_bit >= IDLE_TIME) {
      pin_idle = true;
    }
    interrupts();
  }
}

uint32_t LogicData::ReadTrace() {
  index_t fini;

  noInterrupts();
  fini=q.tail;
  bool level = ((q.size() & 1)==0) ^ !prev_level;
  interrupts();

  micros_t t;
  index_t i=0;

  //-- Find start-bit (idle)
  for (; q.peek(i, &t); i++) {
    if (level && t > 40 * SAMPLE_RATE) break;
    level = !level;
  }

  // HACK: show stream of us-times  :HACK
  if (q.pop(&t)) {
    noInterrupts();
    t = micros_t(0x12340000) + micros_t(q.head) * 256 + q.tail;
    // Huh: this is weird. head and tail always seem to increment by 2. at least until we go empty.
    //  Wait -- no we don't.  Looks like my read timing is wrong on stream.py.
    interrupts();
    return t;

    if (t>0x3fffffff) t=0x3fffffff;
    t|= 0x40000000;
//    if (level) t|= 0x10000000;
    return t;
  }
  return 0;
  
  //-- Sample signals at mid-point of data rate
  uint32_t mask = 1<<31;
  uint32_t acc = 0;
  acc = 0;
  micros_t t_meas = SAMPLE_RATE/2;
  for (t=0; mask; mask >>= 1) {
    if (t_meas < SAMPLE_RATE) {
      if (!q.peek(++i, &t)) break;
      level = !level;
      t_meas += t;
    }
    acc += !level ? mask : 0;
    t_meas -= SAMPLE_RATE;
  }

  // ran out of signal before we got whole word
  if (mask) return 0;

  // We decoded a word and it consumed i samples
  noInterrupts();
  if (fini == q.tail) {
    q.drop(i-1);
  } else {
    // race fail; return 0 and let the caller try again later
    acc = 0;
  }
  interrupts();

  return acc;
}

// Transmit
void LogicData::SendBit(bool bit) {
  digitalWrite(tx_pin, bit);
}

void LogicData::MicroDelay(micros_t us) {
  // spin-wait until timer advances
  while (true) {
    Service();
    micros_t now = micros();
    now -= timer;
    if (now >= us) break;
    // TODO: if (t-now > 50) usleep(t-now-50);
  }
  timer += us;
}

void LogicData::Delay(uint16_t ms) {
  MicroDelay(micros_t(ms)*1000);
}

void LogicData::SendBit(bool bit, uint16_t ms) {
  SendBit(bit);
  Delay(ms);
}

void LogicData::Space(uint16_t ms) {
  SendBit(SPACE, ms);
}

void LogicData::SendStartBit() {
  Space(LOGICDATA_MIN_START_BIT);
}

void LogicData::LogicData::Stop() {
  SendBit(MARK); // IDLE-CLOSED
}

void LogicData::Send(uint32_t data) {
  SendStartBit();

  for (uint32_t i = 0x80000000 ; i; i/=2) {
    SendBit((i&data)?SPACE:MARK, 1);
  }

  SendBit(SPACE); // IDLE-OPEN
}

void LogicData::OpenChannel() {
  if (active) return;
  active = true;
  start = timer = micros();
  SendBit(SPACE);
}

void LogicData::CloseChannel() {
  // Send a SPACE at least long enough so our command-channel was open for 500ms, or
  // just start-bit length if we've already been open that long.

  if (!active) return;


  // Workaround for stuck timers
  Stop();
  return;

  // BUG: Following timers never expire!
  micros_t delta = timer-start;
  if (delta/1000 + LOGICDATA_MIN_START_BIT < LOGICDATA_MIN_WINDOW_MS) {
    timer=start;
    Space(LOGICDATA_MIN_WINDOW_MS);
  } else {
    Space(LOGICDATA_MIN_START_BIT);
  }
  Stop();

  active = false;
}

void LogicData::Send(uint32_t * data, unsigned count) {
  if (!count) return;

  OpenChannel();
  for (unsigned i = 0; i != count; i++) {
    Send(data[i]);
  }
  CloseChannel();
}

//
// LOGICDATA protocol
//////////////////////////////////////////////////////////
