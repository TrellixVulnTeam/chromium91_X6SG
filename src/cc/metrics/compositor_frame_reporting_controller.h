// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_
#define CC_METRICS_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/compositor_frame_reporter.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/frame_sequence_metrics.h"

namespace viz {
struct FrameTimingDetails;
}

namespace cc {
class DroppedFrameCounter;
class UkmManager;
struct BeginMainFrameMetrics;

// This is used for managing simultaneous CompositorFrameReporter instances
// in the case that the compositor has high latency. Calling one of the
// event functions will begin recording the time of the corresponding
// phase and trace it. If the frame is eventually submitted, then the
// recorded times of each phase will be reported in UMA.
// See CompositorFrameReporter.
class CC_EXPORT CompositorFrameReportingController {
 public:
  // Used as indices for accessing CompositorFrameReporters.
  enum PipelineStage {
    kBeginImplFrame = 0,
    kBeginMainFrame,
    kCommit,
    kActivate,
    kNumPipelineStages
  };

  CompositorFrameReportingController(bool should_report_metrics,
                                     int layer_tree_host_id);
  virtual ~CompositorFrameReportingController();

  CompositorFrameReportingController(
      const CompositorFrameReportingController&) = delete;
  CompositorFrameReportingController& operator=(
      const CompositorFrameReportingController&) = delete;

  // Events to signal Beginning/Ending of phases.
  virtual void WillBeginImplFrame(const viz::BeginFrameArgs& args);
  virtual void WillBeginMainFrame(const viz::BeginFrameArgs& args);
  virtual void BeginMainFrameAborted(const viz::BeginFrameId& id);
  virtual void WillInvalidateOnImplSide();
  virtual void WillCommit();
  virtual void DidCommit();
  virtual void WillActivate();
  virtual void DidActivate();
  virtual void DidSubmitCompositorFrame(
      uint32_t frame_token,
      const viz::BeginFrameId& current_frame_id,
      const viz::BeginFrameId& last_activated_frame_id,
      EventMetricsSet events_metrics,
      bool has_missing_content);
  virtual void DidNotProduceFrame(const viz::BeginFrameId& id,
                                  FrameSkippedReason skip_reason);
  virtual void OnFinishImplFrame(const viz::BeginFrameId& id);
  virtual void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details);
  void OnStoppedRequestingBeginFrames();

  void SetBlinkBreakdown(std::unique_ptr<BeginMainFrameMetrics> details,
                         base::TimeTicks main_thread_start_time);

  void SetUkmManager(UkmManager* manager);

  void AddActiveTracker(FrameSequenceTrackerType type);
  void RemoveActiveTracker(FrameSequenceTrackerType type);
  void SetScrollingThread(FrameSequenceMetrics::ThreadType thread);

  void SetThreadAffectsSmoothness(FrameSequenceMetrics::ThreadType thread_type,
                                  bool affects_smoothness);
  bool is_main_thread_driving_smoothness() const {
    return is_main_thread_driving_smoothness_;
  }

  void set_tick_clock(const base::TickClock* tick_clock) {
    DCHECK(tick_clock);
    tick_clock_ = tick_clock;
  }

  std::unique_ptr<CompositorFrameReporter>* reporters() { return reporters_; }

  void SetDroppedFrameCounter(DroppedFrameCounter* counter) {
    dropped_frame_counter_ = counter;
  }

 protected:
  struct SubmittedCompositorFrame {
    uint32_t frame_token;
    std::unique_ptr<CompositorFrameReporter> reporter;
    SubmittedCompositorFrame();
    SubmittedCompositorFrame(uint32_t frame_token,
                             std::unique_ptr<CompositorFrameReporter> reporter);
    SubmittedCompositorFrame(SubmittedCompositorFrame&& other);
    ~SubmittedCompositorFrame();
  };
  base::TimeTicks Now() const;

  bool HasReporterAt(PipelineStage stage) const;
  bool next_activate_has_invalidation() const {
    return next_activate_has_invalidation_;
  }

 private:
  void AdvanceReporterStage(PipelineStage start, PipelineStage target);
  bool CanSubmitImplFrame(const viz::BeginFrameId& id) const;
  bool CanSubmitMainFrame(const viz::BeginFrameId& id) const;
  std::unique_ptr<CompositorFrameReporter> RestoreReporterAtBeginImpl(
      const viz::BeginFrameId& id);
  CompositorFrameReporter::SmoothThread GetSmoothThread() const;
  CompositorFrameReporter::SmoothThread GetSmoothThreadAtTime(
      base::TimeTicks timestamp) const;

  // Checks whether there are reporters containing updates from the main
  // thread, and returns a pointer to that reporter (if any). Otherwise
  // returns nullptr.
  CompositorFrameReporter* GetOutstandingUpdatesFromMain(
      const viz::BeginFrameId& id) const;

  // If the display-compositor skips over some frames (e.g. when the gpu is
  // busy, or the client is non-responsive), then it will not issue any
  // |BeginFrameArgs| for those frames. However, |CompositorFrameReporter|
  // instances should still be created for these frames. The following
  // functions accomplish this.
  void ProcessSkippedFramesIfNecessary(const viz::BeginFrameArgs& args);
  void CreateReportersForDroppedFrames(
      const viz::BeginFrameArgs& old_args,
      const viz::BeginFrameArgs& new_args) const;

  // The arg is a reference to the unique_ptr, because depending on the state
  // that reporter is in, its ownership might be pass or not.
  void SetPartialUpdateDeciderWhenWaitingOnMain(
      std::unique_ptr<CompositorFrameReporter>& reporter);

  const bool should_report_metrics_;
  const int layer_tree_host_id_;

  viz::BeginFrameId last_submitted_frame_id_;

  bool next_activate_has_invalidation_ = false;
  CompositorFrameReporter::ActiveTrackers active_trackers_;
  FrameSequenceMetrics::ThreadType scrolling_thread_ =
      FrameSequenceMetrics::ThreadType::kUnknown;

  bool is_compositor_thread_driving_smoothness_ = false;
  bool is_main_thread_driving_smoothness_ = false;
  // Sorted history of smooththread. Element i indicating the smooththread
  // from timestamp of element i-1 until timestamp of element i.
  std::map<base::TimeTicks, CompositorFrameReporter::SmoothThread>
      smooth_thread_history_;

  // The latency reporter passed to each CompositorFrameReporter. Owned here
  // because it must be common among all reporters.
  // DO NOT reorder this line and the ones below. The latency_ukm_reporter_
  // must outlive the objects in |submitted_compositor_frames_|.
  std::unique_ptr<LatencyUkmReporter> latency_ukm_reporter_;

  std::unique_ptr<CompositorFrameReporter>
      reporters_[PipelineStage::kNumPipelineStages];

  // Mapping of frame token to pipeline reporter for submitted compositor
  // frames.
  // DO NOT reorder this line and the one above. The latency_ukm_reporter_
  // must outlive the objects in |submitted_compositor_frames_|.
  base::circular_deque<SubmittedCompositorFrame> submitted_compositor_frames_;

  // The latest frame that was started.
  viz::BeginFrameArgs previous_frame_;

  const base::TickClock* tick_clock_ = base::DefaultTickClock::GetInstance();

  DroppedFrameCounter* dropped_frame_counter_ = nullptr;

  // When a frame with events metrics fails to be presented, its events metrics
  // will be added to this map. The first following presented frame will get
  // these metrics and report them.
  std::map<viz::BeginFrameId, EventMetrics::List>
      events_metrics_from_dropped_frames_;
};
}  // namespace cc

#endif  // CC_METRICS_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_
