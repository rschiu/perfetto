#include <vector>

#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "perfetto/base/file_utils.h"
#include "perfetto/base/scoped_file.h"
#include "perfetto/protozero/field.h"
#include "perfetto/protozero/proto_decoder.h"
#include "perfetto/protozero/proto_utils.h"
#include "third_party/pprof/profile.pb.h"

namespace perfetto {
namespace protoprofile {

namespace {

using protozero::proto_utils::ProtoWireType;
using GLine = ::perftools::profiles::Line;
using GMapping = ::perftools::profiles::Mapping;
using GLocation = ::perftools::profiles::Location;
using GProfile = ::perftools::profiles::Profile;
using GValueType = ::perftools::profiles::ValueType;
using GFunction = ::perftools::profiles::Function;
using GSample = ::perftools::profiles::Sample;
using ::google::protobuf::Descriptor;
using ::google::protobuf::DynamicMessageFactory;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::Message;
using ::google::protobuf::compiler::DiskSourceTree;
using ::google::protobuf::compiler::Importer;
using ::google::protobuf::compiler::MultiFileErrorCollector;

class MultiFileErrorCollectorImpl
    : public ::google::protobuf::compiler::MultiFileErrorCollector {
 public:
  ~MultiFileErrorCollectorImpl() override;
  void AddError(const std::string& filename,
                int line,
                int column,
                const std::string& message) override;

  void AddWarning(const std::string& filename,
                  int line,
                  int column,
                  const std::string& message) override;
};

MultiFileErrorCollectorImpl::~MultiFileErrorCollectorImpl() = default;

void MultiFileErrorCollectorImpl::AddError(const std::string& filename,
                                           int line,
                                           int column,
                                           const std::string& message) {
  PERFETTO_ELOG("Error %s %d:%d: %s", filename.c_str(), line, column,
                message.c_str());
}

void MultiFileErrorCollectorImpl::AddWarning(const std::string& filename,
                                             int line,
                                             int column,
                                             const std::string& message) {
  PERFETTO_ELOG("Error %s %d:%d: %s", filename.c_str(), line, column,
                message.c_str());
}

class SizeProfileComputer {
 public:
  GProfile Compute(const uint8_t* ptr,
                   size_t size,
                   const Descriptor* descriptor);

 private:
  struct Info {
    size_t total_size;
    size_t count;
    std::vector<int> locations;
  };

  void ComputeInner(const uint8_t* ptr,
                    size_t size,
                    const Descriptor* descriptor);
  void Sample(size_t size);
  int InternString(const std::string& str);
  int InternLocation(const std::string& str);

  std::string StackToKey();

  std::vector<std::string> stack_;
  std::map<std::string, Info> infos_;

