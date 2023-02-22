
#include "./grpc-client.h"
using namespace std;
using termcolor::reset, termcolor::yellow, termcolor::red, termcolor::blue;

GRPC_Client::GRPC_Client(std::shared_ptr<Channel> channel) : stub_(AFS::NewStub(channel)) {}

int GRPC_Client::getFileAttributes(const std::string& path, struct stat* buf, int& errornum) {
  std::cout << blue << "GRPC_Client::getFileAttributes" << reset << std::endl;

  Attributes reply;
  ClientContext context;
  Path request;
  request.set_path(path);
  Status status = stub_->getFileAttributes(&context, request, &reply);

  if (!status.ok()) {
    std::cout << red << "GRPC error: " << status.error_code() << ": " << status.error_message() << reset << std::endl;
    return -1;
  }

  if (reply.status() != 0) {
    errornum = reply.errornum();
    return -1;
  }

  // retrieve status info from grpc message
  buf->st_dev = reply.grpc_st_dev();
  buf->st_ino = reply.grpc_st_ino();
  buf->st_mode = reply.grpc_st_mode();
  buf->st_nlink = reply.grpc_st_nlink();
  buf->st_uid = reply.grpc_st_uid();
  buf->st_gid = reply.grpc_st_gid();
  buf->st_rdev = reply.grpc_st_rdev();
  buf->st_size = reply.grpc_st_size();
  buf->st_blksize = reply.grpc_st_blksize();
  buf->st_blocks = reply.grpc_st_blocks();
  buf->st_atime = reply.grpc_st_atime();
  buf->st_mtime = reply.grpc_st_mtime();
  buf->st_ctime = reply.grpc_st_ctime();

  return 0;
}

int GRPC_Client::ReadDirectory(const std::string& path, int& errornum, std::vector<std::string>& results) {
  // return cppWrapper_readdir(path, buf, filler, offset, fi);
  // const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi
  std::cout << "trigger grpc client read dir on path: " << path << std::endl;
  Path request;
  afs::ReadDirResponse response;
  ClientContext context;
  request.set_path(path);
  std::unique_ptr<ClientReader<afs::ReadDirResponse>> reader(stub_->ReadDir(&context, request));

  while (reader->Read(&response)) {
    results.push_back(response.buf());
    if (response.err() < 0) {
      break;
    }
  }

  Status status = reader->Finish();

  return status.ok() ? 0 : status.error_code();
  /*
  Status status = stub_->ReadDir(&context, request, &response);
  if (status.ok()) {
    // -----------------after receive repeated de from grpc server, store them into result_ptr
    for (int i = 0; i < response.buf_size(); ++i) {
      results.push_back(response.buf(i));
    }
    return 0;
  } else {
    errornum = -1;
    std::cout << status.error_code() << ": " << status.error_message()
              << std::endl;
    return -1;
  }
  */
}

int GRPC_Client::MakeDirectory(const std::string& path, mode_t mode, int& errornum) {
  MkDirRequest request;
  request.set_path(path);
  request.set_modet(mode);
  MkDirResponse reply;
  ClientContext context;

  Status status = stub_->MkDir(&context, request, &reply);
  if (status.ok()) {
    // std::cout << status.status() << std::endl;
    if (reply.status() != 0) {
      errornum = reply.erronum();
    }
    return reply.status();
  } else {
    errornum = -1;
    std::cout << status.error_code() << ": " << status.error_message()
              << std::endl;
    return -1;
  }
}

int GRPC_Client::RemoveDirectory(const std::string& path) {
  Path request;
  request.set_path(path);
  Response reply;
  ClientContext context;

  // Here we can use the stub's newly available method we just added.
  Status status = stub_->RmDir(&context, request, &reply);
  if (status.ok()) {
    // std::cout << status.status() << std::endl;
    return reply.status();
  } else {
    std::cout << status.error_code() << ": " << status.error_message()
              << std::endl;
    return -1;
  }
}

int GRPC_Client::Unlink(const std::string& path) {
  Path request;
  request.set_path(path);
  Response reply;
  ClientContext context;

  // Here we can use the stub's newly available method we just added.
  Status status = stub_->Unlink(&context, request, &reply);
  if (status.ok()) {
    return reply.status();
  } else {
    std::cout << status.error_code() << ": " << status.error_message()
              << std::endl;
    return -1;
  }
}

