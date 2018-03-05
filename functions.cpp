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

void put(char address[], char value[]) {
  printf("Store instruction:\n");

  MutateRowRequest req;
  req.set_table_name(tableName);
  req.set_row_key(address);
  auto setCell = req.add_mutations()->mutable_set_cell();
  setCell->set_family_name("values");
  setCell->set_column_qualifier("value");
  setCell->set_value(value);

  unique_ptr<Bigtable::Stub> bigtableStub = getBigtableStub();

  MutateRowResponse resp;
  grpc::ClientContext clientContext;

  auto status = bigtableStub->MutateRow(&clientContext, req, &resp);
  if (!status.ok()) {
    cerr << "Error in MutateRow() request: " << status.error_message()
         << " [" << status.error_code() << "] " << status.error_details()
         << endl;
  } else {
    printf("\tPut with key '%s': %s\n", address, value);
  }
}

void put(int* address, int value) {
  char addr[16+1], val[16+1];
  sprintf(addr, "%p", address);
  sprintf(val, "%d", value);

  put(addr, val);
}

void put(int64_t* address, int64_t value) {
  char addr[16+1], val[16+1];
  sprintf(addr, "%p", address);
  sprintf(val, "%ld", value);

  put(addr, val);
}

void put(int** address, int* value) {
  char addr[16+1], val[16+1];
  sprintf(addr, "%p", address);
  sprintf(val, "%p", value);

  put(addr, val);
}

void put(int8_t** address, int8_t* value) {
  put((int**) address, (int*) value);
}

void put(int8_t** address, int32_t* value) {
  put((int**) address, value);
}

void put(int*** address, int** value) {
  char addr[16+1], val[16+1];
  sprintf(addr, "%p", address);
  sprintf(val, "%p", value);

  put(addr, val);
}

string get(char address[]) {
  printf("Load instruction:\n");

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

int get(int* address) {
  char addr[16+1];
  sprintf(addr, "%p", address);

  string valueStr = get(addr);
  int value;
  sscanf(valueStr.c_str(), "%i", &value);
  printf("\tGet with key '%p': %i\n", address, value);
  return value;
}

int64_t get(int64_t* address) {
  char addr[16+1];
  sprintf(addr, "%p", address);

  string valueStr = get(addr);
  int64_t value;
  sscanf(valueStr.c_str(), "%li", &value);
  printf("\tGet with key '%p': %li\n", address, value);
  return value;
}

int* get(int** address) {
  char addr[16+1];
  sprintf(addr, "%p", address);

  string value = get(addr);
  int* p;
  sscanf(value.c_str(), "%p", &p);
  printf("\tGet with key '%p': %p\n", address, p);
  return p;
}

int8_t* get(int8_t** address) {
  char addr[16+1];
  sprintf(addr, "%p", address);

  string value = get(addr);
  int8_t* p;
  sscanf(value.c_str(), "%p", &p);
  printf("\tGet with key '%p': %p\n", address, p);
  return p;
}

int** get(int*** address) {
  char addr[16+1];
  sprintf(addr, "%p", address);

  string value = get(addr);
  int** p;
  sscanf(value.c_str(), "%p", &p);
  printf("\tGet with key '%p': %p\n", address, p);
  return p;
}
