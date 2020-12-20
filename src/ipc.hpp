#pragma once

#include "nlohmann/json.hpp"
#include <wayland-server-core.h>
#include <wayfire/nonstd/noncopyable.hpp>
#include <sys/un.h>

namespace wf
{
class ipc_t;
/**
 * Represents a single connected client to the IPC.
 */
class ipc_client_t : public noncopyable_t
{
  public:
    ipc_client_t(ipc_t *ipc, int fd);
    ~ipc_client_t();

    /** Handle incoming data on the socket */
    void handle_data();

    void send_message(const std::string& message);

    ipc_t *ipc;

  private:
    int fd;
    wl_event_source *source;
};

/**
 * The Wayfire debugging IPC object.
 *
 * A single ipc object is instantiated by the main function, if IPC is enabled
 * at compile time.
 */
class ipc_t : public noncopyable_t
{
  public:
    ipc_t(wl_display *display, const std::string& socket_name);
    ~ipc_t();

    /** Handle new request on the main socket */
    void handle_new_client();

    /** Handle communication error */
    void handle_error(ipc_client_t *client);

    void send_message(nlohmann::json j);

  private:
    int fd;
    /**
     * Setup a socket at the given address, and set it as CLOEXEC and non-blocking.
     */
    int setup_socket(const char *address);

    sockaddr_un saddr;
    wl_event_source *source;
    std::vector<std::unique_ptr<ipc_client_t>> clients;
};
}
