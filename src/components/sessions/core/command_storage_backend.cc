// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_backend.h"

#include <stdint.h>
#include <algorithm>
#include <limits>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_service_commands.h"
#include "crypto/aead.h"

namespace sessions {

using SessionType = CommandStorageManager::SessionType;

namespace {

// File version number.
constexpr int32_t kFileVersion1 = 1;
constexpr int32_t kEncryptedFileVersion = 2;
// The versions that are used if `use_marker` is true.
constexpr int32_t kFileVersionWithMarker = 3;
constexpr int32_t kEncryptedFileVersionWithMarker = 4;

// The signature at the beginning of the file = SSNS (Sessions).
constexpr int32_t kFileSignature = 0x53534E53;

// Length (in bytes) of the nonce (used when encrypting).
constexpr int kNonceLength = 12;

// The file header is the first bytes written to the file,
// and is used to identify the file as one written by us.
struct FileHeader {
  int32_t signature;
  int32_t version;
};

// See CommandStorageBackend for details.
const SessionCommand::id_type kInitialStateMarkerCommandId = 255;

// SessionFileReader ----------------------------------------------------------

// SessionFileReader is responsible for reading the set of SessionCommands that
// describe a Session back from a file. SessionFileRead does minimal error
// checking on the file (pretty much only that the header is valid).

class SessionFileReader {
 public:
  typedef sessions::SessionCommand::id_type id_type;
  typedef sessions::SessionCommand::size_type size_type;

  // Returns true if the header is valid. If false, the file does not contain
  // a valid sessions file.
  static bool IsHeaderValid(const base::FilePath& path,
                            const std::vector<uint8_t>& crypto_key) {
    SessionFileReader reader(path, crypto_key);
    return reader.IsHeaderValid();
  }

  struct MarkerStatus {
    // True if the file was written with a version that supports the marker.
    bool supports_marker = false;

    // If true, the file was written with a version that supports the marker
    // *and* the file has a marker. If `supports_marker` is true and this is
    // false, it means the initial state was not correctly written, and this
    // file should not be used.
    bool has_marker = false;
  };

  static MarkerStatus GetMarkerStatus(const base::FilePath& path,
                                      const std::vector<uint8_t>& crypto_key) {
    SessionFileReader reader(path, crypto_key);
    MarkerStatus status;
    status.supports_marker = reader.SupportsMarker();
    if (status.supports_marker)
      status.has_marker = reader.ReadToMarker();
    return status;
  }

  // Reads the state of commands from the specified file.
  static CommandStorageBackend::ReadCommandsResult Read(
      const base::FilePath& path,
      const std::vector<uint8_t>& crypto_key) {
    SessionFileReader reader(path, crypto_key);
    return reader.Read();
  }

 private:
  struct ReadResult {
    std::unique_ptr<sessions::SessionCommand> command;
    bool error_reading = false;
  };

