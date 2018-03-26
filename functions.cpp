#include <iostream>
#include <string>
#include <mutex>

#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <google/bigtable/v2/bigtable.grpc.pb.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using google::bigtable::v2::Bigtable;
using google::bigtable::v2::MutateRowRequest;
using google::bigtable::v2::MutateRowResponse;
using google::bigtable::v2::ReadRowsRequest;
using google::bigtable::v2::ReadRowsResponse;
using google::bigtable::v2::ReadModifyWriteRowRequest;
using google::bigtable::v2::ReadModifyWriteRule;
using google::bigtable::v2::ReadModifyWriteRowResponse;

using namespace std;

static auto creds = grpc::GoogleDefaultCredentials();
string TABLE_NAME = "projects/infra-inkwell-192811/instances/instance/tables/table";
string FAMILY_NAME = "values";
string COLUMN_QUALIFIER = "value";
string BIGTABLE_API = "bigtable.googleapis.com";

mutex mtx;
auto channel = grpc::CreateChannel(BIGTABLE_API, creds);
unique_ptr<Bigtable::Stub> bigtableStub(Bigtable::NewStub(channel));

void put(unsigned long long addr, long long val) {
  mtx.lock();
  if (DEBUG)
    cout << "\tPut with key '" << std::hex << addr << "': " << std::dec << val << endl;
  string address = to_string(addr);
  string value = to_string(val);

  // setup request
  MutateRowRequest req;
  req.set_table_name(TABLE_NAME);
  req.set_row_key(address);
  auto setCell = req.add_mutations()->mutable_set_cell();
  setCell->set_family_name(FAMILY_NAME);
  setCell->set_column_qualifier(COLUMN_QUALIFIER);
  setCell->set_value(value);

  // invoke mutate row request
  MutateRowResponse resp;
  grpc::ClientContext clientContext;
  auto status = bigtableStub->MutateRow(&clientContext, req, &resp);

  if (!status.ok()) {
    cerr << "Error in MutateRow() request: " << status.error_message()
         << " [" << status.error_code() << "] " << status.error_details()
         << endl;
  } else if (DEBUG) {
    cout << "\tStored successfully!" << endl;
  }
  mtx.unlock();
}

long long get(unsigned long long addr) {
  mtx.lock();

  if (DEBUG)
      cout << "\tGet with key '" << std::hex << addr << endl;

  // setup request
  ReadRowsRequest req;
  req.set_table_name(TABLE_NAME);
  string address = to_string(addr);
  req.mutable_rows()->add_row_keys(address);

  // invoke mutate row request
  ReadRowsResponse resp;
  grpc::ClientContext clientContext;

  string valueStr;

  auto stream = bigtableStub->ReadRows(&clientContext, req);
  while (stream->Read(&resp)) {
    for (auto& cellChunk : *resp.mutable_chunks()) {
     if (cellChunk.value_size() > 0) {
        valueStr.reserve(cellChunk.value_size());
     }
     valueStr.append(cellChunk.value());
    }
  }

  // convert value to 64-bit integer
  long long value = 0;
  if (!valueStr.empty())
      value = stoll(valueStr);

  if (DEBUG) {
      cout << "\tLoaded successfully! Value " << dec << value << endl;
  }

  mtx.unlock();
  return value;
}

long long bytesToInt(const char* bytes) {
  long long result = 0;
  for (unsigned n = 0; n < sizeof(bytes); n++)
    result = (result << 8) + bytes[n];
  return result;
}

long long atomic_increment(string address, unsigned long long increment) {
  ReadModifyWriteRowRequest req;
  req.set_table_name(TABLE_NAME);
  req.set_row_key(address);
  ReadModifyWriteRule rule;
  rule.set_family_name(FAMILY_NAME);
  rule.set_column_qualifier(COLUMN_QUALIFIER);
  rule.set_increment_amount(increment);
  *req.add_rules() = std::move(rule);

  ReadModifyWriteRowResponse resp;
  grpc::ClientContext clientContext;

  auto status = bigtableStub->ReadModifyWriteRow(&clientContext, req, &resp);
  if (!status.ok()) {
    cerr << "Error in MutateRow() request: " << status.error_message()
         << " [" << status.error_code() << "] " << status.error_details()
         << endl;
  }
  const char* bytes = resp.mutable_row()->mutable_families(0)->mutable_columns(0)->mutable_cells(0)->value().c_str();
  long long value = bytesToInt(bytes);
  if (DEBUG) {
    cout << "\tAtomic increment: " << value << endl;
  }
  return value;
}
