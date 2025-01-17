// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_METADATA_MODEL_H_
#define ASH_PROJECTOR_PROJECTOR_METADATA_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace base {
class Value;
}  // namespace base

namespace ash {

// Base class to describe a metadata item.
class MetadataItem {
 public:
  explicit MetadataItem(const base::TimeDelta start_time,
                        const base::TimeDelta end_time,
                        const std::string& text);
  MetadataItem(const MetadataItem&) = delete;
  MetadataItem& operator=(const MetadataItem&) = delete;
  virtual ~MetadataItem();

  base::TimeDelta& start_time() { return start_time_; }

  base::TimeDelta& end_time() { return end_time_; }

  // Return the serialized metadata item. This is used for storage.
  virtual base::Value ToJson() = 0;

 protected:
  // The start time of the metadata item from the start of the recording
  // session.
  base::TimeDelta start_time_;
  // The end time of the metadata item from the start of the recording session.
  base::TimeDelta end_time_;
  // Text data of the metadata item.
  std::string text_;
};

// Class to describe a key idea.
class ASH_EXPORT ProjectorKeyIdea : public MetadataItem {
 public:
  explicit ProjectorKeyIdea(const base::TimeDelta start_time,
                            const base::TimeDelta end_time,
                            const std::string& text = std::string());
  ProjectorKeyIdea(const ProjectorKeyIdea&) = delete;
  ProjectorKeyIdea& operator=(const ProjectorKeyIdea&) = delete;
  ~ProjectorKeyIdea() override;

  base::Value ToJson() override;
};

// Class to describe a transcription.
class ASH_EXPORT ProjectorTranscript : public MetadataItem {
 public:
  explicit ProjectorTranscript(
      const base::TimeDelta start_time,
      const base::TimeDelta end_time,
      const std::string& text,
      const std::vector<base::TimeDelta>& word_alignments);
  ProjectorTranscript(const ProjectorTranscript&) = delete;
  ProjectorTranscript& operator=(const ProjectorTranscript&) = delete;
  ~ProjectorTranscript() override;

  base::Value ToJson() override;

 private:
  std::vector<base::TimeDelta> word_alignments_;
};

// Class to describe a projector metadata of a screencast session, including
// name, transcriptions, key_ideas, etc
class ASH_EXPORT ProjectorMetadata {
 public:
  explicit ProjectorMetadata();
  ProjectorMetadata(const ProjectorMetadata&) = delete;
  ProjectorMetadata& operator=(const ProjectorMetadata&) = delete;
  ~ProjectorMetadata();

  // Adds the transcript to the metadata. Virtual for testing.
  void AddTranscript(std::unique_ptr<ProjectorTranscript> transcript);
  // Marks a beginning of a key idea. The timing info of the next transcript
  // will be used as the timing of the key idea. Virtual for testing.
  void MarkKeyIdea();
  // Sets the name of the screencast. Virtual for testing.
  void SetName(const std::string& name);
  // Serializes the metadata for storage. Virtual for testing.
  std::string Serialize();

 private:
  base::Value ToJson();

  std::vector<std::unique_ptr<ProjectorTranscript>> transcripts_;
  std::vector<std::unique_ptr<ProjectorKeyIdea>> key_ideas_;
  std::string name_;

  // True if user mark the transcript as a key idea. It will be reset to false
  // when the final recognition result is received and recorded as a key idea.
  bool should_mark_key_idea_ = false;
};

}  // namespace ash
#endif  // ASH_PROJECTOR_PROJECTOR_METADATA_MODEL_H_
