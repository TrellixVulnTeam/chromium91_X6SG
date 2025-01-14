// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_ANDROID_COMPONENT_LOADER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_ANDROID_COMPONENT_LOADER_POLICY_H_

#include <jni.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/component_updater/android/component_loader_policy_forward.h"

namespace base {
class Version;
class DictionaryValue;
}  // namespace base

namespace component_updater {

// Components should use `AndroidComponentLoaderPolicy` by defining a class that
// implements the members of `ComponentLoaderPolicy`, and then registering a
// `AndroidComponentLoaderPolicy` that has been constructed with an instance of
// that class in an instance of embedded WebView or WebLayer with the Java
// AndroidComponentLoaderPolicy. The `AndroidComponentLoaderPolicy` will fetch
// the components files from the Android `ComponentsProviderService` and invoke
// the callbacks defined in this class.
//
// Ideally, the implementation of this class should share implementation with
// its component `ComponentInstallerPolicy` counterpart.
//
// Used on the UI thread, should post any non-user-visible tasks to a background
// runner.
class ComponentLoaderPolicy {
 public:
  virtual ~ComponentLoaderPolicy();

  // ComponentLoaded is called when the loader successfully gets file
  // descriptors for all files in the component from the
  // ComponentsProviderService.
  //
  // Will be called at most once. This is mutually exclusive with
  // ComponentLoadFailed; if this is called then ComponentLoadFailed won't be
  // called.
  //
  // Overriders must close all file descriptors after using them.
  //
  // `version` is the version of the component.
  // `fd_map` maps file relative paths in the install directory to its file
  //          descriptor.
  // `manifest` is the manifest for this version of the component.
  virtual void ComponentLoaded(
      const base::Version& version,
      const base::flat_map<std::string, int>& fd_map,
      std::unique_ptr<base::DictionaryValue> manifest) = 0;

  // Called if connection to the service fails, components files are not found
  // or if the manifest file is missing or invalid.
  //
  // Will be called at most once. This is mutually exclusive with
  // ComponentLoaded; if this is called then ComponentLoaded won't be called.
  //
  // TODO(crbug.com/1180966) accept error code for different types of errors.
  virtual void ComponentLoadFailed() = 0;

  // Returns the component's SHA2 hash as raw bytes, the hash value is used as
  // the unique id of the component and will be used to request components files
  // from the ComponentsProviderService.
  virtual void GetHash(std::vector<uint8_t>* hash) const = 0;
};

// Provides a bridge from Java to native to receive callbacks from the Java
// loader and pass it to the wrapped ComponentLoaderPolicy instance.
//
// The object is single use only, it will be deleted when ComponentLoaded or
// ComponentLoadedFailed is called once.
//
// Called on the UI thread, should post any non-user-visible tasks to a
// background runner.
class AndroidComponentLoaderPolicy {
 public:
  explicit AndroidComponentLoaderPolicy(
      std::unique_ptr<ComponentLoaderPolicy> loader_policy);
  ~AndroidComponentLoaderPolicy();

  AndroidComponentLoaderPolicy(const AndroidComponentLoaderPolicy&) = delete;
  AndroidComponentLoaderPolicy& operator=(const AndroidComponentLoaderPolicy&) =
      delete;

  // A utility method that returns an array of Java objects of
  // `org.chromium.components.component_updater.ComponentLoaderPolicy`.
  static base::android::ScopedJavaLocalRef<jobjectArray>
  ToJavaArrayOfAndroidComponentLoaderPolicy(
      JNIEnv* env,
      ComponentLoaderPolicyVector policies);

  // JNI overrides:
  void ComponentLoaded(JNIEnv* env,
                       const base::android::JavaRef<jobjectArray>& jfile_names,
                       const base::android::JavaRef<jintArray>& jfds);
  void ComponentLoadFailed(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jstring> GetComponentId(JNIEnv* env);

 private:
  // Returns a Java object of
  // `org.chromium.components.component_updater.ComponentLoaderPolicy`.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  void NotifyNewVersion(const base::flat_map<std::string, int>& fd_map,
                        std::unique_ptr<base::DictionaryValue> manifest);
  void CloseFdsAndFail(const base::flat_map<std::string, int>& fd_map);

  SEQUENCE_CHECKER(sequence_checker_);

  // A Java object of
  // `org.chromium.components.component_updater.ComponentLoaderPolicy`.
  base::android::ScopedJavaGlobalRef<jobject> obj_;

  std::unique_ptr<ComponentLoaderPolicy> loader_policy_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_ANDROID_COMPONENT_LOADER_POLICY_H_