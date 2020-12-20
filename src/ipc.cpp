#include "ipc.hpp"
#include "core/core-impl.hpp"
#include "wayfire/core.hpp"
#include "wayfire/debug.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <wayfire/util/log.hpp>

int wf::ipc_t::setup_socket(const char *address)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
    {
        return -1;
    }

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
    {
        return -1;
    }

    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
    {
        return -1;
    }

    // Ensure no instance left after a crash or similar
    unlink(address);

    saddr.sun_family = AF_UNIX;
    strncpy(saddr.sun_path, address, sizeof(saddr.sun_path) - 1);

    int r = bind(fd, (sockaddr*)&saddr, sizeof(saddr));
    if (r != 0)
    {
        LOGE("Failed to bind debug IPC socket at address ", address, " !");
        // TODO: shutdown socket?
        return -1;
    }

    return fd;
}

/**
 * Handle WL_EVENT_READABLE on the socket.
 * Indicates a new connection.
 */
int wl_loop_handle_ipc_fd_connection(int, uint32_t mask, void *data)
{
    auto ipc = (wf::ipc_t*)data;
    ipc->handle_new_client();
    return 0;
}

/**
 * Handle communication with client
 */
int wl_loop_handle_ipc_client_fd_event(int, uint32_t mask, void *data)
{
    auto client = (wf::ipc_client_t*)data;

    if (mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP))
    {
        client->ipc->handle_error(client);
    } else
    {
        client->handle_data();
    }

    return 0;
}

wf::ipc_t::ipc_t(wl_display *display, const std::string& display_name)
{
    std::string sockpath = "/tmp/wayfire-" + display_name + ".sock";
    setenv("WAYFIRE_SOCKET", sockpath.c_str(), 1);
    this->fd = setup_socket(sockpath.c_str());
    if (fd == -1)
    {
        LOGE("Failed to create debug IPC socket!");
    }

    listen(fd, 3);
    source = wl_event_loop_add_fd(wl_display_get_event_loop(display), fd,
        WL_EVENT_READABLE, wl_loop_handle_ipc_fd_connection, this);
}

wf::ipc_t::~ipc_t()
{
    wl_event_source_remove(source);
    close(fd);
    unlink(saddr.sun_path);
}

void wf::ipc_t::handle_new_client()
{
    // Heavily inspired by Sway
    int cfd = accept(this->fd, NULL, NULL);
    if (cfd == -1)
    {
        LOGW("Error accepting client connection");
        return;
    }

    int flags;
    if (((flags = fcntl(cfd, F_GETFD)) == -1) ||
        (fcntl(cfd, F_SETFD, flags | FD_CLOEXEC) == -1))
    {
        LOGE("Failed setting CLOEXEC");
        close(cfd);
        return;
    }

    if (((flags = fcntl(cfd, F_GETFL)) == -1) ||
        (fcntl(cfd, F_SETFL, flags | O_NONBLOCK) == -1))
    {
        LOGE("Failed setting NONBLOCK");
        close(cfd);
        return;
    }

    this->clients.push_back(std::make_unique<ipc_client_t>(this, cfd));
}

void wf::ipc_t::send_message(nlohmann::json j)
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    j["timestamp"] = ts.tv_sec * 1'000'000'000ll + ts.tv_nsec;

    std::string as_str = j.dump();
    for (auto& cl : this->clients)
    {
        cl->send_message(as_str);
    }
}

void wf::ipc_t::handle_error(wf::ipc_client_t *client)
{
    auto it = std::remove_if(clients.begin(), clients.end(),
        [&] (const auto& cl) { return cl.get() == client; });
    clients.erase(it, clients.end());
}

wf::ipc_client_t::ipc_client_t(ipc_t *ipc, int fd)
{
    LOGD("New IPC client, fd ", fd);

    this->fd  = fd;
    this->ipc = ipc;

    auto ev_loop = wf::get_core().ev_loop;
    source = wl_event_loop_add_fd(ev_loop, fd, WL_EVENT_READABLE,
        wl_loop_handle_ipc_client_fd_event, this);
}

wf::ipc_client_t::~ipc_client_t()
{
    LOGD("IPC client gone, fd ", fd);

    wl_event_source_remove(source);
    shutdown(fd, SHUT_RDWR);
    close(this->fd);
}

void wf::ipc_client_t::handle_data()
{}

void wf::ipc_client_t::send_message(const std::string& message)
{
    uint32_t len = message.length();
    write(fd, (char*)&len, 4);
    write(fd, message.data(), len);
}

void wf::publish_message(const std::string& category, nlohmann::json json)
{
    json["category"] = category;
    wf::get_core_impl().ipc->send_message(std::move(json));
}