  SessionFileReader(const base::FilePath& path,
                    const std::vector<uint8_t>& crypto_key)
      : buffer_(CommandStorageBackend::kFileReadBufferSize, 0),
        crypto_key_(crypto_key) {
    if (!crypto_key.empty()) {
      aead_ = std::make_unique<crypto::Aead>(crypto::Aead::AES_256_GCM);
      aead_->Init(base::make_span(crypto_key_));
    }
    file_ = std::make_unique<base::File>(
        path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    is_header_valid_ = ReadHeader();
  }

  // Returns true if the file has a valid header.
  bool IsHeaderValid() const { return is_header_valid_; }

  // Reads the contents of the file specified in the constructor.
  CommandStorageBackend::ReadCommandsResult Read();

  bool SupportsMarker() const {
    return IsHeaderValid() && (version_ == kFileVersionWithMarker ||
                               version_ == kEncryptedFileVersionWithMarker);
  }

  // Parses the header.
  bool ReadHeader();

  // Reads commands until the marker is found, or no more commands.
  bool ReadToMarker();

  // Reads a single command. If the command returned in the structure is null,
  // there are no more commands.
  ReadResult ReadCommand();

  // Decrypts a previously encrypted command. Returns the new command on
  // success.
  std::unique_ptr<sessions::SessionCommand> CreateCommandFromEncrypted(
      const char* data,
      size_type length);

  // Creates a command from the previously written value.
  std::unique_ptr<sessions::SessionCommand> CreateCommand(const char* data,
                                                          size_type length);

  // Shifts the unused portion of buffer_ to the beginning and fills the
  // remaining portion with data from the file. Returns false if the buffer
  // couldn't be filled or there was an error reading the file.
  bool FillBuffer();

  bool is_header_valid_ = false;

  // As we read from the file, data goes here.
  std::string buffer_;

  const std::vector<uint8_t> crypto_key_;

  std::unique_ptr<crypto::Aead> aead_;

  // The file.
  std::unique_ptr<base::File> file_;

  // Position in buffer_ of the data.
  size_t buffer_position_ = 0;

  // Number of available bytes; relative to buffer_position_.
  size_t available_count_ = 0;

  // Count of the number of commands encountered.
  int command_counter_ = 0;

  bool did_check_header_ = false;

  // The version the file was written with. Should only be used if
  // IsHeaderValid() returns true.
  int32_t version_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SessionFileReader);
};

CommandStorageBackend::ReadCommandsResult SessionFileReader::Read() {
  if (!IsHeaderValid())
    return {};

  CommandStorageBackend::ReadCommandsResult commands_result;
  // Even if there was an error the commands are returned. The hope is at least
  // some portion of the previous session is restored.
  ReadResult result = ReadCommand();
  for (; result.command; result = ReadCommand()) {
    if (result.command->id() != kInitialStateMarkerCommandId)
      commands_result.commands.push_back(std::move(result.command));
  }
  // `error_reading` is only set if `command` is null.
  commands_result.error_reading = result.error_reading;
  return commands_result;
}

bool SessionFileReader::ReadHeader() {
  // This function advances |file| and should only be called once.
  DCHECK(!did_check_header_);
  did_check_header_ = true;

  if (!file_->IsValid())
    return false;
  FileHeader header;
  const int read_count =
      file_->ReadAtCurrentPos(reinterpret_cast<char*>(&header), sizeof(header));
  if (read_count != sizeof(header) || header.signature != kFileSignature)
    return false;
  version_ = header.version;
  const bool encrypt = aead_.get() != nullptr;
  return (encrypt && (version_ == kEncryptedFileVersion ||
                      version_ == kEncryptedFileVersionWithMarker)) ||
         (!encrypt &&
          (version_ == kFileVersion1 || version_ == kFileVersionWithMarker));
}

bool SessionFileReader::ReadToMarker() {
  // It's expected this is only called if the marker is supported.
  DCHECK(IsHeaderValid() && SupportsMarker());
  for (ReadResult result = ReadCommand(); result.command;
       result = ReadCommand()) {
    if (result.command->id() == kInitialStateMarkerCommandId)
      return true;
  }
  return false;
}

SessionFileReader::ReadResult SessionFileReader::ReadCommand() {
  SessionFileReader::ReadResult result;
  // Make sure there is enough in the buffer for the size of the next command.
  if (available_count_ < sizeof(size_type)) {
    if (!FillBuffer())
      return result;
    if (available_count_ < sizeof(size_type)) {
      VLOG(1) << "SessionFileReader::ReadCommand, file incomplete";
      // Still couldn't read a valid size for the command, assume write was
      // incomplete and return null.
      result.error_reading = true;
      return result;
    }
  }
  // Get the size of the command.
  size_type command_size;
  memcpy(&command_size, &(buffer_[buffer_position_]), sizeof(command_size));
  buffer_position_ += sizeof(command_size);
  available_count_ -= sizeof(command_size);

  if (command_size == 0) {
    VLOG(1) << "SessionFileReader::ReadCommand, empty command";
    // Empty command. Shouldn't happen if write was successful, fail.
    result.error_reading = true;
    return result;
  }

  // Make sure buffer has the complete contents of the command.
  if (command_size > available_count_) {
    if (command_size > buffer_.size())
      buffer_.resize((command_size / 1024 + 1) * 1024, 0);
    if (!FillBuffer() || command_size > available_count_) {
      // Again, assume the file was ok, and just the last chunk was lost.
      VLOG(1) << "SessionFileReader::ReadCommand, last chunk lost";
      result.error_reading = true;
      return result;
    }
  }
  if (aead_) {
    result.command = CreateCommandFromEncrypted(
        buffer_.c_str() + buffer_position_, command_size);
  } else {
    result.command =
        CreateCommand(buffer_.c_str() + buffer_position_, command_size);
  }
  ++command_counter_;
  buffer_position_ += command_size;
  available_count_ -= command_size;
  return result;
}

std::unique_ptr<sessions::SessionCommand>
SessionFileReader::CreateCommandFromEncrypted(const char* data,
                                              size_type length) {
  // This means the nonce overflowed and we're reusing a nonce.
  // CommandStorageBackend should never write enough commands to trigger this,
  // so assume we should stop.
  if (command_counter_ < 0)
    return nullptr;

  char nonce[kNonceLength];
  memset(nonce, 0, kNonceLength);
  memcpy(nonce, &command_counter_, sizeof(command_counter_));
  std::string plain_text;
  if (!aead_->Open(base::StringPiece(data, length),
                   base::StringPiece(nonce, kNonceLength), base::StringPiece(),
                   &plain_text)) {
    DVLOG(1) << "SessionFileReader::ReadCommand, decryption failed";
    return nullptr;
  }
  if (plain_text.size() < sizeof(id_type)) {
    DVLOG(1) << "SessionFileReader::ReadCommand, size too small";
    return nullptr;
  }
  return CreateCommand(plain_text.c_str(), plain_text.size());
}

std::unique_ptr<sessions::SessionCommand> SessionFileReader::CreateCommand(
    const char* data,
    size_type length) {
  // Callers should have checked the size.
  DCHECK_GE(length, sizeof(id_type));
  const id_type command_id = data[0];
  // NOTE: |length| includes the size of the id, which is not part of the
  // contents of the SessionCommand.
  std::unique_ptr<sessions::SessionCommand> command =
      std::make_unique<sessions::SessionCommand>(command_id,
                                                 length - sizeof(id_type));
  if (length > sizeof(id_type)) {
    memcpy(command->contents(), &(data[sizeof(id_type)]),
           length - sizeof(id_type));
  }
  return command;
}

bool SessionFileReader::FillBuffer() {
  if (available_count_ > 0 && buffer_position_ > 0) {
    // Shift buffer to beginning.
    memmove(&(buffer_[0]), &(buffer_[buffer_position_]), available_count_);
  }
  buffer_position_ = 0;
  DCHECK(buffer_position_ + available_count_ < buffer_.size());
  int to_read = static_cast<int>(buffer_.size() - available_count_);
  int read_count =
      file_->ReadAtCurrentPos(&(buffer_[available_count_]), to_read);
  if (read_count < 0) {
    // TODO(sky): communicate/log an error here.
    return false;
  }
  if (read_count == 0)
    return false;
  available_count_ += read_count;
  return true;
}

base::FilePath::StringType TimestampToString(const base::Time time) {
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  return base::NumberToString(time.ToDeltaSinceWindowsEpoch().InMicroseconds());
#elif defined(OS_WIN)
  return base::NumberToWString(
      time.ToDeltaSinceWindowsEpoch().InMicroseconds());
#endif
}

// Returns the directory the files are stored in.
base::FilePath GetSessionDirName(CommandStorageManager::SessionType type,
                                 const base::FilePath& supplied_path) {
  if (type == CommandStorageManager::kOther)
    return supplied_path.DirName();
  return supplied_path.Append(kSessionsDirectory);
}

base::FilePath::StringType GetSessionBaseName(
    CommandStorageManager::SessionType type,
    const base::FilePath& supplied_path) {
  switch (type) {
    case CommandStorageManager::kAppRestore:
      return kAppSessionFileNamePrefix;
    case CommandStorageManager::kTabRestore:
      return kTabSessionFileNamePrefix;
    case CommandStorageManager::kSessionRestore:
      return kSessionFileNamePrefix;
    case CommandStorageManager::kOther:
      return supplied_path.BaseName().value();
  }
}

base::FilePath::StringType GetSessionFilename(
    CommandStorageManager::SessionType type,
    const base::FilePath& supplied_path,
    const base::FilePath::StringType& timestamp_str) {
  return base::JoinString(
      {GetSessionBaseName(type, supplied_path), timestamp_str},
      kTimestampSeparator);
}

base::FilePath GetLegacySessionPath(CommandStorageManager::SessionType type,
                                    const base::FilePath& base_path,
                                    bool current) {
  switch (type) {
    case CommandStorageManager::kAppRestore:
      return base_path;
    case CommandStorageManager::kTabRestore:
      return base_path.Append(current ? kLegacyCurrentTabSessionFileName
                                      : kLegacyLastTabSessionFileName);
    case CommandStorageManager::kSessionRestore:
      return base_path.Append(current ? kLegacyCurrentSessionFileName
                                      : kLegacyLastSessionFileName);
    case CommandStorageManager::kOther:
      return base_path;
  }
}

}  // namespace

CommandStorageBackend::ReadCommandsResult::ReadCommandsResult() = default;
CommandStorageBackend::ReadCommandsResult::ReadCommandsResult(
    CommandStorageBackend::ReadCommandsResult&& other) = default;
CommandStorageBackend::ReadCommandsResult&
CommandStorageBackend::ReadCommandsResult::operator=(
    CommandStorageBackend::ReadCommandsResult&& other) = default;
CommandStorageBackend::ReadCommandsResult::~ReadCommandsResult() = default;

// CommandStorageBackend
// -------------------------------------------------------------

// static
const int CommandStorageBackend::kFileReadBufferSize = 1024;

// static
const SessionCommand::size_type
    CommandStorageBackend::kEncryptionOverheadInBytes = 16;

CommandStorageBackend::CommandStorageBackend(
    scoped_refptr<base::SequencedTaskRunner> owning_task_runner,
    const base::FilePath& path,
    SessionType type,
    bool use_marker,
    const std::vector<uint8_t>& decryption_key)
    : RefCountedDeleteOnSequence(owning_task_runner),
      type_(type),
      supplied_path_(path),
      use_marker_(use_marker),
      initial_decryption_key_(decryption_key),
      callback_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  // This is invoked on the main thread, don't do file access here.
}

