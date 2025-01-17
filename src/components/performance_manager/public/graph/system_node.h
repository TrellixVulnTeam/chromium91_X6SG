// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "components/performance_manager/public/graph/node.h"

namespace performance_manager {

class SystemNodeObserver;

// The SystemNode represents system-wide state. There is at most one system node
// in a graph.
class SystemNode : public Node {
 public:
  using Observer = SystemNodeObserver;
  using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;
  class ObserverDefaultImpl;

  SystemNode();
  ~SystemNode() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemNode);
};

// Pure virtual observer interface. Derive from this if you want to be forced to
// implement the entire interface.
class SystemNodeObserver {
 public:
  SystemNodeObserver();
  virtual ~SystemNodeObserver();

  // Node lifetime notifications.

  // Called when the |system_node| is added to the graph. Observers must not
  // make any property changes or cause re-entrant notifications during the
  // scope of this call. Instead, make property changes via a separate posted
  // task.
  virtual void OnSystemNodeAdded(const SystemNode* system_node) = 0;

  // Called before the |system_node| is removed from the graph. Observers must
  // not make any property changes or cause re-entrant notifications during the
  // scope of this call.
  virtual void OnBeforeSystemNodeRemoved(const SystemNode* system_node) = 0;

  // Called when a new set of process memory metrics is available.
  virtual void OnProcessMemoryMetricsAvailable(
      const SystemNode* system_node) = 0;

  // Called before OnMemoryPressure(). This can be used to track state before
  // memory start being released in response to memory pressure.
  //
  // Note: This is guaranteed to be invoked before OnMemoryPressure(), but
  // will not necessarily be called before base::MemoryPressureListeners
  // are notified.
  virtual void OnBeforeMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel new_level) = 0;

  // Called when the system is under memory pressure. Observers may start
  // releasing memory in response to memory pressure.
  //
  // NOTE: This isn't called for a transition to the MEMORY_PRESSURE_LEVEL_NONE
  // level. For this reason there's no corresponding property in this node and
  // the response to these notifications should be stateless.
  virtual void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel new_level) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemNodeObserver);
};

// Default implementation of observer that provides dummy versions of each
// function. Derive from this if you only need to implement a few of the
// functions.
class SystemNode::ObserverDefaultImpl : public SystemNodeObserver {
 public:
  ObserverDefaultImpl();
  ~ObserverDefaultImpl() override;

  // SystemNodeObserver implementation:
  void OnSystemNodeAdded(const SystemNode* system_node) override {}
  void OnBeforeSystemNodeRemoved(const SystemNode* system_node) override {}
  void OnProcessMemoryMetricsAvailable(const SystemNode* system_node) override {
  }
  void OnBeforeMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel new_level) override {}
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel new_level) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ObserverDefaultImpl);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_
