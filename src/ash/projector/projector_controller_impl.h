// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_CONTROLLER_IMPL_H_
#define ASH_PROJECTOR_PROJECTOR_CONTROLLER_IMPL_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/projector/model/projector_session_impl.h"
#include "ash/public/cpp/projector/projector_controller.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

class ProjectorClient;
class ProjectorUiController;
class ProjectorMetadataController;

// A controller to handle projector functionalities.
class ASH_EXPORT ProjectorControllerImpl : public ProjectorController {
 public:
  ProjectorControllerImpl();
  ProjectorControllerImpl(const ProjectorControllerImpl&) = delete;
  ProjectorControllerImpl& operator=(const ProjectorControllerImpl&) = delete;
  ~ProjectorControllerImpl() override;

  // ProjectorController:
  void SetClient(ash::ProjectorClient* client) override;
  void OnSpeechRecognitionAvailable(bool available) override;
  void OnTranscription(const std::u16string& text,
                       base::TimeDelta audio_start_time,
                       base::TimeDelta audio_end_time,
                       const std::vector<base::TimeDelta>& word_offsets,
                       bool is_final) override;
  void SetProjectorToolsVisible(bool is_visible) override;
  void StartProjectorSession(SourceType scope, aura::Window* window) override;
  bool IsEligible() const override;
  // Set caption on/off state.
  void SetCaptionState(bool is_on);
  // Mark a key idea.
  void MarkKeyIdea();

  // TODO(crbug.com/1165435): Consider updating to be delegate of recording
  // service after finalizing on the integration plan with recording mode.
  // Invoked when recording is started to start a screencast session.
  void OnRecordingStarted();

  // Saves the screencast including metadata.
  void SaveScreencast(const base::FilePath& saved_video_path);

  // Invoked when laser pointer button is pressed.
  void OnLaserPointerPressed();
  // Invoked when marker button is pressed.
  void OnMarkerPressed();

  void SetProjectorUiControllerForTest(
      std::unique_ptr<ProjectorUiController> ui_controller);
  void SetProjectorMetadataControllerForTest(
      std::unique_ptr<ProjectorMetadataController> metadata_controller);

  ProjectorUiController* ui_controller() { return ui_controller_.get(); }
  ProjectorSessionImpl* projector_session() { return projector_session_.get(); }

 private:
  // Starts or stops the speech recognition session.
  void StartSpeechRecognition();
  void StopSpeechRecognition();

  ProjectorClient* client_ = nullptr;
  std::unique_ptr<ProjectorSessionImpl> projector_session_;
  std::unique_ptr<ProjectorUiController> ui_controller_;
  std::unique_ptr<ProjectorMetadataController> metadata_controller_;

  // Whether the caption bubble ui is being shown or not.
  bool is_caption_on_ = false;

  // Whether SODA is available on the device.
  bool is_speech_recognition_available_ = false;

  // Whether speech recognition is taking place or not.
  bool is_speech_recognition_on_ = false;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_CONTROLLER_IMPL_H_