  std::vector<std::string> strings_;
  std::map<std::string, int> string_to_id_;
  std::map<std::string, int> locations_;
};

int SizeProfileComputer::InternString(const std::string& s) {
  if (string_to_id_.count(s)) {
    return string_to_id_[s];
  }
  strings_.push_back(s);
  int id = static_cast<int>(strings_.size() - 1);
  string_to_id_[s] = id;
  return id;
}

int SizeProfileComputer::InternLocation(const std::string& s) {
  if (locations_.count(s)) {
    return locations_[s];
  }
  int id = static_cast<int>(locations_.size()) + 1;
  locations_[s] = id;
  return id;
}

std::string SizeProfileComputer::StackToKey() {
  std::string key;
  for (const std::string& part : stack_) {
    key += part;
    key += "$";
  }
  return key;
}

void SizeProfileComputer::Sample(size_t size) {
  const std::string& key = StackToKey();

  if (!infos_.count(key)) {
    Info& info = infos_[key];
    info.total_size = 0;
    info.count = 0;
    info.locations.resize(stack_.size());
    for (size_t i = 0; i < stack_.size(); i++) {
      info.locations[i] = InternLocation(stack_[i]);
    }
  }

  Info& info = infos_[key];
  info.count++;
  info.total_size += size;

  // std::string indent = "";
  // for (size_t i=0; i<stack_.size()-1; i++)
  //  indent += "  ";
  // PERFETTO_ELOG("%s %s %zu", indent.c_str(), stack_.back().c_str(), size);
}

GProfile SizeProfileComputer::Compute(const uint8_t* ptr,
                                      size_t size,
                                      const Descriptor* descriptor) {
  PERFETTO_CHECK(InternString("") == 0);
  ComputeInner(ptr, size, descriptor);
  GProfile profile;
  GValueType* sample_type = profile.add_sample_type();
  sample_type->set_type(InternString("size"));
  sample_type->set_unit(InternString("bytes"));
  sample_type = profile.add_sample_type();
  sample_type->set_type(InternString("instances"));
  sample_type->set_unit(InternString("count"));

  for (const auto& id_info : infos_) {
    const Info& info = id_info.second;

    GSample* sample = profile.add_sample();
    for (auto it = info.locations.rbegin(); it != info.locations.rend(); ++it) {
      sample->add_location_id(static_cast<uint64_t>(*it));
    }

    sample->add_value(static_cast<int64_t>(info.total_size));
    sample->add_value(static_cast<int64_t>(info.count));
  }

  for (const auto& location_id : locations_) {
    GLocation* location = profile.add_location();
    location->set_id(static_cast<uint64_t>(location_id.second));
    GLine* line = location->add_line();
    line->set_function_id(static_cast<uint64_t>(location_id.second));
  }

  for (const auto& location_id : locations_) {
    GFunction* function = profile.add_function();
    function->set_id(static_cast<uint64_t>(location_id.second));
    function->set_name(InternString(location_id.first));
  }

  for (int i = 0; i < static_cast<int>(strings_.size()); i++) {
    profile.add_string_table(strings_[static_cast<size_t>(i)]);
  }
  return profile;
}

void SizeProfileComputer::ComputeInner(const uint8_t* ptr,
                                       size_t size,
                                       const Descriptor* descriptor) {
  size_t overhead = size;
  size_t unknown = 0;
  protozero::ProtoDecoder decoder(ptr, size);

  stack_.push_back(descriptor->name());

  for (;;) {
    if (decoder.bytes_left() == 0)
      break;
    protozero::Field field = decoder.ReadField();
    if (!field.valid()) {
      PERFETTO_ELOG("Error");
      break;
    }

    int id = field.id();
    ProtoWireType type = field.type();

    overhead -= field.size_for_size_measurement();
    const FieldDescriptor* field_descriptor = descriptor->FindFieldByNumber(id);
    if (!field_descriptor) {
      unknown += field.size_for_size_measurement();
      continue;
    }

    std::string field_name = field_descriptor->name();

    bool is_message_type =
        field_descriptor->type() == FieldDescriptor::TYPE_MESSAGE;
    stack_.push_back("#" + field_name);
    if (type == ProtoWireType::kLengthDelimited && is_message_type) {
      ComputeInner(field.data(), field.size(),
                   field_descriptor->message_type());
    } else {
      stack_.push_back(field_descriptor->type_name());
      Sample(field.size_for_size_measurement());
      stack_.pop_back();
    }
    stack_.pop_back();
  }

  if (unknown) {
    stack_.push_back("#:unknown:");
    Sample(unknown);
    stack_.pop_back();
  }

  Sample(overhead);
  stack_.pop_back();
}

int Main(int argc, const char** argv) {
  if (argc != 3) {
    return 1;
  }
  const char* proto_path = argv[1];
  base::ScopedFile proto_fd = base::OpenFile(proto_path, O_RDONLY);
  if (!proto_fd) {
    return 1;
  }

  std::string s;
  base::ReadFileDescriptor(proto_fd.get(), &s);

  const Descriptor* descriptor;
  DiskSourceTree dst;
  dst.MapPath("perfetto", "protos/perfetto");
  MultiFileErrorCollectorImpl mfe;
  Importer importer(&dst, &mfe);
  const FileDescriptor* parsed_file =
      importer.Import("perfetto/trace/trace.proto");
  DynamicMessageFactory dmf;
  descriptor = parsed_file->message_type(0);

  const uint8_t* start = reinterpret_cast<const uint8_t*>(s.data());
  size_t size = s.size();

  if (!descriptor) {
    return -1;
  }

  const char* out_path = argv[2];
  base::ScopedFile out_fd =
      base::OpenFile(out_path, O_WRONLY | O_TRUNC | O_CREAT, 0600);
  if (!out_fd) {
    return 1;
  }
  SizeProfileComputer computer;
  GProfile profile = computer.Compute(start, size, descriptor);
  std::string out;
  profile.SerializeToString(&out);
  base::WriteAll(out_fd.get(), out.data(), out.size());
  base::FlushFile(out_fd.get());

  return 0;
}

}  // namespace

}  // namespace protoprofile
}  // namespace perfetto

int main(int argc, const char** argv) {
  return perfetto::protoprofile::Main(argc, argv);
}