// static
bool CommandStorageBackend::IsValidFile(const base::FilePath& path) {
  return SessionFileReader::IsHeaderValid(path, {});
}

void CommandStorageBackend::AppendCommands(
    std::vector<std::unique_ptr<SessionCommand>> commands,
    bool truncate,
    base::OnceClosure error_callback,
    const std::vector<uint8_t>& crypto_key) {
  InitIfNecessary();

  // `kInitialStateMarkerCommandId` is reserved for use by this class.
#if DCHECK_IS_ON()
  for (const auto& command : commands)
    DCHECK_NE(kInitialStateMarkerCommandId, command->id());
#endif

  // If `use_marker` is true, the consumer must call this with `truncate` set
  // to true to indicate the initial state has been supplied. To do otherwise
  // would mean the file never contains the marker, and would not be considered
  // valid. This includes first time through.
  if (use_marker_ && !truncate && (!file_ || !file_->IsValid()))
    return;

  if (truncate) {
    const bool was_encrypted = IsEncrypted();
    const bool encrypt = !crypto_key.empty();

    // The header is different when encrypting, so the file needs to be
    // recreated.
    if (use_marker_ || (was_encrypted != encrypt))
      CloseFile();

    if (encrypt) {
      aead_ = std::make_unique<crypto::Aead>(crypto::Aead::AES_256_GCM);
      crypto_key_ = crypto_key;
      aead_->Init(base::make_span(crypto_key_));
    } else {
      aead_.reset();
    }
    if (use_marker_) {
      commands.push_back(
          std::make_unique<SessionCommand>(kInitialStateMarkerCommandId, 0));
    }
  } else {
    // |crypto_key| is only used when |truncate| is true.
    DCHECK(crypto_key.empty());
  }

  // Make sure and check |file_|, if opening the file failed |file_| will be
  // null.
  if (truncate || !file_ || !file_->IsValid())
    TruncateOrOpenFile();

  // Check |file_| again as TruncateOrOpenFile() may fail.
  if (file_ && file_->IsValid() &&
      !AppendCommandsToFile(file_.get(), commands)) {
    CloseFile();
  }

  if (use_marker_ && truncate && file_ && file_->IsValid()) {
    did_write_marker_ = true;
    if (last_file_with_valid_marker_) {
      DCHECK_NE(*last_file_with_valid_marker_, current_path_);
      base::DeleteFile(*last_file_with_valid_marker_);
      last_file_with_valid_marker_.reset();
    }
    last_file_with_valid_marker_ = current_path_;
  }

  // If `file_` is null, there was an error in writing.
  if (!file_ && error_callback)
    callback_task_runner_->PostTask(FROM_HERE, std::move(error_callback));
}

