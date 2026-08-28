#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "web_assets.h"
#include "webview/webview.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

namespace {

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool send_all(SOCKET socket, const char *data, std::size_t length) {
  while (length > 0) {
    const auto amount = send(socket, data, static_cast<int>(std::min<std::size_t>(length, 1U << 20)), 0);
    if (amount <= 0) {
      return false;
    }
    data += amount;
    length -= static_cast<std::size_t>(amount);
  }
  return true;
}

class AssetServer {
 public:
  AssetServer() {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
      throw std::runtime_error("Unable to initialize Windows networking.");
    }
    winsock_started_ = true;
    for (const auto &asset : kEmbeddedAssets) {
      assets_.emplace(lowercase(asset.path), &asset);
    }

    listener_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener_ == INVALID_SOCKET) {
      throw std::runtime_error("Unable to create the local asset server.");
    }
    BOOL exclusive = TRUE;
    setsockopt(listener_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
               reinterpret_cast<const char *>(&exclusive), sizeof(exclusive));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(listener_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(listener_, SOMAXCONN) == SOCKET_ERROR) {
      throw std::runtime_error("Unable to bind the local asset server.");
    }
    int address_size = sizeof(address);
    if (getsockname(listener_, reinterpret_cast<sockaddr *>(&address), &address_size) == SOCKET_ERROR) {
      throw std::runtime_error("Unable to read the local asset server address.");
    }
    port_ = ntohs(address.sin_port);
    worker_ = std::thread([this] { run(); });
  }

  AssetServer(const AssetServer &) = delete;
  AssetServer &operator=(const AssetServer &) = delete;

  ~AssetServer() {
    stopping_.store(true);
    if (listener_ != INVALID_SOCKET) {
      shutdown(listener_, SD_BOTH);
      closesocket(listener_);
      listener_ = INVALID_SOCKET;
    }
    if (worker_.joinable()) {
      worker_.join();
    }
    if (winsock_started_) {
      WSACleanup();
    }
  }

  std::string url() const {
    return "http://127.0.0.1:" + std::to_string(port_) + "/";
  }

 private:
  struct ResourceView {
    const char *data = nullptr;
    std::size_t size = 0;
  };

  static ResourceView load_resource(int resource_id) {
    const auto module = GetModuleHandleW(nullptr);
    const auto resource = FindResourceW(module, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (!resource) return {};
    const auto loaded = LoadResource(module, resource);
    if (!loaded) return {};
    const auto *bytes = static_cast<const char *>(LockResource(loaded));
    return {bytes, static_cast<std::size_t>(SizeofResource(module, resource))};
  }

  void run() {
    while (!stopping_.load()) {
      const SOCKET client = accept(listener_, nullptr, nullptr);
      if (client == INVALID_SOCKET) {
        if (stopping_.load()) break;
        continue;
      }
      serve(client);
      shutdown(client, SD_BOTH);
      closesocket(client);
    }
  }

  void serve(SOCKET client) const {
    char buffer[8192];
    const int count = recv(client, buffer, static_cast<int>(sizeof(buffer) - 1), 0);
    if (count <= 0) return;
    buffer[count] = '\0';

    std::istringstream request(std::string(buffer, static_cast<std::size_t>(count)));
    std::string method;
    std::string path;
    std::string protocol;
    request >> method >> path >> protocol;
    if (method != "GET" && method != "HEAD") {
      send_status(client, "405 Method Not Allowed", "text/plain", "Method not allowed");
      return;
    }
    const auto query = path.find_first_of("?#");
    if (query != std::string::npos) path.resize(query);
    if (path.empty() || path == "/") path = "/index.html";
    if (path.find("..") != std::string::npos || path.front() != '/') {
      send_status(client, "400 Bad Request", "text/plain", "Bad request");
      return;
    }

    const auto found = assets_.find(lowercase(path));
    if (found == assets_.end()) {
      send_status(client, "404 Not Found", "text/plain", "Not found");
      return;
    }
    const auto view = load_resource(found->second->resource_id);
    if (!view.data) {
      send_status(client, "500 Internal Server Error", "text/plain", "Resource unavailable");
      return;
    }

    std::ostringstream header;
    header << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: " << found->second->content_type << "\r\n"
           << "Content-Length: " << view.size << "\r\n"
           << "Cache-Control: public, max-age=31536000, immutable\r\n"
           << "X-Content-Type-Options: nosniff\r\n"
           << "Connection: close\r\n\r\n";
    const auto header_text = header.str();
    if (!send_all(client, header_text.data(), header_text.size())) return;
    if (method == "GET") send_all(client, view.data, view.size);
  }

  static void send_status(SOCKET client, const char *status, const char *content_type,
                          const char *body) {
    const std::size_t length = std::strlen(body);
    std::ostringstream response;
    response << "HTTP/1.1 " << status << "\r\n"
             << "Content-Type: " << content_type << "\r\n"
             << "Content-Length: " << length << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    const auto text = response.str();
    send_all(client, text.data(), text.size());
  }

  bool winsock_started_ = false;
  SOCKET listener_ = INVALID_SOCKET;
  std::uint16_t port_ = 0;
  std::atomic<bool> stopping_{false};
  std::thread worker_;
  std::unordered_map<std::string, const EmbeddedAsset *> assets_;
};

std::wstring widen(const std::string &value) {
  if (value.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                         nullptr, 0);
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length);
  return result;
}

}  // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  try {
    AssetServer server;
    webview::webview window(false, nullptr);
    window.set_title("SimTower Native");
    window.set_size(1280, 800, WEBVIEW_HINT_NONE);
    window.navigate(server.url());
    window.run();
    return 0;
  } catch (const webview::exception &error) {
    MessageBoxW(nullptr, widen(error.what()).c_str(), L"SimTower Native", MB_OK | MB_ICONERROR);
  } catch (const std::exception &error) {
    MessageBoxW(nullptr, widen(error.what()).c_str(), L"SimTower Native", MB_OK | MB_ICONERROR);
  }
  return 1;
}
