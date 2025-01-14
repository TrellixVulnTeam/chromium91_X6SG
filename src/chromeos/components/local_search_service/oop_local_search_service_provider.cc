// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/oop_local_search_service_provider.h"
#include "content/public/browser/service_process_host.h"

namespace chromeos {
namespace local_search_service {

OopLocalSearchServiceProvider::OopLocalSearchServiceProvider() {
  LocalSearchServiceProvider::Set(this);
}

OopLocalSearchServiceProvider::~OopLocalSearchServiceProvider() {
  LocalSearchServiceProvider::Set(nullptr);
}

void OopLocalSearchServiceProvider::BindLocalSearchService(
    mojo::PendingReceiver<mojom::LocalSearchService> receiver) {
  content::ServiceProcessHost::Launch(
      std::move(receiver), content::ServiceProcessHost::Options()
                               .WithDisplayName("Local Search Service")
                               .Pass());
}

}  // namespace local_search_service
}  // namespace chromeos