// static
bool CommandStorageBackend::TimestampFromPath(const base::FilePath& path,
                                              base::Time& timestamp_result) {
  auto parts =
      base::SplitString(path.BaseName().value(), kTimestampSeparator,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() != 2u)
    return false;

  int64_t result = 0u;
  if (!base::StringToInt64(parts[1], &result))
    return false;

  timestamp_result = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(result));
  return true;
}

// static
std::set<base::FilePath> CommandStorageBackend::GetSessionFilePaths(
    const base::FilePath& path,
    CommandStorageManager::SessionType type) {
  std::set<base::FilePath> result;
  for (const auto& info : GetSessionFilesSortedByReverseTimestamp(path, type))
    result.insert(info.path);
  return result;
}

CommandStorageBackend::ReadCommandsResult
CommandStorageBackend::ReadLastSessionCommands() {
  InitIfNecessary();

  if (last_session_info_)
    return ReadCommandsFromFile(last_session_info_->path,
                                initial_decryption_key_);
  return {};
}

void CommandStorageBackend::DeleteLastSession() {
  InitIfNecessary();
  if (last_session_info_) {
    base::DeleteFile(last_session_info_->path);
    last_session_info_.reset();
  }
}

void CommandStorageBackend::MoveCurrentSessionToLastSession() {
  // TODO(sky): make this work for kOther.
  DCHECK_NE(CommandStorageManager::SessionType::kOther, type_);

  InitIfNecessary();
  CloseFile();
  DeleteLastSession();

  // Move current session to last.
  base::Optional<SessionInfo> new_last_session_info;
  if (use_marker_) {
    if (last_file_with_valid_marker_) {
      new_last_session_info =
          SessionInfo{*last_file_with_valid_marker_, timestamp_};
      last_file_with_valid_marker_.reset();
    }
  } else if (base::PathExists(current_path_)) {
    new_last_session_info = SessionInfo{current_path_, timestamp_};
  }
  last_session_info_ = new_last_session_info;

  // This ensures TruncateOrOpenFile() opens the file.
  current_path_.clear();
  TruncateOrOpenFile();
}

