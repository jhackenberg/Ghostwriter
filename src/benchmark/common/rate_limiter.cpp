#include <stdexcept>
#include <thread>
#include "rembrandt/benchmark/common/rate_limiter.h"

RateLimiter::RateLimiter()
    : stored_permits_(0), stable_interval_(MicrosecondFractions(1)), next_free_ticket_(Clock::now()) {}

RateLimiter::RateLimiter(const RateLimiter &other) : RateLimiter(other, std::lock_guard<std::mutex>(other.mutex_)) {}

RateLimiter::RateLimiter(const RateLimiter &other, const std::lock_guard<std::mutex> &) :
    stored_permits_(other.stored_permits_),
    stable_interval_(other.stable_interval_),
    next_free_ticket_(other.next_free_ticket_) {}

std::unique_ptr<RateLimiter> RateLimiter::Create(double permits_per_second) {
  std::unique_ptr<RateLimiter> rate_limiter_p = std::make_unique<RateLimiter>();
  rate_limiter_p->SetRate(permits_per_second);
  return rate_limiter_p;
}

void RateLimiter::Reset() {
  stored_permits_ = 0;
  next_free_ticket_ = Clock::now();
}

void RateLimiter::Acquire() {
  Acquire(1);
}

void RateLimiter::Acquire(int permits) {
  if (permits <= 0) {
    std::runtime_error("Permits must be positive");
  }
  auto wait_time = Reserve(permits);
  std::this_thread::sleep_for(wait_time);
}

Microseconds RateLimiter::Reserve(int permits) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto now = Clock::now();
  Resync(now);
  auto wait_time = next_free_ticket_ - now;
  double stored_permits_to_spend = std::min((double) permits, stored_permits_);
  double fresh_permits = permits - stored_permits_to_spend;

  Microseconds next_free = std::chrono::duration_cast<Microseconds>(fresh_permits * stable_interval_);
  next_free_ticket_ += next_free;
  stored_permits_ -= stored_permits_to_spend;
  return std::chrono::duration_cast<Microseconds>(wait_time);
}

void RateLimiter::Resync(Timepoint now) {
  if (now > next_free_ticket_) {
    stored_permits_ = stored_permits_ + (now - next_free_ticket_) / stable_interval_;
    next_free_ticket_ = now;
  }
}

double RateLimiter::GetRate() {
  std::lock_guard<std::mutex> lock(mutex_);
  auto second = std::chrono::duration_cast<MicrosecondFractions>(std::chrono::seconds(1));
  return second / stable_interval_;
}

void RateLimiter::SetRate(double permits_per_second) {
  if (permits_per_second <= 0) {
    throw std::runtime_error("Rate must be positive");
  }
  std::lock_guard<std::mutex> lock(mutex_);
  auto second = std::chrono::duration_cast<MicrosecondFractions>(std::chrono::seconds(1));
  stable_interval_ = second / permits_per_second;
}

