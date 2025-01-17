// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_CHROMEOS_H_
#define PRINTING_PRINTING_CONTEXT_CHROMEOS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/cups_deleters.h"
#include "printing/backend/cups_printer.h"
#include "printing/printing_context.h"

namespace printing {

class PRINTING_EXPORT PrintingContextChromeos : public PrintingContext {
 public:
  static std::unique_ptr<PrintingContextChromeos> CreateForTesting(
      Delegate* delegate,
      std::unique_ptr<CupsConnection> connection);

  explicit PrintingContextChromeos(Delegate* delegate);
  PrintingContextChromeos(const PrintingContextChromeos&) = delete;
  PrintingContextChromeos& operator=(const PrintingContextChromeos&) = delete;
  ~PrintingContextChromeos() override;

  // PrintingContext implementation.
  void AskUserForSettings(int max_pages,
                          bool has_selection,
                          bool is_scripted,
                          PrintSettingsCallback callback) override;
  Result UseDefaultSettings() override;
  gfx::Size GetPdfPaperSizeDeviceUnits() override;
  Result UpdatePrinterSettings(bool external_preview,
                               bool show_system_dialog,
                               int page_count) override;
  Result NewDocument(const std::u16string& document_name) override;
  Result NewPage() override;
  Result PageDone() override;
  Result DocumentDone() override;
  void Cancel() override;
  void ReleaseContext() override;
  printing::NativeDrawingContext context() const override;

  Result StreamData(const std::vector<char>& buffer);

 private:
  // For testing. Use CreateForTesting() to create.
  PrintingContextChromeos(Delegate* delegate,
                          std::unique_ptr<CupsConnection> connection);

  // Lazily initializes |printer_|.
  Result InitializeDevice(const std::string& device);

  const std::unique_ptr<CupsConnection> connection_;
  std::unique_ptr<CupsPrinter> printer_;
  std::vector<ScopedCupsOption> cups_options_;
  bool send_user_info_ = false;
  std::string username_;
};

// This has the side effect of recording UMA for advanced attributes usage,
// so only call once per job.
PRINTING_EXPORT std::vector<ScopedCupsOption> SettingsToCupsOptions(
    const PrintSettings& settings);

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_CHROMEOS_H_