// static
void CommandStorageBackend::ForceAppendCommandsToFailForTesting() {
  force_append_commands_to_fail_for_testing_ = true;
}

bool CommandStorageBackend::AppendCommandsToFile(
    base::File* file,
    const std::vector<std::unique_ptr<sessions::SessionCommand>>& commands) {
  if (force_append_commands_to_fail_for_testing_) {
    force_append_commands_to_fail_for_testing_ = false;
    return false;
  }

  for (auto& command : commands) {
    if (IsEncrypted()) {
      if (!AppendEncryptedCommandToFile(file, *(command.get())))
        return false;
    } else if (!AppendCommandToFile(file, *(command.get()))) {
      return false;
    }
    commands_written_++;
  }
  file->Flush();
  return true;
}

CommandStorageBackend::~CommandStorageBackend() = default;

void CommandStorageBackend::InitIfNecessary() {
  if (inited_)
    return;

  inited_ = true;
  base::CreateDirectory(GetSessionDirName(type_, supplied_path_));

  // TODO(sky): with `use_marker_` this is expensive. See if it can be delayed.
  last_session_info_ = FindLastSessionFile();

  // Best effort delete all sessions except the current & last.
  DeleteLastSessionFiles();
}

// static
base::FilePath CommandStorageBackend::FilePathFromTime(
    const SessionType type,
    const base::FilePath& path,
    base::Time time) {
  return GetSessionDirName(type, path)
      .Append(GetSessionFilename(type, path, TimestampToString(time)));
}

// static
CommandStorageBackend::ReadCommandsResult
CommandStorageBackend::ReadCommandsFromFile(
    const base::FilePath& path,
    const std::vector<uint8_t>& crypto_key) {
  return SessionFileReader::Read(path, crypto_key);
}

void CommandStorageBackend::CloseFile() {
  file_.reset();

  // If a marker wasn't written, no need to keep the current file.
  if (use_marker_ && !did_write_marker_ && !current_path_.empty())
    base::DeleteFile(current_path_);
}

