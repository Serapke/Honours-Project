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

unique_ptr<Bigtable::Stub> getBigtableStub() {
  auto channel = grpc::CreateChannel("bigtable.googleapis.com", creds);
  unique_ptr<Bigtable::Stub> bigtable_stub(
    Bigtable::NewStub(channel)
  );
  return bigtable_stub;
}

void put(int32_t* address, int32_t value) {
  printf("Store instruction:\n");
  printf("  address = %#010x", address);
  printf("  value = %i\n", value);

  MutateRowRequest req;
  req.set_table_name(tableName);
  req.set_row_key(to_string(*address));
  auto setCell = req.add_mutations()->mutable_set_cell();
  setCell->set_family_name("values");
  setCell->set_column_qualifier("value");
  string value_to_string = to_string(value);
  setCell->mutable_value()->swap(value_to_string);

  unique_ptr<Bigtable::Stub> bigtableStub = getBigtableStub();

  MutateRowResponse resp;
  grpc::ClientContext clientContext;

  auto status = bigtableStub->MutateRow(&clientContext, req, &resp);
  if (!status.ok()) {
      cerr << "Error in MutateRow() request: " << status.error_message()
           << " [" << status.error_code() << "] " << status.error_details()
           << endl;
  }
}

void get(int* address) {
  printf("Load instruction:\n");
  printf("  address = %#010x", address);

  ReadRowsRequest req;
  req.set_table_name(tableName);
  req.mutable_rows()->add_row_keys(to_string(*address));

  unique_ptr<Bigtable::Stub> bigtableStub = getBigtableStub();

  ReadRowsResponse resp;
  grpc::ClientContext clientContext;

  string currentRowKey;
  string currentColumnFamily;
  string currentColumn;
  string currentValue;

  auto stream = bigtableStub->ReadRows(&context, request);
  while (stream->Read(&resp)) {
    for (auto& cellChunk : *resp.mutable_chunks()) {
      if (!cellChunk.row_key().empty()) {
        currentRowKey = cellChunk.row_key();
      }
      if (!cellChunk.has_family_name()) {
        currentColumnFamily = cellChunk.family_name().value();
        if (currentColumnFamily != "values") {
          throw std::runtime_error("strange, only 'values' family name expected in the query");
        }
      }
      if (!cellChunk.has_qualifier()) {
        currentColumn = cellChunk.qualifier().value();
        if (currentColumn != "value") {
          throw std::runtime_error("strange, only 'value' column expected in the query");
        }
      }
      if (cellChunk.value_size() > 0) {
        currentValue.reserve(cellChunk.value_size());
      }
      currentValue.append(cellChunk.value());
      if (cellChunk.commit_row()) {
        printf("Loaded with key '%s', column family name '%s', column '%s': %s",
          currentRowKey,
          currentColumnFamily,
          currentColumn,
          currentValue);
      }
      if (cellChunk.reset_row()) {
        currentValue.clear();
      }
    }
  }
}

void setup() {

}

int main() {
  int32_t *p;
  int32_t i = 3, j = 4;
  p = &i;
  put(p, j);
}

