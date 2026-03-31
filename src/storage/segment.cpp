#include "../../include/rembrandt/storage/segment.h"

#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>

Segment::Segment(void *location, uint64_t segment_size) {
  segment_header_ = reinterpret_cast<SegmentHeader *>(location);
  Segment::ResetHeader(*segment_header_);
}

Segment::Segment(SegmentHeader *segment_header) {
  segment_header_ = segment_header;
}

Segment::Segment(Segment &&other) noexcept : segment_header_(other.segment_header_) {
  other.segment_header_ = nullptr;
}

Segment &Segment::operator=(Segment &&other) noexcept {
  std::swap(segment_header_, other.segment_header_);
  return *this;
}

bool Segment::Allocate(uint64_t topic_id, uint32_t partition_id, uint32_t segment_id, uint64_t start_offset) {
  if (!IsFree() || topic_id >= FREE_BIT) {
    return false;
  }
  segment_header_->topic_id_ = topic_id & ~FREE_BIT;
  segment_header_->partition_id_ = partition_id;
  segment_header_->segment_id_ = segment_id;
  segment_header_->start_offset_ = start_offset;
  segment_header_->write_offset_ = start_offset | WRITEABLE_BIT;
  segment_header_->commit_offset_ = start_offset;

  return true;
}

bool Segment::Free() {
  if (IsFree()) {
    return false;
  }
  Segment::ResetHeader(*segment_header_);
  return true;
}

bool Segment::IsFree() {
  return (segment_header_->topic_id_ & FREE_BIT) != 0;
}

void *Segment::GetMemoryLocation() {
  return static_cast<void *>(segment_header_);
}

uint64_t Segment::GetTopicId() {
  return segment_header_->topic_id_ & ~FREE_BIT;
}

uint32_t Segment::GetPartitionId() {
  return segment_header_->partition_id_;
}

uint32_t Segment::GetSegmentId() {
  return segment_header_->segment_id_;
}

uint64_t Segment::GetDataOffset() {
  return sizeof(SegmentHeader);
}

uint64_t Segment::GetOffsetOfStartOffset() {
  return offsetof(SegmentHeader, start_offset_);
}

uint64_t Segment::GetOffsetOfCommitOffset() {
  return offsetof(SegmentHeader, commit_offset_);
}

uint64_t Segment::GetOffsetOfWriteOffset() {
  return offsetof(SegmentHeader, write_offset_);
}

uint64_t Segment::GetCommitOffset() {
  return segment_header_->commit_offset_;
}

void Segment::SetCommitOffset(uint64_t commit_offset) {
  segment_header_->commit_offset_ = commit_offset;
}

void Segment::ResetHeader(SegmentHeader &segment_header) {
  segment_header.topic_id_ = FREE_BIT;
  segment_header.partition_id_ = 0;
  segment_header.segment_id_ = 0;
  segment_header.start_offset_ = 0;
  segment_header.write_offset_ = 0 | Segment::WRITEABLE_BIT;
  segment_header.commit_offset_ = 0;
}