void CommandStorageBackend::TruncateOrOpenFile() {
  DCHECK(inited_);
  if (use_marker_)
    CloseFile();
  if (use_marker_ || current_path_.empty()) {
    DCHECK(!file_);
    base::Time new_timestamp = base::Time::Now();
    // Ensure we don't reuse the current file (this is extremely unlikely to
    // ever be true).
    if (new_timestamp == timestamp_)
      new_timestamp += base::TimeDelta::FromMicroseconds(1);
    if (last_session_info_) {
      // Ensure that the last session's timestamp is before the current file's.
      // This might not be true if the system clock has changed.
      if (last_session_info_->timestamp > new_timestamp) {
        new_timestamp = last_session_info_->timestamp +
                        base::TimeDelta::FromMicroseconds(1);
      }
    }
    timestamp_ = new_timestamp;
    current_path_ = FilePathFromTime(type_, supplied_path_, timestamp_);
  }
  if (file_) {
    // If `use_marker_` is true, the file is always closed before being
    // truncated.
    DCHECK(!use_marker_);
    // File is already open, truncate it. We truncate instead of closing and
    // reopening to avoid the possibility of scanners locking the file out
    // from under us once we close it. If truncation fails, we'll try to
    // recreate.
    const int header_size = static_cast<int>(sizeof(FileHeader));
    if (file_->Seek(base::File::FROM_BEGIN, header_size) != header_size ||
        !file_->SetLength(header_size))
      file_.reset();
  }
  if (!file_)
    file_ = OpenAndWriteHeader(current_path_);
  commands_written_ = 0;
  did_write_marker_ = false;
}

std::unique_ptr<base::File> CommandStorageBackend::OpenAndWriteHeader(
    const base::FilePath& path) const {
  DCHECK(!path.empty());
  std::unique_ptr<base::File> file = std::make_unique<base::File>(
      path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                base::File::FLAG_EXCLUSIVE_WRITE |
                base::File::FLAG_EXCLUSIVE_READ);
  if (!file->IsValid())
    return nullptr;
  FileHeader header;
  header.signature = kFileSignature;
  if (use_marker_) {
    header.version = IsEncrypted() ? kEncryptedFileVersionWithMarker
                                   : kFileVersionWithMarker;
  } else {
    header.version = IsEncrypted() ? kEncryptedFileVersion : kFileVersion1;
  }
  if (file->WriteAtCurrentPos(reinterpret_cast<char*>(&header),
                              sizeof(header)) != sizeof(header)) {
    return nullptr;
  }
  return file;
}

bool CommandStorageBackend::AppendCommandToFile(
    base::File* file,
    const sessions::SessionCommand& command) {
  const size_type total_size = command.GetSerializedSize();
  if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(&total_size),
                              sizeof(total_size)) != sizeof(total_size)) {
    DVLOG(1) << "error writing";
    return false;
  }
  id_type command_id = command.id();
  if (file->WriteAtCurrentPos(reinterpret_cast<char*>(&command_id),
                              sizeof(command_id)) != sizeof(command_id)) {
    DVLOG(1) << "error writing";
    return false;
  }

  const size_type content_size = total_size - sizeof(id_type);
  if (content_size == 0)
    return true;

  if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(command.contents()),
                              content_size) != content_size) {
    DVLOG(1) << "error writing";
    return false;
  }
  return true;
}

