/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

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

using namespace std;

static auto creds = grpc::GoogleDefaultCredentials();
string tableName = "projects/infra-inkwell-192811/instances/instance/tables/table";
string familyName = "values";
string columnQualifier = "value";

mutex mtx;

unique_ptr<Bigtable::Stub> getBigtableStub() {
  auto channel = grpc::CreateChannel("bigtable.googleapis.com", creds);
  unique_ptr<Bigtable::Stub> bigtable_stub(
    Bigtable::NewStub(channel)
  );
  return bigtable_stub;
}

void put(string address, string value) {
  MutateRowRequest req;
  req.set_table_name(tableName);
  req.set_row_key(address);
  auto setCell = req.add_mutations()->mutable_set_cell();
  setCell->set_family_name(familyName);
  setCell->set_column_qualifier(columnQualifier);
  setCell->set_value(value);

  unique_ptr<Bigtable::Stub> bigtableStub = getBigtableStub();

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
}

void put(unsigned long long address, long long value) {
  mtx.lock();

  if (DEBUG)
    cout << "\tPut with key '" << std::hex << address << "': " << std::dec << value << endl;
  string addr = to_string(address);
  string val = to_string(value);

  put(addr, val);
  mtx.unlock();
}

string get(string address) {
  ReadRowsRequest req;
  req.set_table_name(tableName);
  req.mutable_rows()->add_row_keys(address);

  unique_ptr<Bigtable::Stub> bigtableStub = getBigtableStub();
  ReadRowsResponse resp;
  grpc::ClientContext clientContext;

  string currentValue;

  auto stream = bigtableStub->ReadRows(&clientContext, req);
  while (stream->Read(&resp)) {
    for (auto& cellChunk : *resp.mutable_chunks()) {
     if (cellChunk.value_size() > 0) {
        currentValue.reserve(cellChunk.value_size());
     }
     currentValue.append(cellChunk.value());
    }
  }
  return currentValue;
}

long long get(unsigned long long address) {
  mtx.lock();

  if (DEBUG)
    cout << "\tGet with key '" << std::hex << address << endl;
  string valueStr = get(to_string(address));

  long long value = 0;
  if (!valueStr.empty())
    value = stoll(valueStr);

  if (DEBUG) {
    cout << "\tLoaded successfully! Value " << dec << value << endl;
  }
  mtx.unlock();
  return value;
}
