// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_MUTING_SESSION_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_MUTING_SESSION_H_

#include <utility>

#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {

class CONTENT_EXPORT AudioMutingSession {
 public:
  explicit AudioMutingSession(const base::UnguessableToken& group_id);
  ~AudioMutingSession();

  void Connect(media::mojom::AudioStreamFactory* factory);

 private:
  const base::UnguessableToken group_id_;
  mojo::AssociatedRemote<media::mojom::LocalMuter> muter_;

  DISALLOW_COPY_AND_ASSIGN(AudioMutingSession);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_MUTING_SESSION_H_