bool CommandStorageBackend::AppendEncryptedCommandToFile(
    base::File* file,
    const sessions::SessionCommand& command) {
  // This means the nonce overflowed and we're reusing a nonce. This class
  // should never write enough commands to trigger this, so assume we should
  // stop.
  if (commands_written_ < 0)
    return false;
  DCHECK(IsEncrypted());
  char nonce[kNonceLength];
  memset(nonce, 0, kNonceLength);
  memcpy(nonce, &commands_written_, sizeof(commands_written_));

  // Encryption adds overhead, resulting in a slight reduction in the available
  // space for each command. Chop any contents beyond the available size.
  const size_type command_size = std::min(
      command.size(),
      static_cast<size_type>(std::numeric_limits<size_type>::max() -
                             sizeof(id_type) - kEncryptionOverheadInBytes));
  std::vector<char> command_and_id(command_size + sizeof(id_type));
  const id_type command_id = command.id();
  memcpy(&command_and_id.front(), reinterpret_cast<const char*>(&command_id),
         sizeof(id_type));
  memcpy(&(command_and_id.front()) + sizeof(id_type), command.contents(),
         command_size);

  std::string cipher_text;
  aead_->Seal(base::StringPiece(&command_and_id.front(), command_and_id.size()),
              base::StringPiece(nonce, kNonceLength), base::StringPiece(),
              &cipher_text);
  DCHECK_LE(cipher_text.size(), std::numeric_limits<size_type>::max());
  const size_type command_and_id_size =
      static_cast<size_type>(cipher_text.size());

  int wrote = file->WriteAtCurrentPos(
      reinterpret_cast<const char*>(&command_and_id_size),
      sizeof(command_and_id_size));
  if (wrote != sizeof(command_and_id_size)) {
    DVLOG(1) << "error writing";
    return false;
  }
  wrote = file->WriteAtCurrentPos(cipher_text.c_str(), cipher_text.size());
  if (wrote != static_cast<int>(cipher_text.size())) {
    DVLOG(1) << "error writing";
    return false;
  }
  return true;
}

base::Optional<CommandStorageBackend::SessionInfo>
CommandStorageBackend::FindLastSessionFile() const {
  // Determine the session with the most recent timestamp. This is called
  // at startup, before `current_path_` is set, so no need to check it.
  DCHECK(current_path_.empty());
  for (const SessionInfo& session : GetSessionFilesSortedByReverseTimestamp()) {
    if (CanUseFileForLastSession(session.path))
      return session;
  }

  // If no last session was found, use the legacy session if present.
  // The legacy session is considered to have a timestamp of 0, before any
  // new session.
  base::FilePath legacy_session =
      GetLegacySessionPath(type_, supplied_path_, true);
  if (base::PathExists(legacy_session))
    return SessionInfo{legacy_session, base::Time()};
  return base::nullopt;
}

void CommandStorageBackend::DeleteLastSessionFiles() const {
  // Delete session files whose paths do not match the last session path. This
  // at startup, before `current_path_` is set, so no need to check it.
  DCHECK(current_path_.empty());
  for (const SessionInfo& session : GetSessionFilesSortedByReverseTimestamp()) {
    if (!last_session_info_ || session.path != last_session_info_->path)
      base::DeleteFile(session.path);
  }

  // Delete legacy session files, unless they are being used.
  const base::FilePath legacy_current_session_path =
      GetLegacySessionPath(type_, supplied_path_, true);
  if (last_session_info_ &&
      legacy_current_session_path != last_session_info_->path &&
      base::PathExists(legacy_current_session_path)) {
    base::DeleteFile(legacy_current_session_path);
  }

  // `kOther` does not differentiate between last and current.
  if (type_ != CommandStorageManager::kOther) {
    const base::FilePath legacy_last_session_path =
        GetLegacySessionPath(type_, supplied_path_, false);
    if (base::PathExists(legacy_last_session_path))
      base::DeleteFile(legacy_last_session_path);
  }
}

// static
std::vector<CommandStorageBackend::SessionInfo>
CommandStorageBackend::GetSessionFilesSortedByReverseTimestamp(
    const base::FilePath& path,
    CommandStorageManager::SessionType type) {
  std::vector<SessionInfo> sessions;
  base::FileEnumerator file_enum(
      GetSessionDirName(type, path), false, base::FileEnumerator::FILES,
      GetSessionFilename(type, path, FILE_PATH_LITERAL("*")));
  for (base::FilePath name = file_enum.Next(); !name.empty();
       name = file_enum.Next()) {
    base::Time file_time;
    if (TimestampFromPath(name, file_time))
      sessions.push_back(SessionInfo{name, file_time});
  }
  std::sort(sessions.begin(), sessions.end(), CompareSessionInfoTimestamps);
  return sessions;
}

bool CommandStorageBackend::CanUseFileForLastSession(
    const base::FilePath& path) const {
  if (!use_marker_)
    return true;

  const SessionFileReader::MarkerStatus status =
      SessionFileReader::GetMarkerStatus(path, initial_decryption_key_);
  return !status.supports_marker || status.has_marker;
}

}  // namespace sessions