int GRPC_Client::OpenFile(const std::string& path, const int& mode, long& timestamp) {
  OpenRequest request;
  request.set_path(path);
  request.set_mode(mode);
  OpenResponse reply;
  ClientContext context;
  Status status = stub_->Open(&context, request, &reply);
  if (status.ok()) {
    timestamp = reply.timestamp();
    return reply.err();
  }
  // grpc fail
  return -1;
  // return status.error_code();
}

// touch functionality: only create file without contents
// TODO: is that the same as Open above ? Is it necessary to implement ?
int GRPC_Client::ReadFile(const std::string& path, /*const int& size,const int& offset,*/ int& numBytes, std::string& buf, long& timestamp) {
  std::cout << "trigger grpc client read on path: " << path << "\n";
  ReadRequest request;
  request.set_path(path);
  // request.set_size(size);
  // request.set_offset(offset);
  ReadReply reply;
  ClientContext context;
  std::chrono::time_point<std::chrono::system_clock> deadline =
      std::chrono::system_clock::now() + std::chrono::milliseconds(TIMEOUT);
  context.set_deadline(deadline);

  std::unique_ptr<ClientReader<ReadReply>> reader(
      stub_->Read(&context, request));

  while (reader->Read(&reply)) {
    // if (reply.buf().find("crash3") != std::string::npos) {
    //   std::cout << "Killing client process in read()\n";
    //   kill(getpid(), SIGABRT);
    // }
    std::cout << reply.buf() << std::endl;
    buf.append(reply.buf());
    if (reply.numbytes() < 0) {
      break;
    }
  }
  Status status = reader->Finish();
  if (status.ok()) {
    numBytes = reply.numbytes();
    timestamp = reply.timestamp();
    std::cout << "grpc Read client " << numBytes << " " << timestamp
              << std::endl;
    return reply.err();
  }
  std::cout << "There was an error in the server Read " << status.error_code()
            << std::endl;
  return status.error_code();
}

int GRPC_Client::WriteFile(const std::string& path, const std::string& buf, const int& size, const int& offset, int& numBytes, long& timestamp) {
  std::cout << "GRPC client write\n";
  WriteRequest request;
  WriteReply reply;
  ClientContext context;
  std::chrono::time_point<std::chrono::system_clock> deadline =
      std::chrono::system_clock::now() + std::chrono::milliseconds(TIMEOUT);
  context.set_deadline(deadline);
  std::unique_ptr<ClientWriter<WriteRequest>> writer(
      stub_->Write(&context, &reply));
  int bytesLeft = size;
  int curr = offset;
  while (bytesLeft >= 0) {
    request.set_path(path);
    request.set_buf(buf.substr(curr, std::min(CHUNK_SIZE, bytesLeft)));
    request.set_size(std::min(CHUNK_SIZE, bytesLeft));
    request.set_offset(curr);
    curr += CHUNK_SIZE;
    bytesLeft -= CHUNK_SIZE;
    if (!writer->Write(request)) {
      // Broken stream.
      break;
    }
    if (buf.find("crash4") != std::string::npos) {
      std::cout << "Killing client process in write()\n";
      kill(getpid(), SIGABRT);
    }
  }
  writer->WritesDone();
  Status status = writer->Finish();

  // std::cout << "There was an error in the server Write "
  //           << status.error_code() << std::endl;

  if (status.ok()) {
    numBytes = reply.numbytes();
    timestamp = reply.timestamp();
    return reply.err();
  }
  // cout << "There was an error in the server Write " << status.error_code()
  // << endl;

  return status.error_code();
}

/** EXAMPLE: keep it to make sure things are working
 * Assembles the client's payload, sends it and presents the response back
 * from the server.
 */
std::string GRPC_Client::SayHello(const std::string& user) {
  // Data we are sending to the server.
  HelloRequest request;
  request.set_name(user);

  // Container for the data we expect from the server.
  HelloReply reply;

  // Context for the client. It could be used to convey extra information to
  // the server and/or tweak certain RPC behaviors.
  ClientContext context;

  // The actual RPC.
  Status status = stub_->SayHello(&context, request, &reply);

  // Act upon its status.
  if (status.ok()) {
    return reply.message();
  } else {
    std::cout << status.error_code() << ": " << status.error_message()
              << std::endl;
    return "RPC failed";
  }
}
