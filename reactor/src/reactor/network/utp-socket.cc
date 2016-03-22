#include <reactor/network/utp-socket.hh>
#include <reactor/network/buffer.hh>
#include <reactor/thread.hh>
#include <reactor/scheduler.hh>
#include <reactor/exception.hh>
#include <reactor/network/exception.hh>
#include <elle/log.hh>
#include <utp.h>

#ifdef INFINIT_LINUX
	#include <linux/errqueue.h>
	#include <netinet/ip_icmp.h>
#endif
ELLE_LOG_COMPONENT("reactor.network.utp");

namespace reactor
{

  namespace network
  {
static uint64 on_sendto(utp_callback_arguments* args)
{
  struct sockaddr_in *sin = (struct sockaddr_in *) args->address;
  UTPServer::EndPoint ep(
    boost::asio::ip::address::from_string(inet_ntoa(sin->sin_addr)),
    ntohs(sin->sin_port));
  UTPServer* server = (UTPServer*)utp_context_get_userdata(args->context);
  ELLE_ASSERT(server);
  ELLE_DEBUG("on_sendto %s %s", args->len, ep);
  server->send_to(Buffer(args->buf, args->len), ep);
  return 0;
}

static uint64 on_read(utp_callback_arguments* args)
{
  ELLE_DEBUG("on_read");
  UTPSocket* sock = (UTPSocket*)utp_get_userdata(args->socket);
  if (!sock)
    return 0;
  sock->on_read(elle::ConstWeakBuffer(args->buf, args->len));
  return 0;
}

void UTPSocket::on_read(elle::ConstWeakBuffer const& data)
{
  _read_buffer.append(data.contents(), data.size());
  utp_read_drained(_socket);
  _read();
}

static uint64 on_firewall(utp_callback_arguments *a)
{
  return 0;
}

static uint64 on_accept(utp_callback_arguments* args)
{
  ELLE_DEBUG("on_accept");
  UTPServer* server = (UTPServer*)utp_context_get_userdata(args->context);
  server->on_accept(args->socket);
  return 0;
}

void UTPServer::on_accept(utp_socket* s)
{
  _accept_queue.emplace_back(new UTPSocket(*this, s, true));
  _accept_barrier.open();
}

static uint64 on_error(utp_callback_arguments* args)
{
  ELLE_DEBUG("on_error %s", utp_error_code_names[args->error_code]);
  UTPSocket* s = (UTPSocket*) utp_get_userdata(args->socket);
  if (!s)
    return 0;
  s->on_close();
  return 0;
}

static uint64 on_state_change(utp_callback_arguments* args)
{
  ELLE_DEBUG("on_state_change %s", utp_state_names[args->state]);
  UTPSocket* s = (UTPSocket*) utp_get_userdata(args->socket);
  if (!s)
    return 0;
  switch(args->state)
  {
    case UTP_STATE_CONNECT:
		case UTP_STATE_WRITABLE:
		  s->write_cont();
		  //s->_write_barrier.open();
		  break;
		case UTP_STATE_EOF:
		  s->on_close();
		  break;
		case UTP_STATE_DESTROYING:
		  s->destroyed();
		  break;
  }
  return 0;
}

void UTPSocket::write_cont()
{
  if (_write.size())
  {
    unsigned char* data = const_cast<unsigned char*>(_write.contents());
    int sz = _write.size();
    while (_write_pos < sz)
    {
      ssize_t len = utp_write(_socket, data + _write_pos, sz - _write_pos);
      if (!len)
      {
        ELLE_DEBUG("from status: write buffer full");
        break;
      }
      _write_pos += len;
    }
    if (_write_pos == sz)
      _write_barrier.open();
  }
}

static uint64 on_connect(utp_callback_arguments* args)
{
  ELLE_DEBUG("on_connect");
  UTPSocket* s = (UTPSocket*) utp_get_userdata(args->socket);
  if (!s)
  {
    utp_close(args->socket);
  }
  else
    s->on_connect();
  return 0;
}

void UTPSocket::on_connect()
{
  _open = true;
  _connect_barrier.open();
  _write_barrier.open();
}

static uint64 on_log(utp_callback_arguments* args)
{
  ELLE_DEBUG("utp: %s", args->buf);
  return 0;
}

std::unique_ptr<UTPSocket> UTPServer::accept()
{
  ELLE_DEBUG("accepting...");
  _accept_barrier.wait();
  ELLE_DEBUG("...accepted");
  ELLE_ASSERT(_accept_barrier.opened());
  std::unique_ptr<UTPSocket> sock(_accept_queue.back().release());
  _accept_queue.pop_back();
  if (_accept_queue.empty())
    _accept_barrier.close();
  return std::move(sock);
}

UTPServer::UTPServer()
{
  _sending = false;
  ctx = utp_init(2);
  utp_context_set_userdata(ctx, this);
  utp_set_callback(ctx, UTP_ON_FIREWALL, &on_firewall);
  utp_set_callback(ctx, UTP_ON_ACCEPT, &::reactor::network::on_accept);
  utp_set_callback(ctx, UTP_ON_ERROR, &on_error);
  utp_set_callback(ctx, UTP_ON_STATE_CHANGE, &on_state_change);
  utp_set_callback(ctx, UTP_ON_READ, &on_read);
  utp_set_callback(ctx, UTP_ON_CONNECT, &on_connect);
  utp_set_callback(ctx, UTP_SENDTO, &on_sendto);
  utp_set_callback(ctx, UTP_LOG, &on_log);
  utp_context_set_option(ctx, UTP_INITIAL_TIMEOUT, 300);
  utp_context_set_option(ctx, UTP_TIMEOUT_INCRASE_PERCENT, 150);
  utp_context_set_option(ctx, UTP_MAXIMUM_TIMEOUT, 5000);
}

UTPServer::~UTPServer()
{
  this->_cleanup();
}

void
UTPServer::_cleanup()
{
  ELLE_TRACE_SCOPE("%s: cleanup", *this);
  // Run any completed callback before deleting this.
  {
    reactor::scheduler().io_service().reset();
    reactor::scheduler().io_service().poll();
  }
  if (this->_socket)
  { // Was never initialized.
    if (this->_checker)
    {
      this->_checker->terminate();
      reactor::wait(*_checker);
    }
    if (this->_listener)
    {
      this->_listener->terminate();
      reactor::wait(*this->_listener);
    }
    this->_socket->socket()->close();
    this->_socket->close();
    this->_socket.reset(nullptr);
  }
  utp_destroy(ctx);
}

void
UTPServer::send_to(Buffer buf, EndPoint where)
{
  ELLE_DEBUG("server send_to %s %s", buf.size(), where);
  _send_buffer.emplace_back(elle::Buffer(buf.data(), buf.size()), where);
  if (!_sending)
  {
    _sending = true;
    send_cont =
      static_cast<decltype(send_cont)>(
        [this] (boost::system::error_code const& erc, size_t sz)
        {
          if (erc == boost::asio::error::operation_aborted)
            return;
          if (erc)
            ELLE_TRACE("%s: send_to error: %s", *this, erc.message());
          this->_send_buffer.pop_front();
          if (this->_send_buffer.empty())
            this->_sending = false;
          else
          {
            this->_socket->socket()->async_send_to(
              boost::asio::buffer(
                this->_send_buffer.front().first.contents(),
              this->_send_buffer.front().first.size()),
              this->_send_buffer.front().second,
              this->send_cont);
          }
        });
    this->_socket->socket()->async_send_to(
      boost::asio::buffer(
        this->_send_buffer.front().first.contents(),
        this->_send_buffer.front().first.size()),
      this->_send_buffer.front().second,
      send_cont);
  }
  else
    ELLE_DEBUG("already sending, data queued");
}

auto UTPServer::local_endpoint() -> EndPoint
{
  auto ep = _socket->local_endpoint();
  return EndPoint(ep.address(), ep.port());
}

bool UTPServer::rdv_connected() const
{
  return _socket->rdv_connected();
}

UTPSocket::UTPSocket(UTPServer& server)
: UTPSocket(server, utp_create_socket(server.ctx), false)
{
}

UTPSocket::UTPSocket(UTPServer& server, std::string const& host, int port)
: UTPSocket(server, utp_create_socket(server.ctx), false)
{
  connect(host, port);
}

UTPSocket::~UTPSocket()
{
  ELLE_DEBUG("~UTPSocket");
  on_close();
  reactor::wait(_pending_operations);
  ELLE_DEBUG("~UTPSocket finished");
}

void UTPSocket::destroyed()
{
  _read_barrier.open();
  _write_barrier.open();
  _connect_barrier.open();
  if (_socket)
    utp_set_userdata(_socket, nullptr);
  _socket = nullptr;
}

void UTPSocket::on_close()
{
  if (_closing)
    return;
  _closing = true;
  if (!_socket)
    return;
  //if (_open)
  utp_close(_socket);
  _open = false;
  _read_barrier.open();
  _write_barrier.open();
  _connect_barrier.open();
  utp_set_userdata(_socket, nullptr);
  _socket = nullptr;
}

namespace
{
  class StreamBuffer: public elle::DynamicStreamBuffer
  {
  public:
    StreamBuffer(UTPSocket* socket)
    : DynamicStreamBuffer(65536)
    , _socket(socket)
    {}
    Size read(char* buffer, Size size) override
    {
      elle::Buffer buf = _socket->read_some(size);
      memcpy(buffer, buf.contents(), buf.size());
      return buf.size();
    }
    void write(char* buffer, Size size) override
    {
      _socket->write(elle::ConstWeakBuffer(buffer, size));
    }
    UTPSocket* _socket;
  };
}

UTPSocket::UTPSocket(UTPServer& server, utp_socket* socket, bool open)
: IOStream(new StreamBuffer(this))
, _server(server)
, _socket(socket)
, _open(open)
, _closing(false)
{
  utp_set_userdata(_socket, this);
  if (open)
  {
    _write_barrier.open();
    ELLE_DEBUG("snd %s recv %s", utp_getsockopt(_socket, UTP_SNDBUF),
      utp_getsockopt(_socket, UTP_RCVBUF));
  }
}

void UTPSocket::close()
{
  on_close();
}

void UTPSocket::connect(std::string const& id,
                        std::vector<EndPoint> const& endpoints,
                        DurationOpt timeout)
{
  ELLE_TRACE("Contacting %s at %s", id, endpoints);
  EndPoint res = _server._socket->contact(id, endpoints, timeout);
  ELLE_TRACE("Got contact at %s", res);
  connect(res.address().to_string(), res.port());
}

void UTPSocket::connect(std::string const& host, int port)
{
  auto lock = _pending_operations.lock();
  struct addrinfo * ai;
  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
  getaddrinfo(host.c_str(), std::to_string(port).c_str(),
              &hints, &ai);
  utp_connect(_socket, ai->ai_addr, ai->ai_addrlen);
  freeaddrinfo(ai);
  ELLE_DEBUG("waiting for connect...");
  _connect_barrier.wait();
  ELLE_DEBUG("connected");
  if (!_open)
    throw SocketClosed();
}

void UTPSocket::write(elle::ConstWeakBuffer const& buf, DurationOpt opt)
{
  ELLE_DEBUG("write %s", buf.size());
  using namespace boost::posix_time;
  if (!_open)
    throw SocketClosed();
  auto lock = _pending_operations.lock();
  ptime start = microsec_clock::universal_time();
  Lock l(_write_mutex);
  unsigned char* data = const_cast<unsigned char*>(buf.contents());
  int sz = buf.size();
  _write = buf;
  _write_pos = 0;
  while (_write_pos < sz)
  {
    ssize_t len = utp_write(_socket, data + _write_pos, sz - _write_pos);
    if (!len)
    {
      ELLE_DEBUG("write buffer full");
      _write_barrier.close();
      Duration elapsed = microsec_clock::universal_time() - start;
      if (opt && *opt < elapsed)
        throw TimeOut();
      _write_barrier.wait(opt? elapsed - *opt: opt);
      ELLE_DEBUG("write woken up");
      if (!_open)
        throw SocketClosed();
      continue;
    }
    _write_pos += len;
  }
  _write_pos = 0;
  _write = {};
}

void UTPSocket::_read()
{
  _read_barrier.open();
}

void UTPSocket::stats()
{
  utp_socket_stats* st = utp_get_stats(_socket);
  if (st == nullptr)
    return;
  std::cerr << "recv " << st->nbytes_recv << "\nsent " << st->nbytes_xmit
  << "\nrexmit " << st->rexmit << "\nfastrexmit " << st->fastrexmit
  << "\nnxmit " << st->nxmit <<"\nnrecv" << st->nrecv
  << "\nnduprect " << st->nduprecv <<"\nmtu " << st->mtu_guess << std::endl;
}

elle::Buffer UTPSocket::read_until(std::string const& delimiter, DurationOpt opt)
{
  using namespace boost::posix_time;
  if (!_open)
    throw SocketClosed();
  auto lock = _pending_operations.lock();
  ptime start = microsec_clock::universal_time();
  while (true)
  {
    size_t p = _read_buffer.string().find(delimiter);
    if (p != std::string::npos)
    {
      return read(p + delimiter.length());
    }
    _read_barrier.close();
    Duration elapsed = microsec_clock::universal_time() - start;
    if (opt && *opt < elapsed)
      throw TimeOut();
    _read_barrier.wait(opt? elapsed - *opt: opt);
    if (!_open)
      throw SocketClosed();
  }
}

elle::Buffer UTPSocket::read(size_t sz, DurationOpt opt)
{
  using namespace boost::posix_time;
  if (!_open)
    throw SocketClosed();
  auto lock = _pending_operations.lock();
  ELLE_DEBUG("read");
  ptime start = microsec_clock::universal_time();
  while (_read_buffer.size() < sz)
  {
    ELLE_DEBUG("read wait %s", _read_buffer.size());
    _read_barrier.close();
    Duration elapsed = microsec_clock::universal_time() - start;
    if (opt && *opt < elapsed)
      throw TimeOut();
    _read_barrier.wait(opt? elapsed - *opt: opt);
    ELLE_DEBUG("read wake %s", _read_buffer.size());
    if (!_open)
      throw SocketClosed();
  }
  elle::Buffer res;
  res.size(sz);
  memcpy(res.mutable_contents(), _read_buffer.contents(), sz);
  memmove(_read_buffer.contents(), _read_buffer.contents() + sz,
          _read_buffer.size() - sz);
  _read_buffer.size(_read_buffer.size() - sz);
  return std::move(res);
}

elle::Buffer UTPSocket::read_some(size_t sz, DurationOpt opt)
{
  using namespace boost::posix_time;
  if (!_open)
    throw SocketClosed();
  auto lock = _pending_operations.lock();
  ELLE_DEBUG("read_some");
  ptime start = microsec_clock::universal_time();
  while (_read_buffer.empty())
  {
    ELLE_DEBUG("read_some wait");
    _read_barrier.close();
    Duration elapsed = microsec_clock::universal_time() - start;
    if (opt && *opt < elapsed)
      throw TimeOut();
    _read_barrier.wait(opt? elapsed - *opt: opt);
    ELLE_DEBUG("read_some wake");
    if (!_open)
      throw SocketClosed();
  }
  if (_read_buffer.size() <= sz)
  {
    elle::Buffer res;
    std::swap(res, _read_buffer);
    return std::move(res);
  }
  elle::Buffer res;
  res.size(sz);
  memcpy(res.mutable_contents(), _read_buffer.contents(), sz);
  memmove(_read_buffer.contents(), _read_buffer.contents() + sz,
          _read_buffer.size() - sz);
  _read_buffer.size(_read_buffer.size() - sz);
  return std::move(res);
}

UTPSocket::EndPoint UTPSocket::peer()
{
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  utp_getpeername(_socket, (sockaddr*)&addr, &addrlen);
  return EndPoint(boost::asio::ip::address_v4(ntohl(addr.sin_addr.s_addr)),
    ntohs(addr.sin_port));
}

void UTPServer::listen(int port)
{
  listen(EndPoint(boost::asio::ip::address(), port));
}

void UTPServer::_check_icmp()
{
  // Code comming straight from ucat libutp example.
#ifdef INFINIT_LINUX

  int fd = _socket->socket()->native_handle();
  unsigned char vec_buf[4096], ancillary_buf[4096];
  struct iovec iov = { vec_buf, sizeof(vec_buf) };
  struct sockaddr_in remote;
  struct msghdr msg;
  ssize_t len;
  struct cmsghdr *cmsg;
  struct sock_extended_err *e;
  struct sockaddr *icmp_addr;
  struct sockaddr_in *icmp_sin;

  memset(&msg, 0, sizeof(msg));

  msg.msg_name = &remote;
  msg.msg_namelen = sizeof(remote);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_flags = 0;
  msg.msg_control = ancillary_buf;
  msg.msg_controllen = sizeof(ancillary_buf);

  len = recvmsg(fd, &msg, MSG_ERRQUEUE);
  if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
  {
    ELLE_DEBUG("recvmsg error: %s", errno);
  }

  if (len < 0)
  {
    return;
  }
  for (cmsg = CMSG_FIRSTHDR(&msg);
			 cmsg;
			 cmsg = CMSG_NXTHDR(&msg, cmsg))
	{
	  ELLE_DEBUG("Handling one!");
	  if (cmsg->cmsg_type != IP_RECVERR) {
	    ELLE_DEBUG("Unhandled errqueue type: %s", cmsg->cmsg_type);
	    continue;
	  }

	  if (cmsg->cmsg_level != SOL_IP) {
	    ELLE_DEBUG("Unhandled errqueue level: %s", cmsg->cmsg_level);
	    continue;
	  }

	  ELLE_DEBUG("errqueue: IP_RECVERR, SOL_IP, len %s", cmsg->cmsg_len);

	  if (remote.sin_family != AF_INET) {
	    ELLE_DEBUG("Address family is %s, not AF_INET?  Ignoring", remote.sin_family);
	    continue;
	  }

	  ELLE_DEBUG("Remote host: %s:%s", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));

	  e = (struct sock_extended_err *) CMSG_DATA(cmsg);

	  if (!e) {
	    ELLE_DEBUG("errqueue: sock_extended_err is NULL?");
	    continue;
	  }

	  if (e->ee_origin != SO_EE_ORIGIN_ICMP) {
	    ELLE_DEBUG("errqueue: Unexpected origin: %d", e->ee_origin);
	    continue;
	  }

	  ELLE_DEBUG("    ee_errno:  %s", e->ee_errno);
	  ELLE_DEBUG("    ee_origin: %s", e->ee_origin);
	  ELLE_DEBUG("    ee_type:   %s", e->ee_type);
	  ELLE_DEBUG("    ee_code:   %s", e->ee_code);
	  ELLE_DEBUG("    ee_info:   %s", e->ee_info);	// discovered MTU for EMSGSIZE errors
	  ELLE_DEBUG("    ee_data:   %s", e->ee_data);
	  // "Node that caused the error"
	  // "Node that generated the error"
	  icmp_addr = (struct sockaddr *) SO_EE_OFFENDER(e);
	  icmp_sin = (struct sockaddr_in *) icmp_addr;

	  if (icmp_addr->sa_family != AF_INET) {
	    ELLE_DEBUG("ICMP's address family is %s, not AF_INET?", icmp_addr->sa_family);
	    continue;
	  }

	  if (icmp_sin->sin_port != 0) {
	    ELLE_DEBUG("ICMP's 'port' is not 0?");
	    continue;
	  }

	  ELLE_DEBUG("msg_flags: %s", msg.msg_flags);
	  if (false) {
	    if (msg.msg_flags & MSG_TRUNC)		fprintf(stderr, " MSG_TRUNC");
	    if (msg.msg_flags & MSG_CTRUNC)		fprintf(stderr, " MSG_CTRUNC");
	    if (msg.msg_flags & MSG_EOR)		fprintf(stderr, " MSG_EOR");
	    if (msg.msg_flags & MSG_OOB)		fprintf(stderr, " MSG_OOB");
	    if (msg.msg_flags & MSG_ERRQUEUE)	fprintf(stderr, " MSG_ERRQUEUE");
	    fprintf(stderr, "\n");
	  }

	  if (e->ee_type == 3 && e->ee_code == 4) {
	    ELLE_TRACE("ICMP type 3, code 4: Fragmentation error, discovered MTU %s", e->ee_info);
	    utp_process_icmp_fragmentation(ctx, vec_buf, len, (struct sockaddr *)&remote, sizeof(remote), e->ee_info);
	  }
	  else {
	    ELLE_TRACE("ICMP type %s, code %s", e->ee_type, e->ee_code);
	    utp_process_icmp_error(ctx, vec_buf, len, (struct sockaddr *)&remote, sizeof(remote));
	  }
	}
	#endif
}

void UTPServer::listen(EndPoint const& ep)
{
  _socket = elle::make_unique<RDVSocket>();
  _socket->close();
  _socket->bind(ep);
#ifdef INFINIT_LINUX
  int on = 1;
  /* Set the option, so we can receive errors */
  setsockopt(_socket->socket()->native_handle(), SOL_IP, IP_RECVERR,(char*)&on, sizeof(on));
#endif
  _listener.reset( new Thread("listener " + std::to_string(local_endpoint().port()), [this] {
      elle::Buffer buf;
      while (true)
      {
        buf.size(5000);
        EndPoint source;
        ELLE_DEBUG("Receive from");
        int sz = 0;
        try
        {
          if (!_socket->socket()->is_open())
          {
            ELLE_DEBUG("Socket closed, exiting");
            return;
          }
          sz = _socket->receive_from(Buffer(buf.mutable_contents(), buf.size()),
                                    source);
          buf.size(sz);
          auto* raw = source.data();
          ELLE_DEBUG("process_udp %s", sz);
          utp_process_udp(ctx, buf.contents(), sz, raw, source.size());
          utp_issue_deferred_acks(ctx);

        }
        catch (reactor::Terminate const&)
        {
          _cleanup();
          throw;
        }
        catch (std::exception const& e)
        {
          ELLE_TRACE("listener exception %s", e.what());
          // go on, this error might concern one of the many peers we deal
          // with.
        }
      }
  }));
  _checker.reset(new Thread("checker", [this] {
      try
      {
        while (true)
        {
          utp_check_timeouts(ctx);
          reactor::sleep(50_ms);
          _check_icmp();
        }
      }
      catch(std::exception const& e)
      {
        ELLE_DEBUG("exiting: %s", e.what());
        throw;
      }
  }));
}

void UTPServer::rdv_connect(std::string const& id, std::string const& address,
  DurationOpt timeout)
{
  int port = 7890;
  std::string host = address;
  auto p = host.find_first_of(':');
  if (p != host.npos)
  {
    port = std::stoi(host.substr(p+1));
    host = host.substr(0, p);
  }
  _socket->rdv_connect(id, host, port, timeout);
}

void UTPServer::set_local_id(std::string const& id)
{
  _socket->set_local_id(id);
}

}}
